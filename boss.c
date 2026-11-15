#include <stdlib.h>
#include <string.h>
#include "lib/SMSlib.h"
#include "actor.h"
#include "boss.h"
#include "bullet.h"
#include "shots.h"
#include "sound.h"
#include "game.h"
#include "gen/assets.h"

#define BOSS_BAR_SEGMENTS (6)
#define BOSS_TOP (20)
#define BOSS_DYING_FRAMES (150)

unsigned char boss_active;
unsigned char boss_defeated;
unsigned int boss_hp;

static unsigned char flashing;

static struct boss_ctl {
	const boss_def *def;
	const picture_def *pic;
	int x, y;
	int next_x, next_y;
	unsigned char w, h;			/* in pixels */
	unsigned char move_delay;
	unsigned char speed;
	signed char sweep_dir;

	const boss_attack *attack;
	unsigned char attack_timer;		/* in units of 4 frames */
	unsigned char shot_delay;
	unsigned char rage;

	unsigned char frame, frame_delay, frame_dirty;
	unsigned char flash, flash_cooldown;
	unsigned char intro;			/* frames left of the entrance */
	unsigned char dying;			/* frames left of the death sequence */
	unsigned int hp_per_segment;
	unsigned char palette[16];
} boss;

void init_boss(void) {
	boss_active = 0;
	boss_defeated = 0;
	boss_hp = 0;
}

/*
 * The boss is drawn on the background layer and moved around by scrolling it,
 * so the level map has to go: the screen goes dark for a moment while the
 * boss graphics replace the level tiles.
 */
void start_boss(unsigned char boss_number, unsigned char sky) {
	boss.def = bosses + boss_number;
	boss.pic = pictures + boss.def->picture;
	boss.w = boss.pic->w << 3;
	boss.h = boss.pic->h << 3;

	SMS_displayOff();
	SMS_waitForVBlank();
	clear_tilemap();
	SMS_setBGScrollX(0);
	SMS_setBGScrollY(0);
	load_picture(boss.def->picture);
	draw_picture(boss.def->picture, 0, 0, 0);

	SMS_mapROMBank(boss.pic->bank);
	memcpy(boss.palette, boss.pic->palette, 16);
	SMS_mapROMBank(GAME_BANK);
	boss.palette[0] = sky;
	SMS_loadBGPaletteHalfBrightness(boss.palette);
	SMS_setSpritePaletteColor(0, sky);
	flashing = 1;

	boss.x = (SCREEN_W - boss.w) >> 1;
	boss.y = BOSS_TOP;
	boss.next_x = boss.x;
	boss.next_y = boss.y;
	boss.move_delay = 30;
	boss.speed = boss.def->speed;
	boss.sweep_dir = 1;
	boss.attack = boss.def->attacks;
	boss.attack_timer = boss.attack->duration;
	boss.shot_delay = 60;
	boss.rage = 0;
	boss.frame = 0;
	boss.frame_delay = boss.def->anim_delay;
	boss.frame_dirty = 0;
	boss.flash = 0;
	boss.flash_cooldown = 0;
	boss.intro = 90;
	boss.dying = 0;

	boss_hp = boss.def->hp;
	boss.hp_per_segment = boss_hp / BOSS_BAR_SEGMENTS;
	boss_active = 1;
	boss_defeated = 0;

	SMS_setBGScrollX(boss.x);
	SMS_setBGScrollY(SCROLL_H - boss.y);
	SMS_displayOn();
}

static void move_boss(void) {
	static unsigned char n;

	if (boss.move_delay) {
		boss.move_delay--;
		return;
	}

	if (boss.x != boss.next_x || boss.y != boss.next_y) {
		// Move towards the target
		for (n = boss.speed; n; n--) {
			if (boss.x < boss.next_x) {
				boss.x++;
			} else if (boss.x > boss.next_x) {
				boss.x--;
			}

			if (boss.y < boss.next_y) {
				boss.y++;
			} else if (boss.y > boss.next_y) {
				boss.y--;
			}
		}
		return;
	}

	// Select the next movement target
	switch (boss.def->move) {
	case MOVE_SWEEP:
		// From one side of the screen to the other
		boss.sweep_dir = -boss.sweep_dir;
		boss.next_x = boss.sweep_dir > 0 ? SCREEN_W - boss.w - 4 : 12;
		boss.next_y = BOSS_TOP + (rand() & 0x1F);
		boss.move_delay = 20;
		break;

	case MOVE_CHASE:
		// Tries to hover right above the player
		boss.next_x = player.x + 12 - (boss.w >> 1);
		if (boss.next_x < 8) boss.next_x = 8;
		if (boss.next_x > SCREEN_W - boss.w) boss.next_x = SCREEN_W - boss.w;
		boss.next_y = BOSS_TOP + (rand() % 24);
		boss.move_delay = 25 + (rand() & 0x0F);
		break;

	default:
		// Wanders around the upper part of the screen
		boss.next_x = 8 + rand() % (SCREEN_W - boss.w - 8);
		boss.next_y = 8 + rand() % (SCREEN_H - 72 - boss.h > 8 ? SCREEN_H - 72 - boss.h : 8);
		boss.move_delay = 30 + rand() % 20;
		break;
	}
}

static void boss_attacks(void) {
	if (boss.shot_delay) {
		boss.shot_delay--;
	} else {
		fire_pattern(boss.attack->pattern, boss.x + boss.def->muzzle_x, boss.y + boss.def->muzzle_y);
		boss.shot_delay = boss.attack->fire_delay;
	}
}

static void boss_take_hits(void) {
	static actor *sht;
	static int left, top;

	left = boss.x + boss.def->hit_x;
	top = boss.y + boss.def->hit_y;

	while ((sht = check_shots_in_box(left, top, boss.def->hit_w, boss.def->hit_h)) != 0) {
		sht->active = 0;
		if (!boss.flash_cooldown) {
			boss.flash = 2;
			boss.flash_cooldown = 8;
		}
		if (boss_hp > sht->damage) {
			boss_hp -= sht->damage;
			play_sfx(SFX_HIT);
		} else {
			boss_hp = 0;
		}
	}

	// Touching the boss hurts
	if (player.x + 16 > left && player.x + 8 < left + boss.def->hit_w &&
		player.y + 12 > top && player.y + 4 < top + boss.def->hit_h) {
		hurt_player();
	}
}

void handle_boss(void) {
	static unsigned char tick;

	if (!boss_active) return;

	if (boss.dying) {
		// Death throes: explosions all over the place, then it is gone.
		boss.dying--;
		if (!(boss.dying & 0x07)) {
			spawn_explosion(boss.x + rand() % (boss.w - 8), boss.y + rand() % (boss.h - 8));
			play_sfx(SFX_EXPLOSION);
		}
		boss.flash = boss.dying & 0x04 ? 2 : 0;
		if (!boss.dying) {
			boss_active = 0;
			boss_defeated = 1;
		}
		return;
	}

	if (boss.intro) {
		// The boss fades in; nobody can be hurt yet.
		boss.intro--;
		boss.flash = boss.intro > 45 ? 2 : (boss.intro & 0x04 ? 2 : 0);
		return;
	}

	move_boss();
	boss_attacks();
	if (boss.flash_cooldown) boss.flash_cooldown--;

	tick++;
	if (!(tick & 0x03)) {
		if (boss.attack_timer) {
			boss.attack_timer--;
		} else {
			boss.attack++;
			if (!boss.attack->duration) {
				boss.attack = boss.rage ? boss.def->rage_attacks : boss.def->attacks;
			}
			boss.attack_timer = boss.attack->duration;
			boss.shot_delay = 20;
		}
	}

	boss_take_hits();

	if (!boss_hp) {
		boss.dying = BOSS_DYING_FRAMES;
		clear_bullets();
		add_score(boss.def->score * 10);
		play_sfx(SFX_BOSS_EXPLOSION);
		return;
	}

	if (!boss.rage && boss_hp <= boss.def->rage_hp) {
		// Second phase: faster, meaner, and a different set of attacks.
		boss.rage = 1;
		boss.speed = boss.def->rage_speed;
		boss.attack = boss.def->rage_attacks;
		boss.attack_timer = boss.attack->duration;
		boss.palette[0] = 0x01;		// the sky turns blood red
		boss.flash = 4;
		play_sfx(SFX_ROAR);
	}

	// Animation
	if (boss.pic->frames > 1) {
		if (boss.frame_delay) {
			boss.frame_delay--;
		} else {
			boss.frame++;
			if (boss.frame == boss.pic->frames) boss.frame = 0;
			boss.frame_delay = boss.rage ? boss.def->anim_delay >> 1 : boss.def->anim_delay;
			boss.frame_dirty = 1;
		}
	}
}

void draw_boss(void) {
	static unsigned char n, y;
	static unsigned int level;

	if (!boss_active) return;

	// Life bar: a column of segments on the left, emptying from the top.
	level = boss.hp_per_segment * (BOSS_BAR_SEGMENTS - 1);
	for (n = BOSS_BAR_SEGMENTS, y = 40; n; n--, y += 16) {
		SMS_addSprite(10, y, boss_hp > level ? 58 : 60);
		level -= boss.hp_per_segment;
	}
}

void update_boss_vram(void) {
	if (!boss_active) {
		if (boss_defeated == 1) {
			// The boss is gone: wipe it off the background, once.
			boss_defeated = 2;
			clear_tilemap();
			SMS_setBGPaletteColor(0, 0);
			SMS_setSpritePaletteColor(0, 0);
		}
		return;
	}

	SMS_setBGScrollX(boss.x);
	SMS_setBGScrollY(SCROLL_H - boss.y);

	if (boss.flash) {
		boss.flash--;
		if (!flashing) {
			SMS_loadBGPaletteHalfBrightness(boss.palette);
			SMS_setBGPaletteColor(0, boss.palette[0]);
			flashing = 1;
		}
	} else if (flashing || boss.frame_dirty) {
		SMS_loadBGPalette(boss.palette);
		SMS_setSpritePaletteColor(0, boss.palette[0]);
		flashing = 0;
	}

	if (boss.frame_dirty) {
		boss.frame_dirty = 0;
		draw_picture(boss.def->picture, boss.frame, 0, 0);
	}
}

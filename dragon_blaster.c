#include <stdlib.h>
#include <string.h>
#include "lib/SMSlib.h"
#include "lib/PSGlib.h"
#include "actor.h"
#include "shot.h"
#include "shots.h"
#include "bullet.h"
#include "enemy.h"
#include "boss.h"
#include "map.h"
#include "score.h"
#include "sound.h"
#include "screens.h"
#include "game.h"
#include "gen/assets.h"

#define PLAYER_TOP (16)
#define PLAYER_LEFT (8)
#define PLAYER_RIGHT (256 - 24)
#define PLAYER_BOTTOM (SCREEN_H - 16)
#define PLAYER_START_X (116)
#define PLAYER_START_Y (PLAYER_BOTTOM - 16)

#define POWERUP_BASE_TILE (100)
#define POWERUP_LIGHTINING_TILE (POWERUP_BASE_TILE)
#define POWERUP_FIRE_TILE (POWERUP_BASE_TILE + 8)
#define POWERUP_WIND_TILE (POWERUP_BASE_TILE + 16)
#define POWERUP_NONE_TILE (POWERUP_BASE_TILE + 24)
#define ITEM_LIFE_TILE (204)
#define ITEM_GEM_TILE (208)

#define EXPLOSION_TILE (192)
#define HEART_TILE (90)
#define DIGIT_TILE (236)

#define FOR_EACH_POWERUP(pwr) pwr = powerups; for (char i = POWERUP_MAX; i; i--, pwr++)

/* Stage progress */
#define PHASE_LEVEL (0)
#define PHASE_WARNING (1)
#define PHASE_BOSS (2)
#define PHASE_CLEAR (3)

actor player;
actor icons[2];
actor powerups[POWERUP_MAX];

score_display score;
unsigned int hi_score;
unsigned int next_extra_life;

unsigned char lives;
unsigned char stage_number;
unsigned char game_state;
unsigned char stage_phase;
unsigned char deaths_this_stage;
unsigned int game_ticks;	/* game loops run during the current stage */

struct ply_ctl {
	char shot_delay;
	char shot_type;
	char pressed_shot_selection;

	char powerup1, powerup2;
	char powerup1_active, powerup2_active;

	unsigned char invincible;	/* frames of blinking after coming back */
	unsigned char dying;		/* frames until the dragon comes back */
} ply_ctl;

struct explosion {
	unsigned char timer;		/* 0 = free */
	unsigned char x, y;
} explosions[EXPLOSION_MAX];

/* ------------------------------------------------------------------------ */
/* Video helpers                                                              */
/* ------------------------------------------------------------------------ */

void clear_tilemap(void) {
	SMS_setNextTileatXY(0, 0);
	for (int i = (SCREEN_CHAR_W * SCROLL_CHAR_H); i; i--) {
		SMS_setTile(0);
	}
}

/* Pictures always use the background tiles from 256 on. */
void load_picture(unsigned char picture) {
	static const picture_def *pic;

	pic = pictures + picture;
	SMS_mapROMBank(pic->bank);
	SMS_loadTiles(pic->tiles, 256, pic->tiles_size);
	SMS_mapROMBank(GAME_BANK);
}

void draw_picture(unsigned char picture, unsigned char frame, unsigned char x, unsigned char y) {
	static const picture_def *pic;

	pic = pictures + picture;
	SMS_mapROMBank(pic->bank);
	SMS_loadTileMapArea(x, y, pic->tilemap + frame * (pic->w * pic->h), pic->w, pic->h);
	SMS_mapROMBank(GAME_BANK);
}

/* ------------------------------------------------------------------------ */
/* Score                                                                      */
/* ------------------------------------------------------------------------ */

void add_score(unsigned int tens) {
	static unsigned int value;

	value = score.value + tens;
	if (value < score.value) value = 0xFFFF;	// counter stop
	update_score_display(&score, value);

	if (value >= next_extra_life) {
		if (next_extra_life < 0xFFFF - EXTRA_LIFE_EVERY) {
			next_extra_life += EXTRA_LIFE_EVERY;
		} else {
			next_extra_life = 0xFFFF;
			if (value == 0xFFFF) return;
		}
		if (lives < MAX_LIVES) lives++;
		play_sfx(SFX_EXTRA_LIFE);
	}
}

/* ------------------------------------------------------------------------ */
/* Explosions                                                                 */
/* ------------------------------------------------------------------------ */

void init_explosions(void) {
	static unsigned char n;

	for (n = 0; n != EXPLOSION_MAX; n++) explosions[n].timer = 0;
}

void spawn_explosion(int x, int y) {
	static struct explosion *exp, *oldest;
	static unsigned char n;

	if (x < 0 || x > SCREEN_W - 16 || y < 0 || y > SCREEN_H - 16) return;

	// Takes a free slot, or else replaces the explosion that is nearly over.
	oldest = explosions;
	for (n = EXPLOSION_MAX, exp = explosions; n; n--, exp++) {
		if (!exp->timer) {
			oldest = exp;
			break;
		}
		if (exp->timer > oldest->timer) oldest = exp;
	}
	oldest->timer = 1;
	oldest->x = x;
	oldest->y = y;
}

void draw_explosions(void) {
	static struct explosion *exp;
	static unsigned char n, tile;

	for (n = EXPLOSION_MAX, exp = explosions; n; n--, exp++) {
		if (exp->timer) {
			tile = EXPLOSION_TILE + ((exp->timer >> 2) << 2);
			SMS_addSprite(exp->x, exp->y, tile);
			SMS_addSprite(exp->x + 8, exp->y, tile + 2);
			exp->timer++;
			if (exp->timer == 12) exp->timer = 0;
		}
	}
}

/* ------------------------------------------------------------------------ */
/* Player                                                                     */
/* ------------------------------------------------------------------------ */

void select_combined_powerup(void) {
	switch (ply_ctl.powerup1) {
	case POWERUP_LIGHTINING:
		switch (ply_ctl.powerup2) {
		case POWERUP_LIGHTINING: ply_ctl.shot_type = 3; break; // Thunderstrike
		case POWERUP_FIRE: ply_ctl.shot_type = 6; break; // Firebolt
		case POWERUP_WIND: ply_ctl.shot_type = 7; break; // Thunderstorm
		}
		break;

	case POWERUP_FIRE:
		switch (ply_ctl.powerup2) {
		case POWERUP_LIGHTINING: ply_ctl.shot_type = 6; break; // Firebolt
		case POWERUP_FIRE: ply_ctl.shot_type = 4; break; // Hellfire
		case POWERUP_WIND: ply_ctl.shot_type = 8; break; // Firestorm
		}
		break;

	case POWERUP_WIND:
		switch (ply_ctl.powerup2) {
		case POWERUP_LIGHTINING: ply_ctl.shot_type = 7; break; // Thunderstorm
		case POWERUP_FIRE: ply_ctl.shot_type = 8; break; // Firestorm
		case POWERUP_WIND: ply_ctl.shot_type = 5; break; // Tempest
		}
		break;

	}
}

void switch_powerup(void) {
	if (ply_ctl.powerup1_active && ply_ctl.powerup2_active) {
		// Only the first powerup will be active
		ply_ctl.powerup1_active = 1;
		ply_ctl.powerup2_active = 0;
		ply_ctl.shot_type = ply_ctl.powerup1 - 1;
	} else if (ply_ctl.powerup1_active) {
		// Only the second powerup will be active
		ply_ctl.powerup1_active = 0;
		ply_ctl.powerup2_active = 1;
		ply_ctl.shot_type = ply_ctl.powerup2 - 1;
	} else {
		// Both powerups will be active
		ply_ctl.powerup1_active = 1;
		ply_ctl.powerup2_active = 1;
		select_combined_powerup();
	}
}

/* A new game (or a continue) starts with a single breath weapon. */
void reset_player_powerups(void) {
	ply_ctl.shot_type = 0;
	ply_ctl.powerup1 = POWERUP_LIGHTINING;
	ply_ctl.powerup2 = 0;
	ply_ctl.powerup1_active = 1;
	ply_ctl.powerup2_active = 0;

	if (cheat_flags & CHEAT_POWER) {
		ply_ctl.powerup2 = POWERUP_FIRE;
		ply_ctl.powerup2_active = 1;
		select_combined_powerup();
	}
}

void init_player(void) {
	init_actor(&player, PLAYER_START_X, PLAYER_START_Y, 3, 1, 2, 4);
	player.animation_delay_max = 5;
	ply_ctl.shot_delay = 0;
	ply_ctl.pressed_shot_selection = 0;
	ply_ctl.invincible = 60;
	ply_ctl.dying = 0;
}

char hurt_player(void) {
#ifdef DEBUG_INVINCIBLE
	return 0;
#else
	if (ply_ctl.invincible || ply_ctl.dying || stage_phase == PHASE_CLEAR) return 0;
	if (cheat_flags & CHEAT_INVINCIBLE) return 0;

	spawn_explosion(player.x + 4, player.y);
	play_sfx(SFX_PLAYER_HIT);
	if (lives) lives--;
	deaths_this_stage++;
	ply_ctl.dying = RESPAWN_FRAMES;

	// The second breath weapon is lost.
	if (ply_ctl.powerup2 && !(cheat_flags & CHEAT_POWER)) {
		if (!ply_ctl.powerup1_active) ply_ctl.powerup1 = ply_ctl.powerup2;
		ply_ctl.powerup2 = 0;
		ply_ctl.powerup1_active = 1;
		ply_ctl.powerup2_active = 0;
		ply_ctl.shot_type = ply_ctl.powerup1 - 1;
	}

	return 1;
#endif
}

void handle_player_input(void) {
	static unsigned char joy;

	if (ply_ctl.dying) {
		ply_ctl.dying--;
		if (!ply_ctl.dying && lives) {
			// Back into the fight, blinking for a while
			player.x = PLAYER_START_X;
			player.y = PLAYER_START_Y;
			ply_ctl.invincible = INVINCIBLE_FRAMES;
		}
		return;
	}
	if (!lives) return;

	joy = SMS_getKeysStatus();

	if (joy & PORT_A_KEY_LEFT) {
		if (player.x > PLAYER_LEFT) player.x -= PLAYER_SPEED;
	} else if (joy & PORT_A_KEY_RIGHT) {
		if (player.x < PLAYER_RIGHT) player.x += PLAYER_SPEED;
	}

	if (joy & PORT_A_KEY_UP) {
		if (player.y > PLAYER_TOP) player.y -= PLAYER_SPEED;
	} else if (joy & PORT_A_KEY_DOWN) {
		if (player.y < PLAYER_BOTTOM) player.y += PLAYER_SPEED;
	}

	if (joy & PORT_A_KEY_2) {
		if (!ply_ctl.shot_delay) {
			if (fire_player_shot(&player, ply_ctl.shot_type)) {
				ply_ctl.shot_delay = player_shot_infos[ply_ctl.shot_type].firing_delay;
				play_sfx(SFX_SHOT);
			}
		}
	}

	if (joy & PORT_A_KEY_1) {
		if (!ply_ctl.pressed_shot_selection && ply_ctl.powerup2) {
			switch_powerup();
			play_sfx(SFX_SWITCH);
			ply_ctl.pressed_shot_selection = 1;
		}
	} else {
		ply_ctl.pressed_shot_selection = 0;
	}

	if (ply_ctl.shot_delay) ply_ctl.shot_delay--;
	if (ply_ctl.invincible) ply_ctl.invincible--;
}

void draw_player(void) {
	if (ply_ctl.dying || !lives) return;
	if (!(ply_ctl.invincible & 0x04)) draw_actor(&player);
}

char is_colliding_against_player(actor *act) {
	static unsigned char w;

	// The dragon's body is the middle 8 pixels of its 24x16 sprite; enemies
	// are 2 pixels narrower on each side and 16 pixels tall. Written as two
	// range checks to keep it cheap.
	w = act->pixel_w;
	if ((unsigned int) (act->x - player.x + w - 11) >= (unsigned int) (w + 3)) return 0;
	if ((unsigned int) (act->y - player.y + 9) >= 19) return 0;
	return 1;
}

/* ------------------------------------------------------------------------ */
/* Power-ups and other items                                                  */
/* ------------------------------------------------------------------------ */

unsigned char powerup_base_tile(char type) {
	switch (type) {
	case POWERUP_LIGHTINING: return POWERUP_LIGHTINING_TILE;
	case POWERUP_FIRE: return POWERUP_FIRE_TILE;
	case POWERUP_WIND: return POWERUP_WIND_TILE;
	case ITEM_LIFE: return ITEM_LIFE_TILE;
	case ITEM_GEM: return ITEM_GEM_TILE;
	}

	return POWERUP_NONE_TILE;
}

void init_powerups(void) {
	static actor *pwr;

	init_actor(icons, 256 - 32 - 8, 4, 2, 1, POWERUP_LIGHTINING_TILE, 1);
	init_actor(icons + 1, 256 - 16 - 8, 4, 2, 1, POWERUP_FIRE_TILE, 1);

	FOR_EACH_POWERUP(pwr) {
		init_actor(pwr, 0, 0, 2, 1, POWERUP_LIGHTINING_TILE, 1);
		pwr->active = 0;
	}
}

void handle_icons(void) {
	static unsigned char tile;

	tile = powerup_base_tile(ply_ctl.powerup1);
	if (!ply_ctl.powerup1_active) tile += 4;
	icons[0].base_tile = tile;

	tile = powerup_base_tile(ply_ctl.powerup2);
	if (ply_ctl.powerup2 && !ply_ctl.powerup2_active) tile += 4;
	icons[1].base_tile = tile;
}

void spawn_powerup(int x, int y, char type) {
	static actor *pwr, *selected;

	if (x < 8) x = 8;
	if (x > SCREEN_W - 24) x = SCREEN_W - 24;

	selected = 0;
	FOR_EACH_POWERUP(pwr) {
		if (!pwr->active) selected = pwr;
	}

	if (!selected) {
		// All in use: recycles the one closest to leaving the screen.
		selected = powerups;
		FOR_EACH_POWERUP(pwr) {
			if (pwr->y > selected->y) selected = pwr;
		}
	}

	selected->x = x;
	selected->y = y;
	selected->active = 1;
	selected->state = type;
	selected->base_tile = powerup_base_tile(type);
}

void collect_powerup(char type) {
	if (type == ITEM_LIFE) {
		if (lives < MAX_LIVES) lives++;
		play_sfx(SFX_EXTRA_LIFE);
		return;
	}
	if (type == ITEM_GEM) {
		add_score(GEM_SCORE);
		play_sfx(SFX_GEM);
		return;
	}

	add_score(POWERUP_SCORE);
	play_sfx(SFX_POWERUP);

	if (!ply_ctl.powerup2) {
		// Second is absent
		ply_ctl.powerup2 = type;
	} else  if (!ply_ctl.powerup1_active) {
		// First is inactive
		ply_ctl.powerup1 = type;
	} else if (!ply_ctl.powerup2_active) {
		// Second is inactive
		ply_ctl.powerup2 = type;
	} else {
		// Both are active
		ply_ctl.powerup1 = ply_ctl.powerup2;
		ply_ctl.powerup2 = type;
	}

	ply_ctl.powerup1_active = 1;
	ply_ctl.powerup2_active = 1;
	select_combined_powerup();
}

void handle_powerups(void) {
	static actor *pwr;

	FOR_EACH_POWERUP(pwr) {
		if (!pwr->active) continue;

		pwr->y++;
		if (pwr->y > SCREEN_H) {
			pwr->active = 0;
			continue;
		}

		// Check collision with player
		if (!ply_ctl.dying && lives &&
			pwr->x > player.x - 16 && pwr->x < player.x + 24 &&
			pwr->y > player.y - 16 && pwr->y < player.y + 16) {
			collect_powerup(pwr->state);
			pwr->active = 0;
		}
	}
}

void draw_powerups(void) {
	static actor *pwr;

	FOR_EACH_POWERUP(pwr) {
		draw_actor(pwr);
	}
}

/* ------------------------------------------------------------------------ */
/* Status display, made of sprites                                            */
/* ------------------------------------------------------------------------ */

void draw_hud(void) {
	static char *ch;
	static unsigned char x;

	// Lives
	SMS_addSprite(12, 4, HEART_TILE);
	SMS_addSprite(20, 4, DIGIT_TILE + (lives << 1));

	// Current breath weapons
	draw_actor(icons);
	draw_actor(icons + 1);

	// Score; the game counts in tens, so there is always a zero at the end.
	draw_score_display(&score);
	for (ch = score.characters, x = score.x; *ch; ch++) x += 8;
	SMS_addSprite(x, score.y, DIGIT_TILE);
}

/* ------------------------------------------------------------------------ */
/* One stage, from the first wave to the boss                                 */
/* ------------------------------------------------------------------------ */

void handle_pause(void) {
	if (!SMS_queryPauseRequested()) return;

	SMS_resetPauseRequest();
	pause_sound(1);
	while (!SMS_queryPauseRequested()) {
		SMS_waitForVBlank();
	}
	SMS_resetPauseRequest();
	pause_sound(0);
}

/* Returns 1 if the stage was cleared, 0 if the player ran out of lives. */
char gameplay_loop(void) {
	static const stage_def *stage;
	static const tileset_def *tileset;
	static unsigned char timer, scroll;

	stage = stages + stage_number;
	tileset = tilesets + stage->tileset;

	SMS_displayOff();
	SMS_waitForVBlank();
	clear_sprites();

	SMS_mapROMBank(sprites_tiles_bank);
	SMS_loadTiles(sprites_tiles, 0, sprites_tiles_size);
	SMS_loadSpritePalette(sprites_palette);
	SMS_setSpritePaletteColor(0, 0);

	SMS_mapROMBank(tileset->bank);
	SMS_loadTiles(tileset->tiles, 256, tileset->tiles_size);
	SMS_loadBGPalette(tileset->palette);

	// The enemy paths have their own bank, which stays mapped while playing.
	SMS_mapROMBank(GAME_BANK);

	SMS_setBGScrollX(0);
	init_map(stage->map, stage->map_bank);
	draw_map_screen();

	init_player();
	init_enemies();
	init_boss();
	init_player_shots();
	init_bullets();
	init_powerups();
	init_explosions();

	stage_phase = PHASE_LEVEL;
	deaths_this_stage = 0;
	timer = 0;

	play_music(stage->music);
	SMS_displayOn();
	game_state = STATE_PLAYING;

	game_ticks = 0;
	while (1) {
		game_ticks++;
		handle_pause();

		handle_player_input();
		handle_enemies();
		handle_boss();
		handle_icons();
		handle_powerups();
		handle_player_shots();

		SMS_initSprites();

		// Sprites drawn first win when too many share a line: threats come first.
		draw_player();
		draw_bullets();		// (moves them too)
		draw_enemies();
		draw_player_shots();
		draw_powerups();
		draw_boss();
		draw_hud();
		draw_explosions();

		SMS_finalizeSprites();
		SMS_waitForVBlank();
		UNSAFE_SMS_copySpritestoSAT();

		if (stage_phase == PHASE_LEVEL) {
			for (scroll = stage->scroll; scroll; scroll--) draw_map();
		} else {
			update_boss_vram();
		}

		switch (stage_phase) {
		case PHASE_LEVEL:
			// The level has scrolled to its end and the sky is clear: boss time.
			if (map_finished == 1 && !enemies_busy()) {
				stage_phase = PHASE_WARNING;
				timer = 100;
				stop_music();
				play_sfx(SFX_WARNING);
			}
			break;

		case PHASE_WARNING:
			timer--;
			if (!timer) {
				clear_bullets();
				start_boss(stage->boss, stage->sky);
				play_music(stage->boss_music);
				stage_phase = PHASE_BOSS;
				game_state = STATE_BOSS;
			}
			break;

		case PHASE_BOSS:
			if (boss_defeated == 2) {
				stage_phase = PHASE_CLEAR;
				timer = 200;
				play_music(MUSIC_STAGE_CLEAR);
			}
			break;

		case PHASE_CLEAR:
			timer--;
			if (!timer) return 1;
			break;
		}

		// Out of lives: let the explosion finish, then it is over.
		if (!lives && !ply_ctl.dying) {
			stop_music();
			wait_frames(60);
			return 0;
		}
	}
}

/* ------------------------------------------------------------------------ */

void new_game(void) {
	lives = (cheat_flags & CHEAT_LIVES) ? MAX_LIVES : START_LIVES;
	next_extra_life = EXTRA_LIFE_EVERY;
	init_score_display(&score, 12, 20, DIGIT_TILE);
	reset_player_powerups();
}

void play_game(void) {
	static unsigned int stage_bonus, no_miss_bonus;

	new_game();
#ifdef DEBUG_START_STAGE
	stage_number = DEBUG_START_STAGE;
#else
	stage_number = start_stage;
#endif

	while (1) {
		stage_card_screen();

		if (!gameplay_loop()) {
			if (score.value > hi_score) hi_score = score.value;
			if (!game_over_screen()) return;
			// Continue: same stage, but the score starts again.
			new_game();
			continue;
		}

		stage_bonus = stages[stage_number].bonus * 10;
		no_miss_bonus = deaths_this_stage ? 0 : NO_MISS_BONUS;
		add_score(stage_bonus);
		add_score(no_miss_bonus);
		stage_clear_screen(stage_bonus, no_miss_bonus);

		stage_number++;
		if (stage_number == STAGE_COUNT) {
			game_state = STATE_ENDING;
			story_screen(text_ending, text_ending_bank, MUSIC_ENDING);
			the_end_screen();
			if (score.value > hi_score) hi_score = score.value;
			return;
		}
	}
}

void main(void) {
	SMS_useFirstHalfTilesforSprites(1);
	SMS_setSpriteMode(SPRITEMODE_TALL);
	SMS_VDPturnOnFeature(VDPFEATURE_HIDEFIRSTCOL);

	hi_score = 0;
	cheat_flags = 0;
	start_stage = 0;
	init_sound();

	while (1) {
		title_screen();
		story_screen(text_intro, text_intro_bank, MUSIC_INTRO);
		play_game();
	}
}

SMS_EMBED_SEGA_ROM_HEADER(9999,0); // code 9999 hopefully free, here this means 'homebrew'
SMS_EMBED_SDSC_HEADER(1,0, 2026,11,14, "Haroldo-OK\\2022-2026", "Dragon Blaster",
  "A dragon-themed shoot-em-up: five stages, five bosses and nine breath weapons.\n"
  "Originally made for the SHMUP JAM 1 - Dragons - https://itch.io/jam/shmup-jam-1-dragons\n"
  "Enhanced for SMS Power! Competition 2022 - https://www.smspower.org/forums/18879-Competitions2022DeadlineIs27thMarch\n"
  "Further Enhanced for Improve My Game Jam 22 - https://itch.io/jam/improve-my-game-jam-22\n"
  "Built using devkitSMS & SMSlib - https://github.com/sverx/devkitSMS");

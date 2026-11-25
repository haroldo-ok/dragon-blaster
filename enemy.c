#include <stdlib.h>
#include "lib/SMSlib.h"
#include "actor.h"
#include "enemy.h"
#include "bullet.h"
#include "shots.h"
#include "sound.h"
#include "game.h"
#include "gen/assets.h"

#define FOR_EACH_ENEMY(enm) enm = enemies; for (char i = ENEMY_MAX; i; i--, enm++)

struct spawner {
	const wave_def *wave;		/* 0 = free slot */
	unsigned char x;
	unsigned char flags;
	unsigned char delay;
	unsigned char remaining;	/* enemies still to enter */
	unsigned char alive;		/* enemies of this wave on screen */
	unsigned char escaped;		/* set once one of them got away */
};
typedef struct spawner spawner;

enemy enemies[ENEMY_MAX];
unsigned char enemies_alive;

static spawner spawners[SPAWNER_MAX];

/* Waves that showed up while every spawner was busy, oldest first. */
#define PENDING_MAX (4)
#define NO_WAVE (0xFF)
static unsigned char pending_wave[PENDING_MAX];
static unsigned char pending_x[PENDING_MAX];
static unsigned char perfect_waves;

void init_enemies(void) {
	static enemy *enm;
	static unsigned char n;

	FOR_EACH_ENEMY(enm) {
		enm->act.active = 0;
	}
	for (n = 0; n != SPAWNER_MAX; n++) {
		spawners[n].wave = 0;
	}
	for (n = 0; n != PENDING_MAX; n++) {
		pending_wave[n] = NO_WAVE;
	}
	enemies_alive = 0;
	perfect_waves = 0;
}

static char try_start_wave(unsigned char x, unsigned char wave_number) {
	static spawner *spw;
	static unsigned char n;

	for (n = SPAWNER_MAX, spw = spawners; n; n--, spw++) {
		if (!spw->wave) {
			spw->wave = waves + wave_number;
			spw->x = x;
			spw->flags = x > 124 ? PATH_FLIP_X : 0;
			spw->delay = 0;
			spw->remaining = spw->wave->count;
			spw->alive = 0;
			spw->escaped = 0;
			return 1;
		}
	}
	return 0;
}

void start_wave(unsigned char x, unsigned char wave_number) {
	static unsigned char n;

	if (wave_number >= WAVE_COUNT || !waves[wave_number].count) return;
	if (try_start_wave(x, wave_number)) return;

	// Every spawner is busy: the wave waits for one to be free. Only when the
	// waiting line is full too is the wave skipped.
	for (n = 0; n != PENDING_MAX; n++) {
		if (pending_wave[n] == NO_WAVE) {
			pending_wave[n] = wave_number;
			pending_x[n] = x;
			return;
		}
	}
}

char enemies_busy(void) {
	static unsigned char n;

	if (enemies_alive) return 1;
	for (n = 0; n != SPAWNER_MAX; n++) {
		if (spawners[n].wave) return 1;
	}
	for (n = 0; n != PENDING_MAX; n++) {
		if (pending_wave[n] != NO_WAVE) return 1;
	}
	return 0;
}

/* Returns 0 when every enemy slot is taken; the spawner then tries again later. */
static char spawn_enemy(spawner *spw) {
	static enemy *enm;
	static const enemy_type *type;

	FOR_EACH_ENEMY(enm) {
		if (!enm->act.active) {
			type = enemy_types + spw->wave->enemy;
			init_actor(&enm->act, spw->x, -15, type->char_w, 1, type->base_tile, type->frame_count);
			enm->act.animation_delay_max = type->anim_delay;
			enm->act.path_flags = spw->flags;
			enm->act.path = spw->wave->path;
			enm->type = type;
			enm->hp = type->hp;
			enm->fire_timer = (type->fire_delay >> 1) + (rand() & 0x1F);
			enm->flash = 0;
			enm->spawner = spw;
			enm->age = 0;
			enm->vx = 0;
			enm->vy = 0;
			enm->fx = 8;
			enm->fy = 8;
			spw->alive++;
			return 1;
		}
	}
	return 0;
}

static void handle_spawners(void) {
	static spawner *spw;
	static unsigned char n;

	for (n = 0, spw = spawners; n != SPAWNER_MAX; n++, spw++) {
		if (!spw->wave) continue;

		if (spw->remaining) {
			if (spw->delay) {
				spw->delay--;
			} else if (spawn_enemy(spw)) {
				spw->remaining--;
				spw->delay = spw->wave->spacing;
			} else {
				spw->delay = 4;		// no free slot: try again in a moment
			}
		} else if (!spw->alive) {
			spw->wave = 0;
		}
	}

	// A waiting wave takes the first free spawner.
	if (pending_wave[0] != NO_WAVE && try_start_wave(pending_x[0], pending_wave[0])) {
		for (n = 0; n != PENDING_MAX - 1; n++) {
			pending_wave[n] = pending_wave[n + 1];
			pending_x[n] = pending_x[n + 1];
		}
		pending_wave[PENDING_MAX - 1] = NO_WAVE;
	}
}

/* Called when the last enemy of a wave is shot down. */
static void reward_wave(spawner *spw, int x, int y) {
	if (spw->remaining || spw->alive || spw->escaped) return;

	add_score(WAVE_BONUS_SCORE);
	perfect_waves++;
	if (perfect_waves == POWERUP_EVERY_WAVES) {
		perfect_waves = 0;
		spawn_powerup(x, y, 1 + (rand() % 3));
	} else {
		spawn_powerup(x, y, ITEM_GEM);
	}
}

/*
 * Homing movement: the enemy accelerates towards the dragon on each axis, up
 * to its top speed, so it overshoots and swings around instead of turning on
 * a dime. Once its chase time is over it keeps its last speed and leaves.
 */
static void move_homing_enemy(enemy *enm, const enemy_type *type) {
	static actor *act;
	static int target, speed, pos;

	act = &enm->act;
	if (enm->age != 0xFFFF) enm->age++;

	if (enm->age <= type->lock) {
		// Still entering: follows the wave's path, remembering how fast it went.
		target = act->x;
		pos = act->y;
		move_actor(act);
		enm->vx = (act->x - target) << 4;
		enm->vy = (act->y - pos) << 4;
		return;
	}

	if (enm->age <= type->lock + type->chase) {
		speed = type->max_speed;

		// Aims for the middle of the dragon's body.
		target = player.x + 12 - (act->pixel_w >> 1);
		if (act->x < target) {
			enm->vx += type->turn;
			if (enm->vx > speed) enm->vx = speed;
		} else if (act->x > target) {
			enm->vx -= type->turn;
			if (enm->vx < -speed) enm->vx = -speed;
		}

		target = player.y;
		if (act->y < target) {
			enm->vy += type->turn;
			if (enm->vy > speed) enm->vy = speed;
		} else if (act->y > target) {
			enm->vy -= type->turn;
			if (enm->vy < -speed) enm->vy = -speed;
		}
	} else if (enm->vy < 16 && enm->vy > -16 && enm->vx < 16 && enm->vx > -16) {
		// Gave up while nearly still: drop away instead of hanging around.
		enm->vy = 32;
	}

	// Position in 1/16 pixels; >> on a signed int keeps the sign with SDCC.
	pos = (act->x << 4) + enm->fx + enm->vx;
	act->x = pos >> 4;
	enm->fx = pos & 0x0F;
	pos = (act->y << 4) + enm->fy + enm->vy;
	act->y = pos >> 4;
	enm->fy = pos & 0x0F;
}

void handle_enemies(void) {
	static enemy *enm;
	static actor *act, *sht;
	static const enemy_type *type;
	static spawner *spw;
	static int x, y;
	static unsigned char parity;

	handle_spawners();

	/*
	 * Each enemy checks for hits every other frame, half of them on even
	 * frames and half on odd ones. A shot moves at most 8 pixels a frame and
	 * the box where it hits is 32 pixels tall, so no hit is missed.
	 */
	parity ^= 1;

	enemies_alive = 0;
	FOR_EACH_ENEMY(enm) {
		act = &enm->act;
		if (!act->active) continue;

		type = enm->type;
		if (type->chase) {
			move_homing_enemy(enm, type);
		} else {
			move_actor(act);
		}

		spw = enm->spawner;
		x = act->x;
		y = act->y;

		// Left the screen? (-32 <= x <= 287 and -16 <= y <= 192, as two
		// unsigned comparisons)
		if ((unsigned int) (x + 32) > 319 || (unsigned int) (y + 16) > 208) {
			act->active = 0;
			spw->alive--;
			spw->escaped = 1;
			continue;
		}

		if (enm->flash) enm->flash--;

		if ((i ^ parity) & 1) goto checked;

		// (things hanging over the left or top edges cannot be hit)
		if (shot_count && !(x & 0xFF00) && y >= -8 && y < 192) {
			sht = check_shots_in_box(x, y, act->pixel_w, 16);
			if (sht) {
				sht->active = 0;
				if (enm->hp > sht->damage) {
					enm->hp -= sht->damage;
					enm->flash = 6;
					play_sfx(SFX_HIT);
				} else {
					act->active = 0;
					spw->alive--;
					add_score(type->score);
					spawn_explosion(x + (act->pixel_w >> 1) - 8, y);
					play_sfx(SFX_EXPLOSION);
					reward_wave(spw, x, y);
					continue;
				}
			}
		}

		// Rams the dragon? Most enemies are far away vertically, so that is
		// checked first, here, before calling the full test.
		if ((unsigned int) (y - player.y + 9) < 19 &&
				!(type->flags & ENEMY_FLAG_GROUND) && is_colliding_against_player(act)) {
			if (hurt_player() && !(type->flags & ENEMY_FLAG_TOUGH)) {
				act->active = 0;
				spw->alive--;
				spw->escaped = 1;
				spawn_explosion(x, y);
				continue;
			}
		}

checked:
		if (type->pattern) {
			if (enm->fire_timer) {
				enm->fire_timer--;
			} else {
				// Enemies that already flew past the player hold their fire.
				if (y < 136 && y >= 0) {
					fire_pattern(type->pattern, x + (act->pixel_w >> 1), y + 16);
				}
				enm->fire_timer = type->fire_delay;
			}
		}

		enemies_alive++;
	}
}

void draw_enemies(void) {
	static enemy *enm;

	FOR_EACH_ENEMY(enm) {
		// Enemies that were just hit blink.
		if (!(enm->flash & 0x02)) draw_actor(&enm->act);
	}
}

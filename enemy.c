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
	enemies_alive = 0;
	perfect_waves = 0;
}

void start_wave(unsigned char x, unsigned char wave_number) {
	static spawner *spw;
	static unsigned char n;

	if (wave_number >= WAVE_COUNT || !waves[wave_number].count) return;

	for (n = SPAWNER_MAX, spw = spawners; n; n--, spw++) {
		if (!spw->wave) {
			spw->wave = waves + wave_number;
			spw->x = x;
			spw->flags = x > 124 ? PATH_FLIP_X : 0;
			spw->delay = 0;
			spw->remaining = spw->wave->count;
			spw->alive = 0;
			spw->escaped = 0;
			return;
		}
	}
	// Every spawner is busy: the wave is skipped.
}

char enemies_busy(void) {
	static unsigned char n;

	if (enemies_alive) return 1;
	for (n = 0; n != SPAWNER_MAX; n++) {
		if (spawners[n].wave) return 1;
	}
	return 0;
}

static void spawn_enemy(spawner *spw) {
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
			spw->alive++;
			return;
		}
	}
	// No free slot: this enemy never shows up, which also voids the wave bonus.
	spw->escaped = 1;
}

static void handle_spawners(void) {
	static spawner *spw;
	static unsigned char n;

	for (n = 0, spw = spawners; n != SPAWNER_MAX; n++, spw++) {
		if (!spw->wave) continue;

		if (spw->remaining) {
			if (spw->delay) {
				spw->delay--;
			} else {
				spawn_enemy(spw);
				spw->remaining--;
				spw->delay = spw->wave->spacing;
			}
		} else if (!spw->alive) {
			spw->wave = 0;
		}
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

void handle_enemies(void) {
	static enemy *enm;
	static actor *act, *sht;
	static const enemy_type *type;
	static spawner *spw;
	static int x, y;

	handle_spawners();

	enemies_alive = 0;
	FOR_EACH_ENEMY(enm) {
		act = &enm->act;
		if (!act->active) continue;

		move_actor(act);
		spw = enm->spawner;
		x = act->x;
		y = act->y;

		if (x < -32 || x > 287 || y < -16 || y > 192) {
			act->active = 0;
			spw->alive--;
			spw->escaped = 1;
			continue;
		}

		type = enm->type;
		if (enm->flash) enm->flash--;

		if (shot_count) {
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

		if (!(type->flags & ENEMY_FLAG_GROUND) && is_colliding_against_player(act)) {
			if (hurt_player() && !(type->flags & ENEMY_FLAG_TOUGH)) {
				act->active = 0;
				spw->alive--;
				spw->escaped = 1;
				spawn_explosion(x, y);
				continue;
			}
		}

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

#ifndef DEFS_H
#define DEFS_H

#include "actor.h"

/*
 * Data structures filled in by tool/build_assets.py from data/game.ini
 * (see gen/tables.c after a build).
 */

typedef struct picture_def {
	const unsigned char *tiles;
	unsigned int tiles_size;
	const unsigned int *tilemap;	/* frames * w * h entries */
	const unsigned char *palette;	/* 16 colours */
	unsigned char bank;
	unsigned char w, h, frames;	/* size in 8x8 tiles */
	unsigned int tile_count;	/* the first PICTURE_MAIN_TILES go to tile 256 on, the rest
					   to the boss_extra_tiles ranges (boss pictures only) */
} picture_def;

typedef struct tileset_def {
	const unsigned char *tiles;
	unsigned int tiles_size;
	const unsigned char *palette;
	unsigned char bank;
} tileset_def;

typedef struct music_def {
	const unsigned char *data;
	unsigned char bank;
	unsigned char loop;
} music_def;

typedef struct sfx_def {
	const unsigned char *data;
	unsigned char channels;
	unsigned char priority;
} sfx_def;

#define ENEMY_FLAG_GROUND (0x01)	/* part of the scenery: cannot be rammed */
#define ENEMY_FLAG_TOUGH (0x02)		/* does not die when it rams the player */

typedef struct enemy_type {
	unsigned char base_tile, char_w, frame_count, anim_delay;
	unsigned char hp, score;
	unsigned char pattern, fire_delay;
	unsigned char flags;
} enemy_type;

typedef struct wave_def {
	unsigned char enemy, count, spacing;
	const path_step *path;
} wave_def;

typedef struct boss_attack {
	unsigned char pattern, fire_delay, duration;	/* duration in units of 4 frames */
} boss_attack;

#define BOSS_ANIM_LOOP (0)		/* 0 1 2 0 1 2 ... */
#define BOSS_ANIM_PINGPONG (1)		/* 0 1 2 1 0 1 ... */

typedef struct boss_def {
	const char *name;
	unsigned char picture;
	unsigned int hp, rage_hp;
	unsigned char hit_x, hit_y, hit_w, hit_h;
	unsigned char muzzle_x, muzzle_y;
	unsigned char anim_delay;
	unsigned char anim_mode;			/* BOSS_ANIM_LOOP or BOSS_ANIM_PINGPONG */
	unsigned char move;
	unsigned char speed, rage_speed;
	const boss_attack *attacks, *rage_attacks;
	unsigned char score;				/* in hundreds of tens: 50 = 5,000 points */
} boss_def;

typedef struct stage_def {
	const char *name;
	unsigned char tileset;
	const unsigned char *map;
	unsigned char map_bank;
	unsigned char music, boss, boss_music;
	unsigned char sky;				/* background colour during the boss fight */
	unsigned char scroll;				/* pixels per frame */
	unsigned char bonus;				/* in hundreds of tens */
} stage_def;

#endif /* DEFS_H */

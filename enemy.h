#ifndef ENEMY_H
#define ENEMY_H

#include "actor.h"
#include "config.h"
#include "defs.h"

struct spawner;

typedef struct enemy {
	actor act;
	const enemy_type *type;
	unsigned char hp;
	unsigned char fire_timer;
	unsigned char flash;
	struct spawner *spawner;	/* the wave it belongs to */
} enemy;

extern enemy enemies[ENEMY_MAX];
extern unsigned char enemies_alive;

void init_enemies(void);
void handle_enemies(void);
void draw_enemies(void);

/* Starts one of the waves described in data/game.ini at horizontal position x. */
void start_wave(unsigned char x, unsigned char wave_number);

/* 1 while some wave is still entering or on screen. */
char enemies_busy(void);

#endif /* ENEMY_H */

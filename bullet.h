#ifndef BULLET_H
#define BULLET_H

#include "config.h"

/* Enemy bullets: positions and speeds are 8.8 fixed point. */
typedef struct bullet {
	unsigned char active;		/* 0 = free, otherwise the BULLET_xxx look */
	unsigned int x, y;
	int vx, vy;
	unsigned char timer;
} bullet;

#define BULLET_ORB (1)
#define BULLET_NEEDLE (2)
#define BULLET_SHARD (3)
#define BULLET_FIRE (4)

extern bullet bullets[BULLET_MAX];

void init_bullets(void);
void draw_bullets(void);		/* moves the bullets too */
void clear_bullets(void);

/* Fires one of the PATTERN_xxx attacks from the given point. */
void fire_pattern(unsigned char pattern, int x, int y);

#endif /* BULLET_H */

#ifndef GAME_H
#define GAME_H

#include "actor.h"
#include "config.h"
#include "score.h"

/* Things shared between the gameplay modules. */

#define POWERUP_LIGHTINING (1)
#define POWERUP_FIRE (2)
#define POWERUP_WIND (3)
#define ITEM_LIFE (4)
#define ITEM_GEM (5)

extern actor player;
extern unsigned char lives;
extern unsigned char stage_number;
extern score_display score;
extern unsigned int hi_score;
extern unsigned char game_state;

/*
 * Cheats, switched on with secret codes on the title screen (see screens.c).
 * They stay on until the console is reset.
 */
#define CHEAT_LIVES (0x01)		/* start with 9 lives */
#define CHEAT_INVINCIBLE (0x02)		/* nothing hurts the dragon */
#define CHEAT_STAGE_SELECT (0x04)	/* left/right on the title screen pick the first stage */
#define CHEAT_POWER (0x08)		/* start with two orbs, and keep them after losing a life */

extern unsigned char cheat_flags;
extern unsigned char start_stage;

/* What gameplay_loop() is currently doing; handy for tests and cheats. */
#define STATE_TITLE (0)
#define STATE_STORY (1)
#define STATE_PLAYING (2)
#define STATE_BOSS (3)
#define STATE_STAGE_CLEAR (4)
#define STATE_GAME_OVER (5)
#define STATE_ENDING (6)
#define STATE_STAGE_CARD (7)

/* Returns 1 if the player was actually hurt (0 while invincible). */
char hurt_player(void);
char is_colliding_against_player(actor *act);
void add_score(unsigned int tens);

void spawn_powerup(int x, int y, char type);
void spawn_explosion(int x, int y);

void load_picture(unsigned char picture);
void draw_picture(unsigned char picture, unsigned char frame, unsigned char x, unsigned char y);
void clear_tilemap(void);

#endif /* GAME_H */

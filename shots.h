#ifndef SHOTS_H
#define SHOTS_H

#include "actor.h"
#include "config.h"

#define FOR_EACH_PLAYER_SHOT(sht) sht = player_shots; for (char i = PLAYER_SHOT_MAX; i; i--, sht++)

extern actor player_shots[PLAYER_SHOT_MAX];
extern unsigned char shot_count;	/* shots flying right now */

void init_player_shots(void);
void handle_player_shots(void);
void draw_player_shots(void);
char fire_player_shot(actor *player, char shot_type);
actor *check_collision_against_shots(actor *act);
actor *check_shots_in_box(int x, int y, unsigned char w, unsigned char h);

#endif /* SHOTS_H */

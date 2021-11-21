#ifndef SHOTS_H
#define SHOTS_H

#define PLAYER_SHOT_SPEED (4)
#define PLAYER_SHOT_MAX (16)
#define FOR_EACH_PLAYER_SHOT(sht) sht = player_shots; for (int i = PLAYER_SHOT_MAX; i; i--, sht++)
	
extern actor player_shots[PLAYER_SHOT_MAX];

void init_player_shots();
void handle_player_shots();
void draw_player_shots();
char fire_player_shot(actor *player, char shot_type);
actor *check_collision_against_shots(actor *_act);

#endif /* SHOTS_H */
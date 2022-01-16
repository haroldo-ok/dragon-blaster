#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lib/SMSlib.h"
#include "lib/PSGlib.h"
#include "actor.h"
#include "shot.h"
#include "shots.h"
#include "map.h"
#include "data.h"

actor player_shots[PLAYER_SHOT_MAX];

// Store which rows contain player shots, in order to speed up collision checks.
char player_shots_collision_rows[SCREEN_CHAR_H];
#define player_shots_collision_rows_end (player_shots_collision_rows + SCREEN_CHAR_H)

// Store which columns contain player shots, in order to speed up collision checks.
char player_shots_collision_cols[SCREEN_CHAR_W];
#define player_shots_collision_cols_end (player_shots_collision_cols + SCREEN_CHAR_W)

void clear_player_shots_collision() {
	memset(player_shots_collision_rows, 0, sizeof(player_shots_collision_rows));
	memset(player_shots_collision_cols, 0, sizeof(player_shots_collision_cols));
}

void mark_player_shot_collision_row(int y) {
	static char *ch;
	
	ch = player_shots_collision_rows + (y >> 3);
	
	*ch = 1; ch++;
	if (ch > player_shots_collision_rows_end) return;
	*ch = 1; ch++;
	if (ch > player_shots_collision_rows_end) return;
	*ch = 1;
}

void mark_player_shot_collision_col(int x) {
	static char *ch;
	
	ch = player_shots_collision_cols + (x >> 3);
	
	*ch = 1; ch++;
	if (ch > player_shots_collision_cols_end) return;
	*ch = 1;
}

char check_player_shot_collision_row(int y) {
	static char *ch;
	
	if (y < 0) return 0;
	
	ch = player_shots_collision_rows + (y >> 3);
	
	if (ch > player_shots_collision_rows_end) return 0;
	if (*ch) return 1;
	ch++;
	if (ch > player_shots_collision_rows_end) return 0;
	if (*ch) return 1;
	ch++;
	if (ch > player_shots_collision_rows_end) return 0;
	if (*ch) return 1;
	ch++;
	if (ch > player_shots_collision_rows_end) return 0;
	if (*ch) return 1;
	ch++;
	if (ch > player_shots_collision_rows_end) return 0;
	if (*ch) return 1;
	
	return 0;
}

char check_player_shot_collision_col(int x) {
	static char *ch;
	
	if (x < 0) return 0;
	
	ch = player_shots_collision_cols + (x >> 3);
	
	if (ch > player_shots_collision_cols_end) return 0;
	if (*ch) return 1;
	ch++;
	if (ch > player_shots_collision_cols_end) return 0;
	if (*ch) return 1;
	ch++;
	if (ch > player_shots_collision_cols_end) return 0;
	if (*ch) return 1;
	ch++;
	if (ch > player_shots_collision_cols_end) return 0;
	if (*ch) return 1;
	
	return 0;
}

void init_player_shots() {
	static actor *sht;
	
	FOR_EACH_PLAYER_SHOT(sht) {
		sht->active = 0;
	}
	
	clear_player_shots_collision();
}

void handle_player_shots() {
	static actor *sht;
	
	clear_player_shots_collision();
	FOR_EACH_PLAYER_SHOT(sht) {
		if (sht->active) {
			move_actor(sht);
			if (sht->y < 0 || sht->y > (SCREEN_H - 16)) sht->active = 0;
			if (sht->state == 1 && !sht->state_timer) sht->active = 0;
			if (sht->active) {
				mark_player_shot_collision_col(sht->x);
				mark_player_shot_collision_row(sht->y);
			}
		}
	}
}

void draw_player_shots() {
	static actor *sht;
	
	FOR_EACH_PLAYER_SHOT(sht) {
		draw_actor(sht);
	}
}

char fire_player_shot(actor *player, char shot_type) {
	static actor *sht;
	static char shots_to_fire, fired;
	static shot_info *info;
	static path *path;
	
	info = player_shot_infos + shot_type;
	path = info->paths;
	shots_to_fire = info->length;
	fired = 0;
	
	FOR_EACH_PLAYER_SHOT(sht) {
		if (!sht->active) {
			init_actor(sht, 
				player->x + path->x, player->y + path->y, 
				1, 1, 
				info->base_tile, info->frame_count);
				
			sht->path = path->steps;
			sht->path_flags = path->flags;
			sht->state = 1;
			sht->state_timer = info->life_time;
						
			// Fired something
			fired = 1;
			path++;
			shots_to_fire--;
			if (!shots_to_fire)	return 1;
		}
	}

	// Didn't fire anything
	return fired;
}

actor *check_collision_against_shots(actor *_act) {
	static actor *act, *sht;
	static int act_x, act_y;
	static int sht_x, sht_y;
	
	act = _act;

	act_x = act->x;	
	if (!check_player_shot_collision_col(act_x)) return 0;

	act_y = act->y;	
	if (!check_player_shot_collision_row(act_y)) return 0;
	
	FOR_EACH_PLAYER_SHOT(sht) {
		if (sht->active) {
			sht_x = sht->x;
			sht_y = sht->y;
			if (sht_x > act_x - 8 && sht_x < act_x + 16 &&
				sht_y > act_y - 16 && sht_y < act_y + 16) {
				return sht;
			}
		}		
	}	
	
	return 0;
}

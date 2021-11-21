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

void init_player_shots() {
	static actor *sht;
	
	FOR_EACH_PLAYER_SHOT(sht) {
		sht->active = 0;
	}
}

void handle_player_shots() {
	static actor *sht;
	
	FOR_EACH_PLAYER_SHOT(sht) {
		if (sht->active) {
			move_actor(sht);
			if (sht->y < 0) sht->active = 0;
			if (sht->state == 1 && !sht->state_timer) sht->active = 0;
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
	act_y = act->y;
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

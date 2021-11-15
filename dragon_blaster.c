#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lib/SMSlib.h"
#include "lib/PSGlib.h"
#include "actor.h"
#include "shot.h"
#include "map.h"
#include "data.h"

#define PLAYER_TOP (0)
#define PLAYER_LEFT (0)
#define PLAYER_RIGHT (256 - 16)
#define PLAYER_BOTTOM (SCREEN_H - 16)
#define PLAYER_SPEED (3)

#define PLAYER_SHOT_SPEED (4)
#define PLAYER_SHOT_MAX (16)
#define FOR_EACH_PLAYER_SHOT(sht) sht = player_shots; for (int i = PLAYER_SHOT_MAX; i; i--, sht++)

#define ENEMY_MAX (3)
#define FOR_EACH_ENEMY(enm) enm = enemies; for (int i = ENEMY_MAX; i; i--, enm++)
	
#define POWERUP_BASE_TILE (100)
#define POWERUP_LIGHTINING_TILE (POWERUP_BASE_TILE)
#define POWERUP_FIRE_TILE (POWERUP_BASE_TILE + 8)
#define POWERUP_WIND_TILE (POWERUP_BASE_TILE + 16)
#define POWERUP_NONE_TILE (POWERUP_BASE_TILE + 24)
#define POWERUP_LIGHTINING (1)
#define POWERUP_FIRE (2)
#define POWERUP_WIND (3)

actor player;
actor player_shots[PLAYER_SHOT_MAX];
actor enemies[ENEMY_MAX];
actor icons[2];
actor powerup;

struct ply_ctl {
	char shot_delay;
	char shot_type;
	char pressed_shot_selection;
	
	char powerup1, powerup2;
	char powerup1_active, powerup2_active;

	char death_delay;
} ply_ctl;

struct enemy_spawner {
	char type;
	char x;
	char flags;
	char delay;
	char next;
	path_step *path;
	char all_dead;
} enemy_spawner;

void load_standard_palettes() {
	SMS_loadBGPalette(tileset_palette_bin);
	SMS_loadSpritePalette(sprites_palette_bin);
	SMS_setSpritePaletteColor(0, 0);
}

char fire_player_shot();

void select_combined_powerup() {
	switch (ply_ctl.powerup1) {
	case POWERUP_LIGHTINING:
		switch (ply_ctl.powerup2) {
		case POWERUP_LIGHTINING: ply_ctl.shot_type = 3; break; // Thunderstrike
		case POWERUP_FIRE: ply_ctl.shot_type = 6; break; // Firebolt
		case POWERUP_WIND: ply_ctl.shot_type = 7; break; // Thunderstorm
		}
		break;
	
	case POWERUP_FIRE:
		switch (ply_ctl.powerup2) {
		case POWERUP_LIGHTINING: ply_ctl.shot_type = 6; break; // Firebolt
		case POWERUP_FIRE: ply_ctl.shot_type = 4; break; // Hellfire
		case POWERUP_WIND: ply_ctl.shot_type = 8; break; // Firestorm
		}
		break;

	case POWERUP_WIND:
		switch (ply_ctl.powerup2) {
		case POWERUP_LIGHTINING: ply_ctl.shot_type = 7; break; // Thunderstorm
		case POWERUP_FIRE: ply_ctl.shot_type = 8; break; // Firestorm
		case POWERUP_WIND: ply_ctl.shot_type = 5; break; // Tempest
		}
		break;

	}
}

void switch_powerup() {
	if (ply_ctl.powerup1_active && ply_ctl.powerup2_active) {
		// Only the first powerup will be active
		ply_ctl.powerup1_active = 1;
		ply_ctl.powerup2_active = 0;
		ply_ctl.shot_type = ply_ctl.powerup1 - 1;
	} else if (ply_ctl.powerup1_active) {
		// Only the second powerup will be active
		ply_ctl.powerup1_active = 0;
		ply_ctl.powerup2_active = 1;
		ply_ctl.shot_type = ply_ctl.powerup2 - 1;
	} else {
		// Both powerups will be active
		ply_ctl.powerup1_active = 1;
		ply_ctl.powerup2_active = 1;
		select_combined_powerup();
	}
}

void handle_player_input() {
	static unsigned char joy;	
	joy = SMS_getKeysStatus();

	if (joy & PORT_A_KEY_LEFT) {
		if (player.x > PLAYER_LEFT) player.x -= PLAYER_SPEED;
	} else if (joy & PORT_A_KEY_RIGHT) {
		if (player.x < PLAYER_RIGHT) player.x += PLAYER_SPEED;
	}

	if (joy & PORT_A_KEY_UP) {
		if (player.y > PLAYER_TOP) player.y -= PLAYER_SPEED;
	} else if (joy & PORT_A_KEY_DOWN) {
		if (player.y < PLAYER_BOTTOM) player.y += PLAYER_SPEED;
	}
	
	if (joy & PORT_A_KEY_2) {
		if (!ply_ctl.shot_delay) {
			if (fire_player_shot()) {
				ply_ctl.shot_delay = player_shot_infos[ply_ctl.shot_type].firing_delay;
			}
		}
	}
	
	if (joy & PORT_A_KEY_1) {
		if (!ply_ctl.pressed_shot_selection && ply_ctl.powerup2) {
			switch_powerup();
			ply_ctl.pressed_shot_selection = 1;
		}
	} else {
		ply_ctl.pressed_shot_selection = 0;
	}
	
	if (ply_ctl.shot_delay) ply_ctl.shot_delay--;
	if (ply_ctl.death_delay) ply_ctl.death_delay--;

}

void draw_player() {
	if (!(ply_ctl.death_delay & 0x08)) draw_actor(&player);
}

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

char fire_player_shot() {
	static actor *sht;
	static char shots_to_fire, fired;
	static shot_info *info;
	static path *path;
	
	info = player_shot_infos + ply_ctl.shot_type;
	path = info->paths;
	shots_to_fire = info->length;
	fired = 0;
	
	FOR_EACH_PLAYER_SHOT(sht) {
		if (!sht->active) {
			init_actor(sht, 
				player.x + path->x, player.y + path->y, 
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

char is_colliding_against_player(actor *_act) {
	static actor *act;
	static int act_x, act_y;
	
	act = _act;
	act_x = act->x;
	act_y = act->y;
	
	if (player.x > act_x - 12 && player.x < act_x + 12 &&
		player.y > act_y - 12 && player.y < act_y + 12) {
		return 1;
	}
	
	return 0;
}

void init_enemies() {
	static actor *enm;

	enemy_spawner.x = 0;	
	enemy_spawner.delay = 0;
	enemy_spawner.next = 0;
	
	FOR_EACH_ENEMY(enm) {
		enm->active = 0;
	}
}

void handle_enemies() {
	static actor *enm, *sht;	
	
	if (enemy_spawner.delay) {
		enemy_spawner.delay--;
	} else if (enemy_spawner.next != ENEMY_MAX) {
		if (!enemy_spawner.x) {
			enemy_spawner.type = rand() & 1;
			enemy_spawner.x = 8 + rand() % 124;
			enemy_spawner.flags = 0;
			enemy_spawner.path = (path_step *) path1_path;
			if (rand() & 1) {
				enemy_spawner.x += 124;
				enemy_spawner.flags |= PATH_FLIP_X;
			}
		}
		
		enm = enemies + enemy_spawner.next;
		
		init_actor(enm, enemy_spawner.x, 0, 2, 1, 66, 1);
		enm->path_flags = enemy_spawner.flags;
		enm->path = enemy_spawner.path;
		enm->state = enemy_spawner.type;

		enemy_spawner.delay = 10;
		enemy_spawner.next++;
	}
	
	enemy_spawner.all_dead = 1;
	FOR_EACH_ENEMY(enm) {
		move_actor(enm);
		
		if (enm->x < -32 || enm->x > 287 || enm->y < -16 || enm->y > 192) {
			enm->active = 0;
		}

		if (enm->active) {
			sht = check_collision_against_shots(enm);
			if (sht) {
				sht->active = 0;
				enm->active = 0;
			}
			
			if (!ply_ctl.death_delay && is_colliding_against_player(enm)) {
				enm->active = 0;
				ply_ctl.death_delay = 60;
			}
		}
		
		if (enm->active) enemy_spawner.all_dead = 0;
	}	
	
	if (enemy_spawner.all_dead) {
		enemy_spawner.x = 0;
		enemy_spawner.next = 0;
	}
}

void draw_enemies() {
	static actor *enm;
	
	FOR_EACH_ENEMY(enm) {
		draw_actor(enm);
	}
}

void draw_background() {
	unsigned int *ch = background_tilemap_bin;
	
	SMS_setNextTileatXY(0, 0);
	for (char y = 0; y != 30; y++) {
		// Repeat pattern every two lines
		if (!(y & 0x01)) {
			ch = background_tilemap_bin;
		}
		
		for (char x = 0; x != 32; x++) {
			unsigned int tile_number = *ch + 256;
			SMS_setTile(tile_number);
			ch++;
		}
	}
}

void init_powerups() {
	init_actor(icons, 256 - 32 - 8, 8, 2, 1, POWERUP_LIGHTINING_TILE, 1);	
	init_actor(icons + 1, 256 - 16 - 8, 8, 2, 1, POWERUP_FIRE_TILE, 1);	

	init_actor(&powerup, 0, 0, 2, 1, POWERUP_LIGHTINING_TILE, 2);
	powerup.active = 0;
}

char powerup_base_tile(char type) {
	switch (type) {
	case POWERUP_LIGHTINING: return POWERUP_LIGHTINING_TILE;
	case POWERUP_FIRE: return POWERUP_FIRE_TILE;
	case POWERUP_WIND: return POWERUP_WIND_TILE;
	}
	
	return POWERUP_NONE_TILE;
}

void handle_icons() {
	static int tile;
	
	tile = powerup_base_tile(ply_ctl.powerup1);
	if (!ply_ctl.powerup1_active) tile += 4;
	icons[0].base_tile = tile;
	
	tile = powerup_base_tile(ply_ctl.powerup2);
	if (ply_ctl.powerup2 && !ply_ctl.powerup2_active) tile += 4;
	icons[1].base_tile = tile;
}

void handle_powerups() {
	powerup.y++;
	if (powerup.y > SCREEN_H) powerup.active = 0;

	if (powerup.active) {
		// Check collision with player
		if (powerup.x > player.x - 16 && powerup.x < player.x + 24 &&
			powerup.y > player.y - 16 && powerup.y < player.y + 16) {
			if (!ply_ctl.powerup2) {
				// Second is absent
				ply_ctl.powerup2 = powerup.state;
			} else  if (!ply_ctl.powerup1_active) {
				// First is inactive
				ply_ctl.powerup1 = powerup.state;
			} else if (!ply_ctl.powerup2_active) {
				// Second is inactive
				ply_ctl.powerup2 = powerup.state;
			} else {
				// Both are active
				ply_ctl.powerup1 = ply_ctl.powerup2;
				ply_ctl.powerup2 = powerup.state;				
			}
			
			ply_ctl.powerup1_active = 1;
			ply_ctl.powerup2_active = 1;
			select_combined_powerup();
			
			powerup.active = 0;			
		}
	} else {
		powerup.x = 8 + rand() % (256 - 24);
		powerup.y = -16;
		powerup.active = 1;
		powerup.state = 1 + rand() % 3;
		powerup.base_tile = powerup_base_tile(powerup.state);
	}	
}

void draw_powerups() {
	draw_actor(icons);
	draw_actor(icons + 1);		
	draw_actor(&powerup);
}

void main() {	
	SMS_useFirstHalfTilesforSprites(1);
	SMS_setSpriteMode(SPRITEMODE_TALL);
	SMS_VDPturnOnFeature(VDPFEATURE_HIDEFIRSTCOL);

	SMS_displayOff();
	SMS_loadPSGaidencompressedTiles(sprites_tiles_psgcompr, 0);
	SMS_loadPSGaidencompressedTiles(tileset_tiles_psgcompr, 256);
	load_standard_palettes();
	
	init_map(level1_bin);
	draw_map_screen();

	SMS_displayOn();
	
	init_actor(&player, 116, PLAYER_BOTTOM - 16, 3, 1, 2, 4);
	player.animation_delay = 20;
	ply_ctl.shot_delay = 0;
	ply_ctl.shot_type = 0;
	ply_ctl.powerup1 = 1;
	ply_ctl.powerup2 = 0;
	ply_ctl.powerup1_active = 1;
	ply_ctl.powerup2_active = 0;
	ply_ctl.death_delay = 0;

	init_enemies();
	init_player_shots();
	init_powerups();
	
	while (1) {	
		handle_player_input();
		handle_enemies();
		handle_icons();
		handle_powerups();
		handle_player_shots();
	
		SMS_initSprites();

		draw_player();
		draw_enemies();
		draw_powerups();
		draw_player_shots();		
		
		SMS_finalizeSprites();
		SMS_waitForVBlank();
		SMS_copySpritestoSAT();
		
		// Scroll two lines per frame
		draw_map();		
		draw_map();		
	}
}

SMS_EMBED_SEGA_ROM_HEADER(9999,0); // code 9999 hopefully free, here this means 'homebrew'
SMS_EMBED_SDSC_HEADER(0,6, 2021,11,15, "Haroldo-OK\\2021", "Dragon Blaster",
  "A dragon-themed shoot-em-up.\n"
  "Made for the SHMUP JAM 1 - Dragons - https://itch.io/jam/shmup-jam-1-dragons\n"
  "Built using devkitSMS & SMSlib - https://github.com/sverx/devkitSMS");

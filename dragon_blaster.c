#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lib/SMSlib.h"
#include "lib/PSGlib.h"
#include "actor.h"
#include "shot.h"
#include "boss_shot.h"
#include "shots.h"
#include "map.h"
#include "score.h"
#include "data.h"

#define PLAYER_TOP (0)
#define PLAYER_LEFT (0)
#define PLAYER_RIGHT (256 - 16)
#define PLAYER_BOTTOM (SCREEN_H - 16)
#define PLAYER_SPEED (3)

#define ENEMY_MAX (3)
#define ENEMY_SHOT_MAX (16)
#define FOR_EACH_ENEMY(enm) enm = enemies; for (int i = ENEMY_MAX; i; i--, enm++)
#define FOR_EACH_ENEMY_SHOT(sht) sht = enemy_shots; for (int i = ENEMY_SHOT_MAX; i; i--, sht++)
	
#define POWERUP_BASE_TILE (100)
#define POWERUP_LIGHTINING_TILE (POWERUP_BASE_TILE)
#define POWERUP_FIRE_TILE (POWERUP_BASE_TILE + 8)
#define POWERUP_WIND_TILE (POWERUP_BASE_TILE + 16)
#define POWERUP_NONE_TILE (POWERUP_BASE_TILE + 24)
#define POWERUP_LIGHTINING (1)
#define POWERUP_FIRE (2)
#define POWERUP_WIND (3)

#define TIMER_MAX (60)

actor player;
actor enemies[ENEMY_MAX];
actor enemy_shots[ENEMY_SHOT_MAX];
actor icons[2];
actor powerup;
actor timer_label;
actor time_over;

score_display timer;
score_display score;

struct ply_ctl {
	char shot_delay;
	char shot_type;
	char pressed_shot_selection;
	
	char powerup1, powerup2;
	char powerup1_active, powerup2_active;

	char death_delay;
} ply_ctl;

struct boss {
	char loaded;
	int x, y;
	int next_x, next_y;
	char move_delay;
} boss;

struct enemy_spawner {
	char type;
	char x;
	char flags;
	char delay;
	char next;
	path_step *path;
	char all_dead;
} enemy_spawner;

char timer_delay;
char frames_elapsed;

void load_standard_palettes() {
	SMS_loadBGPalette(tileset_palette_bin);
	SMS_loadSpritePalette(sprites_palette_bin);
	SMS_setSpritePaletteColor(0, 0);
}

void update_score(actor *enm, actor *sht);

void wait_button_press() {
	do {
		SMS_waitForVBlank();
	} while (!(SMS_getKeysStatus() & (PORT_A_KEY_1 | PORT_A_KEY_2)));
}

void wait_button_release() {
	do {
		SMS_waitForVBlank();
	} while (SMS_getKeysStatus() & (PORT_A_KEY_1 | PORT_A_KEY_2));
}

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
			if (fire_player_shot(&player, ply_ctl.shot_type)) {
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
				update_score(enm, sht);
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
			update_score(&powerup, 0);

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

void update_score(actor *enm, actor *sht) {
	increment_score_display(&score, enm == &powerup ? 5 : 1);
}

void init_score() {
	init_actor(&timer_label, 16, 8, 1, 1, 178, 1);
	init_score_display(&timer, 24, 8, 236);
	update_score_display(&timer, TIMER_MAX);
	timer_delay = 60;
	frames_elapsed = 0;
	
	init_score_display(&score, 16, 24, 236);
}

void handle_score() {
	if (timer_delay) {
		timer_delay--;
	} else {
		if (timer.value) {
			char decrement = frames_elapsed / 60;
			if (decrement > timer.value) decrement = timer.value;
			increment_score_display(&timer, -decrement);
		}
		timer_delay = 60;
		frames_elapsed = 0;
	}
}

void draw_score() {
	draw_actor(&timer_label);
	draw_score_display(&timer);

	draw_score_display(&score);
}

void clear_tilemap() {
	SMS_setNextTileatXY(0, 0);
	for (int i = (SCREEN_CHAR_W * SCROLL_CHAR_H); i; i--) {
		SMS_setTile(0);
	}
}

void init_boss() {
	SMS_loadPSGaidencompressedTiles(dracolich_tiles_psgcompr, 256);	
	SMS_loadBGPalette(dracolich_palette_bin);
	SMS_setSpritePaletteColor(0, 0);
	SMS_setBGPaletteColor(0, 0);

	clear_tilemap();
	SMS_setBGScrollX(0);
	SMS_setBGScrollY(0);

	// Draws the boss.
	unsigned int *t = dracolich_tilemap_bin;
	for (char y = 0; y != 16; y++) {
		SMS_setNextTileatXY(0, y);
		for (char x = 0; x != 12; x++) {
			SMS_setTile(*t + 256);
			t++;
		}
	}

	boss.x = 128 - 96 / 2;
	boss.y = 0;
	boss.next_x = boss.x;
	boss.next_y = boss.y;
	boss.move_delay = 0;
	boss.loaded = 1;

	SMS_setBGScrollX(boss.x);
	SMS_setBGScrollY(boss.y);
}

void handle_boss() {
	if (!boss.loaded) return;
	
	if (boss.move_delay) {
		// Wait
		boss.move_delay--;
	} else if (boss.x != boss.next_x || boss.y != boss.next_y) {
		// Move towards target
		
		if (boss.x < boss.next_x) {
			boss.x++;
		} else if (boss.x > boss.next_x) {
			boss.x--;
		}

		if (boss.y < boss.next_y) {
			boss.y++;
		} else if (boss.y > boss.next_y) {
			boss.y--;
		}
	} else {
		// Select movement target
		boss.next_x = rand() % (SCREEN_W - 96);
		boss.next_y = rand() % (SCREEN_H - 128);
		boss.move_delay = 30 + rand() % 20;
	}
		
}
void draw_boss() {
	if (!boss.loaded) return;
	
	SMS_setBGScrollX(boss.x);
	SMS_setBGScrollY(SCROLL_H - boss.y);
}

void interrupt_handler() {
	PSGFrame();
	PSGSFXFrame();
	frames_elapsed++;
}

void init_enemy_shots() {
	static actor *sht;
	
	FOR_EACH_ENEMY_SHOT(sht) {
		sht->active = 0;
	}
}

void handle_enemy_shots() {
	static actor *sht;
	
	FOR_EACH_ENEMY_SHOT(sht) {
		if (sht->active) {
			move_actor(sht);
			if (sht->y < 0 || sht->y > (SCREEN_H - 16)) sht->active = 0;
			if (sht->state == 1 && !sht->state_timer) sht->active = 0;
		}
	}
}

void draw_enemy_shots() {
	static actor *sht;
	
	FOR_EACH_ENEMY_SHOT(sht) {
		draw_actor(sht);
	}
}

char fire_enemy_shot(int x, int y, char shot_type) {
	static actor *sht;
	static char shots_to_fire, fired;
	static shot_info *info;
	static path *path;
	
	info = boss_shot_infos + shot_type;
	path = info->paths;
	shots_to_fire = info->length;
	fired = 0;
	
	FOR_EACH_ENEMY_SHOT(sht) {
		if (!sht->active) {
			init_actor(sht, 
				x + path->x, y + path->y, 
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

void gameplay_loop() {	
	SMS_useFirstHalfTilesforSprites(1);
	SMS_setSpriteMode(SPRITEMODE_TALL);
	SMS_VDPturnOnFeature(VDPFEATURE_HIDEFIRSTCOL);

	SMS_displayOff();
	SMS_loadPSGaidencompressedTiles(sprites_tiles_psgcompr, 0);
	SMS_loadPSGaidencompressedTiles(tileset_tiles_psgcompr, 256);
	load_standard_palettes();
	
	init_map(level1_bin);
	draw_map_screen();

	SMS_setLineInterruptHandler(&interrupt_handler);
	SMS_setLineCounter(180);
	SMS_enableLineInterrupt();

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
	
	boss.loaded = 0;

	init_enemies();
	init_player_shots();
	init_enemy_shots();
	init_powerups();
	init_score();
	
	while (timer.value) {	
		handle_player_input();
		handle_enemies();
		handle_boss();
		handle_icons();
		handle_powerups();
		handle_player_shots();
		handle_enemy_shots();
		handle_score();
		
		SMS_initSprites();

		draw_player();
		draw_enemies();
		draw_boss();
		draw_powerups();
		draw_player_shots();
		draw_enemy_shots();
		draw_score();
		
		SMS_finalizeSprites();
		SMS_waitForVBlank();
		SMS_copySpritestoSAT();
		
		if (boss.loaded) {
		} else {
			// Scroll two lines per frame
			draw_map();		
			draw_map();
			
			if (timer.value < 59)  init_boss();
		}
	}
}

void timeover_sequence() {
	char timeover_delay = 128;
	char pressed_button = 0;
	
	init_actor(&time_over, 107, 64, 6, 1, 180, 1);

	while (timeover_delay || !pressed_button) {
		SMS_initSprites();

		if (!(timeover_delay & 0x10)) draw_actor(&time_over);
		
		draw_player();
		draw_enemies();
		draw_player_shots();
		draw_score();
		
		SMS_finalizeSprites();
		SMS_waitForVBlank();
		SMS_copySpritestoSAT();
		
		draw_map();
		
		if (timeover_delay) {
			timeover_delay--;
		} else {
			pressed_button = SMS_getKeysStatus() & (PORT_A_KEY_1 | PORT_A_KEY_2);
		}
	}
	
	wait_button_release();
}

void main() {	
	while (1) {
		gameplay_loop();
		timeover_sequence();
	}
}

SMS_EMBED_SEGA_ROM_HEADER(9999,0); // code 9999 hopefully free, here this means 'homebrew'
SMS_EMBED_SDSC_HEADER(0,9, 2022,02,20, "Haroldo-OK\\2022", "Dragon Blaster",
  "A dragon-themed shoot-em-up.\n"
  "Originally made for the SHMUP JAM 1 - Dragons - https://itch.io/jam/shmup-jam-1-dragons\n"
  "Enhanced for SMS Power! Competition 2022 - https://www.smspower.org/forums/18879-Competitions2022DeadlineIs27thMarch\n"
  "Built using devkitSMS & SMSlib - https://github.com/sverx/devkitSMS");

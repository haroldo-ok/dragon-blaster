#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lib/SMSlib.h"
#include "lib/PSGlib.h"
#include "actor.h"
#include "map.h"

// TODO: Refactor enemy logic, move to its own source file
extern void create_enemy_spawner(char x);

struct map_data {
	char *level_data;
	char *next_row;
	char background_y;
	char lines_before_next;
	char scroll_y;
} map_data;

void init_map(void *level_data) {
	map_data.level_data = level_data;
	map_data.next_row = level_data;
	map_data.background_y = SCROLL_CHAR_H - 2;
	map_data.lines_before_next = 0;
	map_data.scroll_y = 0;
}

void decompress_map_row(char *buffer) {
	static char *o, *d;
	static char remaining, ch, repeat, pos;
	
	o = map_data.next_row;
	d = buffer;
	for (remaining = 16; remaining; ) {
		ch = *o;
		o++;
		
		if (ch & 0x80) {
			// Has repeat flag: repeat n times
			repeat = ch & 0x7F;
			ch = *o;
			o++;
			
			for (; repeat && remaining; repeat--, remaining--) {
				*d = ch;
				d++;
			}
		} else if (ch & 0x40) {
			// Is a sprite declaration
			pos = (ch & 0x1F) << 4;
			o++;
			create_enemy_spawner(pos);
		} else {
			// Just use the char
			*d = ch;
			d++;
			remaining--;
		}
	}
	
	map_data.next_row = o;
}

void draw_map_row() {
	static char i, j;
	static char y;
	static char *map_char;
	static unsigned int base_tile, tile;
	static char buffer[16];

	decompress_map_row(buffer);

	for (i = 2, y = map_data.background_y, base_tile = 256; i; i--, y++, base_tile++) {
		SMS_setNextTileatXY(0, y);
		for (j = 16, map_char = buffer; j; j--, map_char++) {
			tile = base_tile + (*map_char << 2);
			SMS_setTile(tile);
			SMS_setTile(tile + 2);
		}
	}
	
	//map_data.next_row += 16;
	if (*map_data.next_row == 0xFF) {
		// Reached the end of the map; reset
		map_data.next_row = map_data.level_data;
	}
	
	if (map_data.background_y) {
		map_data.background_y -= 2;
	} else {
		map_data.background_y = SCROLL_CHAR_H - 2;
	}
	map_data.lines_before_next = 15;
}

void draw_map_screen() {
	map_data.background_y = SCREEN_CHAR_H - 2;
	
	while (map_data.background_y < SCREEN_CHAR_H) {
		draw_map_row();
	}
	draw_map_row();
}

void draw_map() {
	if (map_data.lines_before_next) {
		map_data.lines_before_next--;
	} else {
		draw_map_row();
	}

	SMS_setBGScrollY(map_data.scroll_y);
	if (map_data.scroll_y) {
		map_data.scroll_y--;
	} else {
		map_data.scroll_y = SCROLL_H - 1;
	}
}

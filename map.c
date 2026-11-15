#include <stdlib.h>
#include <string.h>
#include "lib/SMSlib.h"
#include "actor.h"
#include "map.h"
#include "enemy.h"
#include "game.h"
#include "gen/assets.h"

/*
 * Level data, as written by tool/build_assets.py from the Tiled maps
 * (bottom row first):
 *   0x00..0x3F        one tile
 *   0x80|n, tile      the same tile n times
 *   0x40|column, id   an object on this row: a wave (id < ITEM_OBJECT_BASE) or an item
 *   0xFF              end of the level
 */

struct map_data {
	const unsigned char *level_data;
	const unsigned char *next_row;
	unsigned char bank;
	unsigned char background_y;
	unsigned char lines_before_next;
	unsigned char scroll_y;
	unsigned char spawn_objects;
	unsigned char half_pending;
} map_data;

static unsigned char row_buffer[16];

unsigned char map_finished;

void init_map(const void *level_data, unsigned char bank) {
	map_data.level_data = level_data;
	map_data.bank = bank;
	map_data.next_row = level_data;
	map_data.background_y = SCROLL_CHAR_H - 2;
	map_data.lines_before_next = 0;
	map_data.scroll_y = 0;
	map_data.spawn_objects = 0;
	map_data.half_pending = 0;
	map_finished = 0;
}

void decompress_map_row(unsigned char *buffer) {
	static const unsigned char *o;
	static unsigned char *d;
	static unsigned char remaining, ch, repeat, pos;

	// The level sits in its own ROM bank.
	SMS_mapROMBank(map_data.bank);

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
			// Is an object declaration
			pos = (ch & 0x0F) << 4;
			ch = *o;
			o++;

			if (map_data.spawn_objects) {
				if (ch < ITEM_OBJECT_BASE) {
					start_wave(pos, ch);
				} else {
					spawn_powerup(pos, -16, 1 + ch - ITEM_OBJECT_BASE);
				}
			}
		} else {
			// Just use the char
			*d = ch;
			d++;
			remaining--;
		}
	}

	map_data.next_row = o;
	if (*o == 0xFF) {
		// Reached the end of the map: the scrolling stops once this row is in place.
		map_finished = 2;
	}

	SMS_mapROMBank(GAME_BANK);
}

/* Writes one of the two rows of 8x8 tiles that make a row of the map. */
static void draw_map_half_row(unsigned char half) {
	static unsigned char j;
	static unsigned char *map_char;
	static unsigned int base_tile, tile;

	base_tile = 256 + half;
	SMS_setNextTileatXY(0, map_data.background_y + half);
	for (j = 16, map_char = row_buffer; j; j--, map_char++) {
		tile = base_tile + (*map_char << 2);
		SMS_setTile(tile);
		SMS_setTile(tile + 2);
	}
}

static void next_map_row(void) {
	if (map_data.background_y) {
		map_data.background_y -= 2;
	} else {
		map_data.background_y = SCROLL_CHAR_H - 2;
	}
}

void draw_map_row(void) {
	decompress_map_row(row_buffer);
	draw_map_half_row(1);
	draw_map_half_row(0);
	next_map_row();
	map_data.lines_before_next = 15;
}

void draw_map_screen(void) {
	map_data.background_y = SCREEN_CHAR_H - 2;

	// Nothing spawns from the part of the level that is already on screen.
	map_data.spawn_objects = 0;
	while (map_data.background_y < SCREEN_CHAR_H) {
		draw_map_row();
	}
	draw_map_row();
	map_data.spawn_objects = 1;
}

void draw_map(void) {
	if (map_finished == 1) return;

	// A new row of the map is written over two frames, to spread the work:
	// its lower half first, since that is the one about to scroll into view.
	if (map_data.lines_before_next == 15 && map_data.half_pending) {
		map_data.half_pending = 0;
		draw_map_half_row(0);
		next_map_row();
	}

	if (map_data.lines_before_next) {
		map_data.lines_before_next--;
	} else if (map_finished == 2) {
		// The last row is now completely on screen.
		map_finished = 1;
	} else {
		decompress_map_row(row_buffer);
		draw_map_half_row(1);
		map_data.half_pending = 1;
		map_data.lines_before_next = 15;
	}

	SMS_setBGScrollY(map_data.scroll_y);
	if (map_data.scroll_y) {
		map_data.scroll_y--;
	} else {
		map_data.scroll_y = (unsigned char) (SCROLL_H - 1);
	}
}

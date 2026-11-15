#include <stdlib.h>
#include <string.h>
#include "lib/SMSlib.h"
#include "score.h"

void init_score_display(score_display *score, char x, char y, unsigned char base_tile) {
	score->x = x;
	score->y = y;
	score->base_tile = base_tile;
	score->dirty = 1;
	score->value = 0;
	score->characters[0] = 0;
}

void update_score_display(score_display *score, unsigned int value) {
	score->value = value;
	score->dirty = 1;
}

void increment_score_display(score_display *score, int delta) {
	update_score_display(score, score->value + delta);
}

char number_to_digits(unsigned int value, char *buffer) {
	static const unsigned int powers[] = {10000, 1000, 100, 10};
	static unsigned char i, digit, count;
	static unsigned int remaining, power;
	static char *ch;

	remaining = value;
	ch = buffer;
	count = 0;
	// Repeated subtraction is much faster than dividing on a Z80.
	for (i = 0; i != 4; i++) {
		power = powers[i];
		for (digit = 0; remaining >= power; digit++) remaining -= power;
		if (digit || count) {
			*ch++ = digit;
			count++;
		}
	}
	*ch++ = remaining;
	*ch = 0x7F;
	return count + 1;
}

void update_score_display_characters(score_display *_score) {
	static score_display *score;
	static unsigned char base_tile, n;
	static char *ch;

	score = _score;
	base_tile = score->base_tile;

	n = number_to_digits(score->value, score->characters);
	for (ch = score->characters; n; n--, ch++) {
		*ch = base_tile + (*ch << 1);
	}
	*ch = 0;

	score->dirty = 0;
}

void draw_score_display(score_display *_score) {
	static score_display *score;
	static unsigned char x, y;
	static char *ch;

	score = _score;
	if (score->dirty) update_score_display_characters(score);

	x = score->x;
	y = score->y;

	for (ch = score->characters; *ch; ch++, x += 8) {
		SMS_addSprite(x, y, *ch);
	}
}

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lib/SMSlib.h"
#include "lib/PSGlib.h"
#include "score.h"

void init_score_display(score_display *score, char x, char y, char base_tile) {
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

// Adapted from https://stackoverflow.com/a/784455/679240
void reverse_string(char *str) {
    static char *start, *end, temp;
	
    /* get range */
    start = str;
    end = start + strlen(str) - 1; /* -1 for \0 */

    /* reverse */
    while (end > start) {
        /* swap */
        temp = *start;
        *start = *end;
        *end = temp;

        /* move */
        start++;
        end--;
    }
}

void update_score_display_characters(score_display *_score) {
	static score_display *score;
	static unsigned int remaining;
	static char digit, base_tile;
	static char *ch;

	score = _score;
	base_tile = score->base_tile;
	ch = score->characters;
	
	remaining = score->value;
	
	do {
		digit = remaining % 10;
		remaining /= 10;
		
		*ch = base_tile + (digit << 1);
		ch++;
	} while (remaining);
	*ch = 0;
	
	reverse_string(score->characters);
	
	score->dirty = 0;
}

void draw_score_display(score_display *_score) {
	static score_display *score;
	static char x, y, *ch;

	score = _score;
	if (score->dirty) update_score_display_characters(score);
	
	x = score->x;
	y = score->y;
	
	for (ch = score->characters; *ch; ch++, x += 8) {
		SMS_addSprite(x, y, *ch);		
	}
}

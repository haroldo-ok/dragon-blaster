#ifndef SCORE_H
#define SCORE_H

typedef struct score_display {
	char x, y;
	char base_tile, dirty;
	unsigned int value;
	char characters[6];
} score_display;

void init_score_display(score_display *score, char x, char y, char base_tile);
void update_score_display(score_display *score, unsigned int value);
void increment_score_display(score_display *score, int delta);
void draw_score_display(score_display *score);

#endif /* SCORE_H */
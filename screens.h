#ifndef SCREENS_H
#define SCREENS_H

/* The screens between the action: title, story, stage cards, game over. */

void title_screen(void);
void story_screen(const unsigned char *text, unsigned char bank, unsigned char music);
void stage_card_screen(void);
void stage_clear_screen(unsigned int stage_bonus, unsigned int no_miss_bonus);
char game_over_screen(void);		/* returns 1 if the player wants to continue */
void the_end_screen(void);

#endif /* SCREENS_H */

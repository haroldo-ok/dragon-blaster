#ifndef BOSS_H
#define BOSS_H

extern unsigned char boss_active;	/* 1 while the boss is on screen */
extern unsigned char boss_defeated;	/* set when its death sequence has finished */
extern unsigned int boss_hp;

void init_boss(void);			/* call once per stage */
void start_boss(unsigned char boss_number, unsigned char sky);
void handle_boss(void);
void draw_boss(void);			/* sprites: life bar */
void update_boss_vram(void);		/* call right after the VBlank */

#endif /* BOSS_H */

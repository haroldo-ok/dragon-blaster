#ifndef MAP_H
#define MAP_H

extern unsigned char map_finished;	/* 1 once the whole level has scrolled by */

void init_map(const void *level_data, unsigned char bank);
void draw_map_screen(void);
void draw_map(void);

#endif /* MAP_H */

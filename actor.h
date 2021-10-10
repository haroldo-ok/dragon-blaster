#ifndef ACTOR_H
#define ACTOR_H

#define SCREEN_W (256)
#define SCREEN_H (192)
#define SCREEN_CHAR_W (32)
#define SCREEN_CHAR_H (24)


typedef union _fixed {
  struct {
    unsigned char l;
    signed char h;
  } b;
  int w;
} fixed;

typedef struct actor {
	char active;
	
	int x, y;
	fixed incr_x, incr_y;
	fixed spd_x, spd_y;
	
	char facing_left;
	
	char char_w, char_h;
	char pixel_w, pixel_h;
	
	unsigned char animation_delay, animation_delay_max;
	unsigned char base_tile, frame_count;
	unsigned char frame, frame_increment, frame_max;
	
	unsigned char state;
	int state_timer;
	
	char col_x, col_y, col_w, col_h;
} actor;

void draw_meta_sprite(int x, int y, int w, int h, unsigned char tile);
void init_actor(actor *act, int x, int y, int char_w, int char_h, unsigned char base_tile, unsigned char frame_count);
void move_actor(actor *act);
void draw_actor(actor *act);

void aim_actor_towards(actor *act, actor *target, int speed);

void wait_frames(int wait_time);

#endif /* ACTOR_H */
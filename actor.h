#ifndef ACTOR_H
#define ACTOR_H

#define SCREEN_W (256)
#define SCREEN_H (192)
#define SCROLL_H (224)
#define SCREEN_CHAR_W (32)
#define SCREEN_CHAR_H (24)
#define SCROLL_CHAR_H (28)

#define PATH_FLIP_X (0x01)
#define PATH_FLIP_Y (0x02)
#define PATH_2X_SPEED (0x04)

/* A path is a list of per-frame movements, ended by a {PATH_END, PATH_END} step. */
#define PATH_END (-128)

typedef struct _path_step {
	signed char x, y;
} path_step;

typedef struct _path {
	signed char x, y;
	char flags;
	const path_step *steps;
} path;

/*
 * NOTE: move_actor() and draw_actor() are written in assembly and rely on the
 * exact layout of this structure. If you add fields, add them at the end
 * (actor.c checks the layout when it is compiled).
 */
typedef struct actor {
	char active;

	int x, y;

	char char_w, char_h;
	char pixel_w, pixel_h;

	unsigned char animation_delay, animation_delay_max;
	unsigned char base_tile, frame_count;
	unsigned char frame, frame_increment, frame_max;

	char path_flags;
	const path_step *path, *curr_step;

	unsigned char state;
	int state_timer;

	unsigned char damage;
} actor;

void draw_meta_sprite(int x, int y, int w, int h, unsigned char tile);
void init_actor(actor *act, int x, int y, int char_w, int char_h, unsigned char base_tile, unsigned char frame_count);
void move_actor(actor *act);
void draw_actor(actor *act);

void wait_frames(int wait_time);
void clear_sprites(void);

#endif /* ACTOR_H */

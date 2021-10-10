#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lib/SMSlib.h"
#include "lib/PSGlib.h"
#include "actor.h"
#include "data.h"

void draw_meta_sprite(int x, int y, int w, int h, unsigned char tile) {
	static char i, j;
	static int sx, sy;
	static unsigned char st;
	
	sy = y;
	st = tile;
	for (i = h; i; i--) {
		if (y >= 0 && y < SCREEN_H) {
			sx = x;
			for (j = w; j; j--) {
				if (sx >= 0 && sx < SCREEN_W) {
					SMS_addSprite(sx, sy, tile);
				}
				sx += 8;
				tile += 2;
			}
		}
		sy += 16;
	}
}

void init_actor(actor *act, int x, int y, int char_w, int char_h, unsigned char base_tile, unsigned char frame_count) {
	static actor *sa;
	sa = act;
	
	sa->active = 1;
	
	sa->x = x;
	sa->y = y;
	sa->incr_x.w = 0;
	sa->incr_y.w = 0;
	sa->spd_x.w = 0;
	sa->spd_y.w = 0;
	sa->facing_left = 1;
	
	sa->char_w = char_w;
	sa->char_h = char_h;
	sa->pixel_w = char_w << 3;
	sa->pixel_h = char_h << 4;
	
	sa->animation_delay = 0;
	sa->animation_delay_max = 2;
	
	sa->base_tile = base_tile;
	sa->frame_count = frame_count;
	sa->frame = 0;
	sa->frame_increment = char_w * (char_h << 1);
	sa->frame_max = sa->frame_increment * frame_count;
	
	sa->col_w = sa->pixel_w - 4;
	sa->col_h = sa->pixel_h - 4;
	sa->col_x = (sa->pixel_w - sa->col_w) >> 1;
	sa->col_y = (sa->pixel_h - sa->col_h) >> 1;
	
	sa->state = 0;
	sa->state_timer = 256;
}

void move_actor(actor *act) {
	static actor *_act;
	
	if (!act->active) {
		return;
	}
	
	_act = act;
	
	_act->incr_x.w += _act->spd_x.w;
	_act->incr_y.w += _act->spd_y.w;
	
	if (_act->incr_x.b.h) {
		_act->x += _act->incr_x.b.h;
		_act->incr_x.b.h = 0;
	}
	
	if (_act->incr_y.b.h) {
		_act->y += _act->incr_y.b.h;
		_act->incr_y.b.h = 0;
	}
	
	if (_act->state_timer) _act->state_timer--;
}

void draw_actor(actor *act) {
	static actor *_act;
	static unsigned char frame_tile;
	
	if (!act->active) {
		return;
	}
	
	_act = act;
	
	frame_tile = _act->base_tile + _act->frame;
	if (!_act->facing_left) {
		frame_tile += _act->frame_max;
	}
	
	draw_meta_sprite(_act->x, _act->y, _act->char_w, _act->char_h, frame_tile);	

	if (_act->animation_delay) {
		_act->animation_delay--;
	} else {
		_act->frame += _act->frame_increment;
		if (_act->frame >= _act->frame_max) _act->frame = 0;
		_act->animation_delay = _act->animation_delay_max;
	}
}

void aim_actor_towards(actor *act, actor *target, int speed) {
	int delta_x = target->x - act->x;
	int delta_y = target->y - act->y;
	int distance = (abs(delta_x) + abs(delta_y)) / 4; // Manhattan distance divided by scaling factor
	if (!distance) distance = 1;
	
	act->spd_x.w = delta_x * speed / distance;
	act->spd_y.w = delta_y * speed / distance;
}

void wait_frames(int wait_time) {
	for (; wait_time; wait_time--) SMS_waitForVBlank();
}

#include <stdlib.h>
#include <string.h>
#include "lib/SMSlib.h"
#include "actor.h"
#include "shot.h"
#include "shots.h"

actor player_shots[PLAYER_SHOT_MAX];

/*
 * Collision checks run for every enemy, every frame, so the positions of the
 * shots that are flying are copied to a small array of (x, y) bytes, which a
 * few lines of assembly can scan quickly.
 */
unsigned char shot_count;
unsigned char shot_xy[PLAYER_SHOT_MAX * 2];
static actor *shot_actors[PLAYER_SHOT_MAX];
unsigned char shot_hit_w, shot_hit_h;

void init_player_shots(void) {
	static actor *sht;

	FOR_EACH_PLAYER_SHOT(sht) {
		sht->active = 0;
	}

	shot_count = 0;
}

typedef char check_actor_size[(sizeof(actor) == 25) ? 1 : -1];

static unsigned char *shot_xy_next;

/*
 * Moves the shots and lists the ones still flying. In C it would read:
 *
 *	shot_count = 0;
 *	FOR_EACH_PLAYER_SHOT(sht) {
 *		if (!sht->active) continue;
 *		move_actor(sht);
 *		if (sht->x < 0 || sht->x > SCREEN_W - 8 || sht->y < 0 || sht->y > SCREEN_H - 16 || !sht->state_timer) {
 *			sht->active = 0;
 *		} else {
 *			shot_xy[shot_count * 2] = sht->x;
 *			shot_xy[shot_count * 2 + 1] = sht->y;
 *			shot_actors[shot_count++] = sht;
 *		}
 *	}
 */
void handle_player_shots(void) __naked {
__asm
	xor	a, a
	ld	(_shot_count), a
	ld	hl, #_shot_xy
	ld	(_shot_xy_next), hl
	ld	hl, #_player_shots
	ld	b, #PLAYER_SHOT_MAX
00151$:
	ld	a, (hl)
	or	a, a
	jr	Z, 00155$
	push	bc
	push	hl
	call	_move_actor		; leaves the actor in IY

	; Outside the screen (or a negative coordinate)?
	ld	a, 2 (iy)
	or	a, 4 (iy)
	jr	NZ, 00153$
	ld	a, 3 (iy)
	cp	a, #177
	jr	NC, 00153$
	ld	a, 1 (iy)
	cp	a, #249
	jr	NC, 00153$
	; Burned out?
	ld	a, 22 (iy)
	or	a, 23 (iy)
	jr	Z, 00153$

	ld	hl, (_shot_xy_next)
	ld	a, 1 (iy)
	ld	(hl), a
	inc	hl
	ld	a, 3 (iy)
	ld	(hl), a
	inc	hl
	ld	(_shot_xy_next), hl
	ld	a, (_shot_count)
	ld	e, a
	inc	a
	ld	(_shot_count), a
	ld	d, #0
	ld	hl, #_shot_actors
	add	hl, de
	add	hl, de
	push	iy
	pop	de
	ld	(hl), e
	inc	hl
	ld	(hl), d
	jr	00154$
00153$:
	ld	0 (iy), #0
00154$:
	pop	hl
	pop	bc
00155$:
	ld	de, #25
	add	hl, de
	djnz	00151$
	ret
__endasm;
}

void draw_player_shots(void) {
	static actor *sht;

	FOR_EACH_PLAYER_SHOT(sht) {
		draw_actor(sht);
	}
}

char fire_player_shot(actor *player, char shot_type) {
	static actor *sht;
	static char shots_to_fire, fired;
	static const shot_info *info;
	static const path *path;

	info = player_shot_infos + shot_type;
	path = info->paths;
	shots_to_fire = info->length;
	fired = 0;

	FOR_EACH_PLAYER_SHOT(sht) {
		if (!sht->active) {
			init_actor(sht,
				player->x + path->x, player->y + path->y,
				1, 1,
				info->base_tile, info->frame_count);

			sht->path = path->steps;
			sht->path_flags = path->flags;
			sht->state = 1;
			sht->state_timer = info->life_time;
			sht->damage = info->damage;

			// Fired something
			fired = 1;
			path++;
			shots_to_fire--;
			if (!shots_to_fire)	return 1;
		}
	}

	// Didn't fire anything
	return fired;
}

/*
 * Looks for a shot whose (x - left) is below shot_hit_w and whose (y - top) is
 * below shot_hit_h (bytes, wrapping around). Returns its index + 1, or 0.
 */
static unsigned char find_shot(unsigned char left, unsigned char top) __naked {
	left; top;
__asm
	ld	d, a
	ld	e, l
	ld	a, (_shot_count)
	or	a, a
	ret	Z
	ld	b, a
	ld	a, (_shot_hit_w)
	ld	c, a
	ld	hl, #_shot_xy
00151$:
	ld	a, (hl)
	inc	hl
	sub	a, d
	cp	a, c
	jr	NC, 00152$
	ld	a, (hl)
	sub	a, e
	push	hl
	ld	hl, #_shot_hit_h
	cp	a, (hl)
	pop	hl
	jr	C, 00153$
00152$:
	inc	hl
	djnz	00151$
	xor	a, a
	ret
00153$:
	ld	a, (_shot_count)
	sub	a, b
	inc	a
	ret
__endasm;
}

/*
 * Returns a player shot touching the box (x, y, w, h), or 0. The shot is
 * taken off the list, so it can only hit one thing: the caller is expected
 * to deactivate it.
 */
actor *check_shots_in_box(int x, int y, unsigned char w, unsigned char h) {
	static unsigned char n;
	static actor *sht;

	if (!shot_count) return 0;
	// Things hanging over the left or top edges cannot be hit.
	if (x < 0 || x > 255 || y < -8 || y > 191) return 0;

	// A shot is 8 pixels wide and 16 pixels tall.
	shot_hit_w = w + 8;
	shot_hit_h = h + 16;
	n = find_shot(x - 8, y - 16);
	if (!n) return 0;

	n--;
	sht = shot_actors[n];
	shot_count--;
	if (n != shot_count) {
		shot_actors[n] = shot_actors[shot_count];
		shot_xy[n << 1] = shot_xy[shot_count << 1];
		shot_xy[(n << 1) + 1] = shot_xy[(shot_count << 1) + 1];
	}
	return sht;
}

actor *check_collision_against_shots(actor *act) {
	return check_shots_in_box(act->x, act->y, act->pixel_w, act->pixel_h);
}

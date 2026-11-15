#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "lib/SMSlib.h"
#include "actor.h"

/* SMSlib's sprite table, filled directly by draw_actor(). */
extern unsigned char SpriteTableY[];
extern unsigned char SpriteTableXN[];
extern unsigned char SpriteNextFree;

void draw_meta_sprite(int x, int y, int w, int h, unsigned char tile) {
	static char i, j;
	static int sx, sy;
	static unsigned char st;

	sy = y;
	st = tile;
	for (i = h; i; i--) {
		/* Sprites may hang over the top edge, but not over the bottom one. */
		if (sy > -16 && sy < SCREEN_H) {
			sx = x;
			for (j = w; j; j--) {
				if (sx >= 0 && sx < SCREEN_W) {
					SMS_addSprite(sx, (unsigned char) sy, st);
				}
				sx += 8;
				st += 2;
			}
		} else {
			st += w << 1;
		}
		sy += 16;
	}
}

void init_actor(actor *act, int x, int y, int char_w, int char_h, unsigned char base_tile, unsigned char frame_count) {
	static actor *sa;
	static unsigned char increment, max, n;
	sa = act;

	sa->active = 1;

	sa->x = x;
	sa->y = y;

	sa->char_w = char_w;
	sa->char_h = char_h;
	sa->pixel_w = char_w << 3;
	sa->pixel_h = char_h << 4;

	sa->animation_delay = 0;
	sa->animation_delay_max = 2;

	sa->base_tile = base_tile;
	sa->frame_count = frame_count;
	sa->frame = 0;
	// (written with shifts and additions: multiplying is slow on a Z80)
	increment = char_w << 1;
	if (char_h != 1) increment *= char_h;
	sa->frame_increment = increment;
	for (max = 0, n = frame_count; n; n--) max += increment;
	sa->frame_max = max;

	sa->path_flags = 0;
	sa->path = 0;
	sa->curr_step = 0;

	sa->state = 0;
	sa->state_timer = 256;

	sa->damage = 1;
}

/*
 * The two routines below run for every object, every frame, so they are
 * written in assembly. They use these offsets into the actor structure:
 */
#define CHECK_OFFSET(field, offset) typedef char check_actor_##field[(offsetof(actor, field) == (offset)) ? 1 : -1]
CHECK_OFFSET(active, 0);
CHECK_OFFSET(x, 1);
CHECK_OFFSET(y, 3);
CHECK_OFFSET(char_w, 5);
CHECK_OFFSET(char_h, 6);
CHECK_OFFSET(pixel_w, 7);
CHECK_OFFSET(animation_delay, 9);
CHECK_OFFSET(animation_delay_max, 10);
CHECK_OFFSET(base_tile, 11);
CHECK_OFFSET(frame, 13);
CHECK_OFFSET(frame_increment, 14);
CHECK_OFFSET(frame_max, 15);
CHECK_OFFSET(path_flags, 16);
CHECK_OFFSET(curr_step, 19);
CHECK_OFFSET(state_timer, 22);

/*
 * Moves an actor one step along its path (two with PATH_2X_SPEED) and counts
 * its state_timer down. In C it would read:
 *
 *	if (!act->active) return;
 *	if (act->path) {
 *		step = act->curr_step ? act->curr_step : act->path;
 *		if (step->x == PATH_END) step = act->path;
 *		act->x += (act->path_flags & PATH_FLIP_X) ? -step->x : step->x;
 *		act->y += (act->path_flags & PATH_FLIP_Y) ? -step->y : step->y;
 *		act->curr_step = step + 1;
 *	}
 *	if (act->state_timer) act->state_timer--;
 */
void move_actor(actor *act) __naked {
	act;
__asm
	ld	a, (hl)
	or	a, a
	ret	Z
	push	hl
	pop	iy

	ld	a, 17 (iy)
	or	a, 18 (iy)
	jr	Z, 00152$
	ld	l, 19 (iy)
	ld	h, 20 (iy)
	ld	a, h
	or	a, l
	jr	NZ, 00151$
	ld	l, 17 (iy)
	ld	h, 18 (iy)
00151$:
	call	00110$
	bit	2, 16 (iy)
	call	NZ, 00110$
	ld	19 (iy), l
	ld	20 (iy), h

00152$:
	ld	l, 22 (iy)
	ld	h, 23 (iy)
	ld	a, h
	or	a, l
	ret	Z
	dec	hl
	ld	22 (iy), l
	ld	23 (iy), h
	ret

	; One path step. HL = step pointer on entry, the next one on exit.
00110$:
	ld	a, (hl)
	cp	a, #0x80
	jr	NZ, 00111$
	ld	l, 17 (iy)
	ld	h, 18 (iy)
	ld	a, (hl)
00111$:
	inc	hl
	bit	0, 16 (iy)
	jr	Z, 00112$
	neg
00112$:
	ld	e, a
	add	a, a
	sbc	a, a
	ld	d, a
	ld	a, 1 (iy)
	add	a, e
	ld	1 (iy), a
	ld	a, 2 (iy)
	adc	a, d
	ld	2 (iy), a

	ld	a, (hl)
	inc	hl
	bit	1, 16 (iy)
	jr	Z, 00113$
	neg
00113$:
	ld	e, a
	add	a, a
	sbc	a, a
	ld	d, a
	ld	a, 3 (iy)
	add	a, e
	ld	3 (iy), a
	ld	a, 4 (iy)
	adc	a, d
	ld	4 (iy), a
	ret
__endasm;
}

/* The straightforward version, used for actors more than one row tall. */
void draw_tall_actor(actor *act) {
	draw_meta_sprite(act->x, act->y, act->char_w, act->char_h, act->base_tile + act->frame);
}

/*
 * Adds the sprites of an actor to the sprite table (straight into SMSlib's
 * buffers) and advances its animation. In C it would read:
 *
 *	if (!act->active) return;
 *	draw_meta_sprite(act->x, act->y, act->char_w, act->char_h, act->base_tile + act->frame);
 *	if (act->animation_delay) {
 *		act->animation_delay--;
 *	} else {
 *		act->frame += act->frame_increment;
 *		if (act->frame >= act->frame_max) act->frame = 0;
 *		act->animation_delay = act->animation_delay_max;
 *	}
 */
void draw_actor(actor *act) __naked {
	act;
__asm
	ld	a, (hl)
	or	a, a
	ret	Z
	push	hl
	pop	iy

	; More than one row of sprites: let the C version do it.
	ld	a, 6 (iy)
	dec	a
	jr	Z, 00201$
	push	iy
	call	_draw_tall_actor
	pop	iy
	jp	00230$

00201$:
	; Vertical clipping: visible when -16 < y < 192.
	ld	a, 4 (iy)
	or	a, a
	jr	Z, 00202$
	inc	a
	jp	NZ, 00230$
	ld	a, 3 (iy)
	cp	a, #0xF1
	jp	C, 00230$
	jr	00203$
00202$:
	ld	a, 3 (iy)
	cp	a, #192
	jp	NC, 00230$
00203$:
	dec	a
	ld	d, a			; D = Y, as the sprite table wants it
	ld	a, 11 (iy)
	add	a, 13 (iy)
	ld	c, a			; C = tile
	ld	b, 5 (iy)		; B = columns
	ld	l, 1 (iy)
	ld	h, 2 (iy)		; HL = X

	; Entirely inside the screen, horizontally?
	ld	a, h
	or	a, a
	jr	NZ, 00220$
	ld	a, 7 (iy)
	dec	a
	add	a, l
	jr	C, 00220$

	; Fast path: reserve B sprites and fill them in.
	ld	a, (_SpriteNextFree)
	ld	e, a
	add	a, b
	cp	a, #65
	jr	NC, 00230$
	ld	(_SpriteNextFree), a
	push	hl
	ld	hl, #_SpriteTableY
	ld	a, e
	add	a, l
	ld	l, a
	jr	NC, 00210$
	inc	h
00210$:
	ld	a, b
00211$:
	ld	(hl), d
	inc	hl
	dec	a
	jr	NZ, 00211$
	ld	hl, #_SpriteTableXN
	ld	a, e
	add	a, a
	add	a, l
	ld	l, a
	jr	NC, 00212$
	inc	h
00212$:
	pop	de
00213$:
	ld	(hl), e
	inc	hl
	ld	(hl), c
	inc	hl
	ld	a, e
	add	a, #8
	ld	e, a
	inc	c
	inc	c
	djnz	00213$
	jr	00230$

	; Slow path: partly outside, column by column.
00220$:
	ld	a, h
	or	a, a
	jr	NZ, 00222$
	ld	a, (_SpriteNextFree)
	cp	a, #64
	jr	NC, 00230$
	push	hl
	ld	e, l
	push	de
	ld	l, a
	ld	h, #0
	ld	de, #_SpriteTableY
	add	hl, de
	pop	de
	ld	(hl), d
	ld	l, a
	ld	h, #0
	add	hl, hl
	push	de
	ld	de, #_SpriteTableXN
	add	hl, de
	pop	de
	ld	(hl), e
	inc	hl
	ld	(hl), c
	inc	a
	ld	(_SpriteNextFree), a
	pop	hl
00222$:
	inc	c
	inc	c
	ld	a, l
	add	a, #8
	ld	l, a
	jr	NC, 00223$
	inc	h
00223$:
	djnz	00220$

	; Animation
00230$:
	ld	a, 9 (iy)
	or	a, a
	jr	Z, 00231$
	dec	9 (iy)
	ret
00231$:
	ld	a, 13 (iy)
	add	a, 14 (iy)
	cp	a, 15 (iy)
	jr	C, 00232$
	xor	a, a
00232$:
	ld	13 (iy), a
	ld	a, 10 (iy)
	ld	9 (iy), a
	ret
__endasm;
}

void wait_frames(int wait_time) {
	for (; wait_time; wait_time--) SMS_waitForVBlank();
}

void clear_sprites(void) {
	SMS_initSprites();
	SMS_finalizeSprites();
	SMS_copySpritestoSAT();
}

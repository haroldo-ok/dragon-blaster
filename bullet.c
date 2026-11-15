#include <stdlib.h>
#include "lib/SMSlib.h"
#include "actor.h"
#include "bullet.h"
#include "game.h"
#include "gen/assets.h"

/* SMSlib's sprite table, filled directly by draw_bullets(). */
extern unsigned char SpriteTableY[];
extern unsigned char SpriteTableXN[];
extern unsigned char SpriteNextFree;

#define FOR_EACH_BULLET(b) b = bullets; for (char i = BULLET_MAX; i; i--, b++)

bullet bullets[BULLET_MAX];

static unsigned char spiral_angle;

/* 16 directions, 1.5 pixels per frame, starting downwards and turning clockwise. */
static const int dir_x[16] = {0, -147, -272, -355, -384, -355, -272, -147, 0, 147, 272, 355, 384, 355, 272, 147};
static const int dir_y[16] = {384, 355, 272, 147, 0, -147, -272, -355, -384, -355, -272, -147, 0, 147, 272, 355};

/* Sideways sway of the "weave" fire breath. */
const signed char weave_vx[8] = {0, 2, 3, 2, 0, -2, -3, -2};

const unsigned char bullet_tiles[] = {0, 84, 88, 62, 92};

void init_bullets(void) {
	clear_bullets();
	spiral_angle = 0;
}

void clear_bullets(void) {
	static bullet *b;

	FOR_EACH_BULLET(b) {
		b->active = 0;
	}
}

static void add_bullet(unsigned char kind, int x, int y, int vx, int vy) {
	static bullet *b;

	if (x < 8 || x > 247 || y < 0 || y > 180) return;

	FOR_EACH_BULLET(b) {
		if (!b->active) {
			b->active = kind;
			b->x = x << 8;
			b->y = y << 8;
			b->vx = vx;
			b->vy = vy;
			b->timer = 0;
			return;
		}
	}
}

static void add_aimed(int x, int y, int spread) {
	static int vx, vy;

	// Scale the vector towards the player down until it is 1..2 pixels per frame.
	vx = (player.x + 8 - x) << 3;
	vy = (player.y + 4 - y) << 3;
	while (vx > 0x200 || vx < -0x200 || vy > 0x200 || vy < -0x200) {
		vx >>= 1;
		vy >>= 1;
	}
	if (!vx && !vy) vy = 0x180;
	add_bullet(BULLET_ORB, x, y, vx + spread, vy);
}

void fire_pattern(unsigned char pattern, int x, int y) {
	static unsigned char n;

	x -= 4;		// bullets are 8 pixels wide
	y -= 8;

	switch (pattern) {
	case PATTERN_DOWN:
		add_bullet(BULLET_NEEDLE, x, y, 0, 0x280);
		break;

	case PATTERN_AIMED:
		add_aimed(x, y, 0);
		break;

	case PATTERN_AIMED3:
		add_aimed(x, y, 0);
		add_aimed(x, y, -0xA0);
		add_aimed(x, y, 0xA0);
		break;

	case PATTERN_SPREAD3:
		add_bullet(BULLET_ORB, x, y, 0, 0x200);
		add_bullet(BULLET_ORB, x, y, -0xC0, 0x1C0);
		add_bullet(BULLET_ORB, x, y, 0xC0, 0x1C0);
		break;

	case PATTERN_SPREAD5:
		add_bullet(BULLET_ORB, x, y, 0, 0x200);
		add_bullet(BULLET_ORB, x, y, -0xC0, 0x1C0);
		add_bullet(BULLET_ORB, x, y, 0xC0, 0x1C0);
		add_bullet(BULLET_ORB, x, y, -0x180, 0x140);
		add_bullet(BULLET_ORB, x, y, 0x180, 0x140);
		break;

	case PATTERN_RING:
		// Alternates between the two sets of 8 directions.
		spiral_angle ^= 1;
		for (n = spiral_angle & 1; n < 16; n += 2) {
			add_bullet(BULLET_SHARD, x, y, dir_x[n], dir_y[n]);
		}
		break;

	case PATTERN_SPIRAL:
		spiral_angle = (spiral_angle + 3) & 0x0F;
		add_bullet(BULLET_SHARD, x, y, dir_x[spiral_angle], dir_y[spiral_angle]);
		add_bullet(BULLET_SHARD, x, y, dir_x[spiral_angle ^ 8], dir_y[spiral_angle ^ 8]);
		break;

	case PATTERN_WEAVE:
		add_bullet(BULLET_FIRE, x, y, 0, 0x200);
		break;

	case PATTERN_RAIN:
		add_bullet(BULLET_NEEDLE, 16 + (rand() % 220), 0, 0, 0x200 + (rand() & 0xFF));
		break;

	case PATTERN_SIDE:
		add_bullet(BULLET_ORB, x, y, -0x180, 0x180);
		add_bullet(BULLET_ORB, x, y, 0x180, 0x180);
		break;

	case PATTERN_CROSS:
		add_bullet(BULLET_SHARD, x, y, 0, 0x200);
		add_bullet(BULLET_SHARD, x, y, -0x200, 0);
		add_bullet(BULLET_SHARD, x, y, 0x200, 0);
		add_bullet(BULLET_SHARD, x, y, 0, -0x200);
		break;
	}
}

typedef char check_bullet_size[(sizeof(bullet) == 10) ? 1 : -1];

/*
 * Moves every bullet, checks it against the dragon and adds its sprite, all
 * in one pass (this is the busiest loop of a boss fight). In C it would read:
 *
 *	FOR_EACH_BULLET(b) {
 *		if (!b->active) continue;
 *		b->timer++;
 *		if (b->active == BULLET_FIRE) b->vx = weave_vx[(b->timer >> 2) & 7] * 256;
 *		b->x += b->vx;
 *		b->y += b->vy;
 *		bx = b->x >> 8;
 *		by = b->y >> 8;
 *		if (bx < 8 || bx > 248 || by > 184) {
 *			b->active = 0;
 *		} else if (bx is within 5 pixels of the dragon's body && by too && hurt_player()) {
 *			b->active = 0;
 *		} else {
 *			SMS_addSprite(bx, by, tile for its kind and timer);
 *		}
 *	}
 */
void draw_bullets(void) __naked {
__asm
	ld	hl, #_bullets
	ld	b, #BULLET_MAX
00151$:
	ld	a, (hl)
	or	a, a
	jp	Z, 00170$
	push	bc
	push	hl
	push	hl
	pop	iy

	inc	9 (iy)
	cp	a, #4
	jr	NZ, 00152$
	; Fire breath sways from side to side
	ld	a, 9 (iy)
	rrca
	rrca
	and	a, #7
	ld	e, a
	ld	d, #0
	ld	hl, #_weave_vx
	add	hl, de
	ld	a, (hl)
	ld	6 (iy), a
	ld	5 (iy), #0
00152$:
	ld	l, 1 (iy)
	ld	h, 2 (iy)
	ld	e, 5 (iy)
	ld	d, 6 (iy)
	add	hl, de
	ld	1 (iy), l
	ld	2 (iy), h
	ld	b, h			; B = x
	ld	l, 3 (iy)
	ld	h, 4 (iy)
	ld	e, 7 (iy)
	ld	d, 8 (iy)
	add	hl, de
	ld	3 (iy), l
	ld	4 (iy), h		; H = y

	; Left the screen?
	ld	a, h
	cp	a, #185
	jr	NC, 00160$
	ld	a, b
	cp	a, #8
	jr	C, 00160$
	cp	a, #249
	jr	NC, 00160$

	; Hit the dragon? Its body is a 10x10 box in the middle of the sprite.
	ld	a, (_player + 1)
	ld	e, a
	ld	a, b
	sub	a, e
	sub	a, #3
	cp	a, #10
	jr	NC, 00161$
	ld	a, (_player + 3)
	ld	e, a
	ld	a, h
	sub	a, e
	add	a, #5
	cp	a, #10
	jr	NC, 00161$
	push	iy
	call	_hurt_player
	pop	iy
	or	a, a
	jr	Z, 00161$
00160$:
	ld	0 (iy), #0
	jr	00169$

00161$:
	; Which tile?
	ld	a, 0 (iy)
	ld	e, a
	ld	d, #0
	ld	hl, #_bullet_tiles
	add	hl, de
	ld	d, (hl)
	dec	a
	jr	NZ, 00162$
	bit	2, 9 (iy)
	jr	Z, 00163$
	inc	d
	inc	d
	jr	00163$
00162$:
	cp	a, #3
	jr	NZ, 00163$
	ld	a, 9 (iy)
	rrca
	and	a, #6
	add	a, d
	ld	d, a
00163$:
	; Straight into the sprite table of SMSlib
	ld	a, (_SpriteNextFree)
	cp	a, #64
	jr	NC, 00169$
	ld	e, a
	inc	a
	ld	(_SpriteNextFree), a
	ld	hl, #_SpriteTableY
	ld	a, e
	add	a, l
	ld	l, a
	jr	NC, 00164$
	inc	h
00164$:
	ld	a, 4 (iy)
	dec	a
	ld	(hl), a
	ld	hl, #_SpriteTableXN
	ld	a, e
	add	a, a
	add	a, l
	ld	l, a
	jr	NC, 00165$
	inc	h
00165$:
	ld	a, 2 (iy)
	ld	(hl), a
	inc	hl
	ld	(hl), d

00169$:
	pop	hl
	pop	bc
00170$:
	ld	de, #10
	add	hl, de
	dec	b
	jp	NZ, 00151$
	ret
__endasm;
}

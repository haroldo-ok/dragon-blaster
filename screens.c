#include <stdlib.h>
#include <string.h>
#include "lib/SMSlib.h"
#include "actor.h"
#include "screens.h"
#include "sound.h"
#include "score.h"
#include "game.h"
#include "gen/assets.h"

/*
 * Text screens use the background layer:
 *   tiles   0..159  the sprite sheet (so sprites still work)
 *   tiles 160..255  the font
 *   tiles 256..447  one picture (see [picture ...] in data/game.ini)
 * The font is drawn with colour 6 of the palette: white on the background
 * palette, yellow (TEXT_YELLOW) when it borrows the sprite palette.
 */
#define FONT_TILE (160)
#define TEXT_WHITE (0)
#define TEXT_YELLOW (0x0800)

#define ANY_BUTTON (PORT_A_KEY_1 | PORT_A_KEY_2)

#define STORY_BUFFER_SIZE (420)

extern const unsigned char devkitSMS_font__tiles__1bpp[];

/*
 * Cheat codes: eight presses of the d-pad on the title screen
 * (U = up, D = down, L = left, R = right).
 */
#define CHEAT_CODE_LENGTH (8)

typedef struct cheat_code {
	const char *code;
	unsigned char flag;
	const char *message;
} cheat_code;

static const cheat_code cheat_codes[] = {
	{"UUDDLRLR", CHEAT_LIVES, "    NINE LIVES    "},
	{"LLRRUDUD", CHEAT_INVINCIBLE, "  DRAGON SCALES   "},
	{"DDUURLRL", CHEAT_STAGE_SELECT, "   STAGE SELECT   "},
	{"UDUDLLRR", CHEAT_POWER, "   ELDER BREATH   "},
	{0, 0, 0}
};

unsigned char cheat_flags;
unsigned char start_stage;

static char cheat_buffer[CHEAT_CODE_LENGTH];
static char story_buffer[STORY_BUFFER_SIZE];
static actor mascot;

static void text_screen_begin(void) {
	SMS_displayOff();
	SMS_waitForVBlank();
	clear_sprites();
	SMS_setBGScrollX(0);
	SMS_setBGScrollY(0);

	SMS_mapROMBank(sprites_tiles_bank);
	SMS_loadTiles(sprites_tiles, 0, FONT_TILE * 32);
	SMS_loadSpritePalette(sprites_palette);
	SMS_setSpritePaletteColor(0, 0);
	SMS_load1bppTiles(devkitSMS_font__tiles__1bpp, FONT_TILE, 96 * 8, 0, 6);

	clear_tilemap();
	SMS_zeroBGPalette();
	SMS_setBGPaletteColor(6, 0x3F);
}

static void show_picture(unsigned char picture, unsigned char x, unsigned char y) {
	static const picture_def *pic;

	pic = pictures + picture;
	load_picture(picture);
	draw_picture(picture, 0, x, y);
	SMS_mapROMBank(pic->bank);
	SMS_loadBGPalette(pic->palette);
	SMS_setBGPaletteColor(0, 0);
}

static void print_text(unsigned char x, unsigned char y, const char *text, unsigned int color) {
	SMS_setNextTileatXY(x, y);
	for (; *text; text++) {
		SMS_setTile((*text - 32 + FONT_TILE) | color);
	}
}

static void print_centered(unsigned char y, const char *text, unsigned int color) {
	print_text((33 - strlen(text)) >> 1, y, text, color);
}

/* Prints a score: the game counts in tens, so a zero is added at the end. */
static void print_score(unsigned char x, unsigned char y, unsigned int value, unsigned int color) {
	static char digits[6];
	static unsigned char n, i;

	n = number_to_digits(value, digits);
	SMS_setNextTileatXY(x, y);
	if (!value) n = 0;
	for (i = 6 - n; i > 1; i--) SMS_setTile(FONT_TILE | color);
	for (i = 0; i != n; i++) SMS_setTile((digits[i] + 16 + FONT_TILE) | color);
	SMS_setTile((16 + FONT_TILE) | color);
}

static void wait_button_release(void) {
	do {
		SMS_waitForVBlank();
	} while (SMS_getKeysStatus() & ANY_BUTTON);
}

/* Waits up to the given number of frames; returns 1 if a button was pressed. */
static char wait_or_button(unsigned int frames) {
	wait_button_release();
	for (; frames; frames--) {
		SMS_waitForVBlank();
		if (SMS_getKeysStatus() & ANY_BUTTON) {
			wait_button_release();
			return 1;
		}
	}
	return 0;
}

/* ------------------------------------------------------------------------ */

static void show_start_stage(void) {
	static char label[] = "START AT STAGE 0";

	label[15] = '1' + start_stage;
	print_centered(18, label, TEXT_WHITE);
}

/* Remembers the last d-pad presses and switches a cheat on when they spell its code. */
static void handle_cheat_input(unsigned char pressed) {
	static const cheat_code *cheat;
	static char key;

	if (pressed & PORT_A_KEY_UP) key = 'U';
	else if (pressed & PORT_A_KEY_DOWN) key = 'D';
	else if (pressed & PORT_A_KEY_LEFT) key = 'L';
	else if (pressed & PORT_A_KEY_RIGHT) key = 'R';
	else return;

	if (cheat_flags & CHEAT_STAGE_SELECT) {
		if (key == 'L' && start_stage) start_stage--;
		if (key == 'R' && start_stage != STAGE_COUNT - 1) start_stage++;
		show_start_stage();
	}

	memmove(cheat_buffer, cheat_buffer + 1, CHEAT_CODE_LENGTH - 1);
	cheat_buffer[CHEAT_CODE_LENGTH - 1] = key;

	for (cheat = cheat_codes; cheat->code; cheat++) {
		if (!memcmp(cheat_buffer, cheat->code, CHEAT_CODE_LENGTH)) {
			// Entering a code again switches the cheat off.
			cheat_flags ^= cheat->flag;
			print_centered(17, cheat->message, (cheat_flags & cheat->flag) ? TEXT_YELLOW : TEXT_WHITE);
			play_sfx((cheat_flags & cheat->flag) ? SFX_EXTRA_LIFE : SFX_SWITCH);
			cheat_buffer[CHEAT_CODE_LENGTH - 1] = 0;
			if (cheat->flag == CHEAT_STAGE_SELECT) {
				start_stage = 0;
				if (cheat_flags & CHEAT_STAGE_SELECT) {
					show_start_stage();
				} else {
					print_centered(18, "                ", TEXT_WHITE);
				}
			}
		}
	}
}

void title_screen(void) {
	static unsigned int timer;
	static unsigned char keys, previous_keys;

	game_state = STATE_TITLE;

	text_screen_begin();
	show_picture(PIC_TITLE_LOGO, 0, 2);

	print_centered(20, "HIGH SCORE", TEXT_WHITE);
	print_score(13, 21, hi_score, TEXT_YELLOW);
	print_centered(23, "2022-2026 HAROLDO-OK", TEXT_WHITE);
	if (cheat_flags & CHEAT_STAGE_SELECT) show_start_stage();
	memset(cheat_buffer, 0, CHEAT_CODE_LENGTH);

	init_actor(&mascot, 116, 108, 3, 1, 2, 4);
	mascot.animation_delay_max = 6;

	play_music(MUSIC_TITLE);
	SMS_displayOn();
	wait_button_release();

	previous_keys = 0xFF;
	for (timer = 0; ; timer++) {
		SMS_initSprites();
		// The dragon glides from side to side under the logo.
		mascot.x = 116 + ((timer & 0x80) ? 63 - (int) (timer & 0x7F) : (int) (timer & 0x7F) - 64);
		draw_actor(&mascot);
		SMS_finalizeSprites();
		SMS_waitForVBlank();
		SMS_copySpritestoSAT();

		if (!(timer & 0x1F)) {
			print_centered(16, (timer & 0x20) ? "                  " : "PRESS 1 OR 2 START", TEXT_YELLOW);
		}

		keys = SMS_getKeysStatus();
		handle_cheat_input(keys & ~previous_keys);
		previous_keys = keys;
		if (keys & ANY_BUTTON) break;
	}

	play_sfx(SFX_START);
	srand(timer);
	wait_button_release();
	clear_sprites();
}

/*
 * Shows a story made of pages (see data/text). Text is typed out; a button
 * finishes the page at once, and another press goes to the next page.
 */
void story_screen(const unsigned char *text, unsigned char bank, unsigned char music) {
	static const unsigned char *src;
	static char *ch;
	static unsigned int n;
	static unsigned char x, y, top, fast, picture, done;

	game_state = STATE_STORY;
	src = text;
	done = 0;

	play_music(music);

	while (!done) {
		// Copies the page to RAM, so that other banks can be used while it is shown.
		SMS_mapROMBank(bank);
		picture = 0;
		if (*src == 0x01) {
			picture = src[1];
			src += 2;
		}
		for (n = 0, ch = story_buffer; *src && *src != 0x0C && n != STORY_BUFFER_SIZE - 1; n++) {
			*ch++ = *src++;
		}
		*ch = 0;
		if (*src == 0x0C) {
			src++;
		} else {
			done = 1;
		}

		text_screen_begin();
		top = 4;
		if (picture) {
			picture--;
			show_picture(picture, (32 - pictures[picture].w) >> 1, 1);
			top = pictures[picture].h + 2;
		}
		SMS_displayOn();
		wait_button_release();

		fast = 0;
		x = 1;
		y = top;
		for (ch = story_buffer; *ch; ch++) {
			if (*ch == '\n') {
				x = 1;
				y++;
				continue;
			}
			if (!fast) {
				SMS_waitForVBlank();
				if (*ch != ' ') {
					SMS_waitForVBlank();
					if (!(x & 0x01)) play_sfx(SFX_TEXT);
				}
				if (SMS_getKeysStatus() & ANY_BUTTON) fast = 1;
			}
			SMS_setNextTileatXY(x + 1, y);
			SMS_setTile(*ch - 32 + FONT_TILE);
			x++;
		}

		wait_or_button(done ? 600 : 420);
	}
}

void stage_card_screen(void) {
	static char label[] = "STAGE 0";
	static char lives_label[] = "x 0";

	game_state = STATE_STAGE_CARD;
	text_screen_begin();

	label[6] = '1' + stage_number;
	print_centered(8, label, TEXT_YELLOW);
	print_centered(11, stages[stage_number].name, TEXT_WHITE);

	init_actor(&mascot, 100, 124, 3, 1, 2, 4);
	SMS_initSprites();
	draw_actor(&mascot);
	SMS_finalizeSprites();
	SMS_copySpritestoSAT();
	lives_label[2] = '0' + lives;
	print_text(16, 16, lives_label, TEXT_WHITE);

	SMS_displayOn();
	wait_or_button(150);
	clear_sprites();
}

void stage_clear_screen(unsigned int stage_bonus, unsigned int no_miss_bonus) {
	static char label[] = "STAGE 0 CLEAR!";

	game_state = STATE_STAGE_CLEAR;
	text_screen_begin();

	label[6] = '1' + stage_number;
	print_centered(6, label, TEXT_YELLOW);

	print_text(7, 10, "STAGE BONUS", TEXT_WHITE);
	print_score(20, 10, stage_bonus, TEXT_WHITE);
	if (no_miss_bonus) {
		print_text(7, 12, "NO MISS BONUS", TEXT_WHITE);
		print_score(20, 12, no_miss_bonus, TEXT_YELLOW);
	}
	print_text(7, 16, "SCORE", TEXT_WHITE);
	print_score(20, 16, score.value, TEXT_WHITE);

	SMS_displayOn();
	wait_or_button(300);
}

char game_over_screen(void) {
	static unsigned char count, frame;
	static unsigned char keys;

	game_state = STATE_GAME_OVER;
	text_screen_begin();

	print_centered(7, "GAME OVER", TEXT_YELLOW);
	print_text(9, 11, "SCORE", TEXT_WHITE);
	print_score(16, 11, score.value, TEXT_WHITE);
	print_centered(16, "CONTINUE?", TEXT_WHITE);
	print_centered(20, "PRESS 1 OR 2", TEXT_WHITE);

	play_music(MUSIC_GAME_OVER);
	SMS_displayOn();
	wait_button_release();

	for (count = 9; count != 0xFF; count--) {
		SMS_setNextTileatXY(16, 18);
		SMS_setTile((count + 16 + FONT_TILE) | TEXT_YELLOW);
		for (frame = 70; frame; frame--) {
			SMS_waitForVBlank();
			keys = SMS_getKeysStatus();
			if (keys & ANY_BUTTON) {
				play_sfx(SFX_START);
				wait_button_release();
				return 1;
			}
		}
	}

	return 0;
}

void the_end_screen(void) {
	text_screen_begin();

	print_centered(7, "THE END", TEXT_YELLOW);
	print_text(8, 12, "FINAL SCORE", TEXT_WHITE);
	print_score(20, 12, score.value, TEXT_YELLOW);
	print_centered(17, "THANK YOU FOR PLAYING!", TEXT_WHITE);

	init_actor(&mascot, 116, 76, 3, 1, 2, 4);
	SMS_initSprites();
	draw_actor(&mascot);
	SMS_finalizeSprites();
	SMS_copySpritestoSAT();

	SMS_displayOn();
	wait_or_button(900);
	clear_sprites();
}

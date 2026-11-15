#include "lib/SMSlib.h"
#include "lib/PSGlib.h"
#include "sound.h"
#include "gen/assets.h"

#define NO_MUSIC (0xFF)

unsigned char current_music;

static unsigned char music_bank;
static unsigned char sfx_priority;
static unsigned char sound_paused;

/*
 * Runs once per frame, from the line interrupt, so the music keeps its tempo
 * even when the game slows down. Music lives in a ROM bank that is only
 * mapped while the player routine runs; sound effects are in fixed ROM.
 */
static void sound_interrupt(void) {
	static unsigned char saved_bank;

	if (sound_paused) return;

	saved_bank = SMS_getROMBank();
	SMS_mapROMBank(music_bank);
	PSGFrame();
	SMS_mapROMBank(saved_bank);

	PSGSFXFrame();
}

void init_sound(void) {
	current_music = NO_MUSIC;
	music_bank = GAME_BANK;
	sfx_priority = 0;
	sound_paused = 0;

	SMS_setLineInterruptHandler(&sound_interrupt);
	SMS_setLineCounter(180);
	SMS_enableLineInterrupt();
}

void play_music(unsigned char music) {
	static const music_def *def;
	static unsigned char saved_bank;

	if (music == current_music) return;

	def = musics + music;
	__asm__("di");
	saved_bank = SMS_getROMBank();
	music_bank = def->bank;
	SMS_mapROMBank(music_bank);
	if (def->loop) {
		PSGPlay(def->data);
	} else {
		PSGPlayNoRepeat(def->data);
	}
	SMS_mapROMBank(saved_bank);
	current_music = music;
	__asm__("ei");
}

void stop_music(void) {
	__asm__("di");
	PSGStop();
	current_music = NO_MUSIC;
	__asm__("ei");
}

void play_sfx(unsigned char sfx) {
	static const sfx_def *def;

	def = sfxs + sfx;
	// A quiet effect never cuts off a more important one.
	if (PSGSFXGetStatus() && def->priority < sfx_priority) return;

	__asm__("di");
	sfx_priority = def->priority;
	PSGSFXPlay(def->data, def->channels);
	__asm__("ei");
}

void pause_sound(char paused) {
	if (paused) {
		sound_paused = 1;
		PSGSilenceChannels();
	} else {
		PSGRestoreVolumes();
		sound_paused = 0;
	}
}

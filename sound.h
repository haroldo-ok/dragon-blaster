#ifndef SOUND_H
#define SOUND_H

/* Music and sound effect numbers (MUSIC_xxx, SFX_xxx) come from gen/assets.h. */

void init_sound(void);
void play_music(unsigned char music);
void stop_music(void);
void play_sfx(unsigned char sfx);
void pause_sound(char paused);

extern unsigned char current_music;

#endif /* SOUND_H */

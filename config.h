#ifndef CONFIG_H
#define CONFIG_H

/*
 * Gameplay tuning knobs. Change a number, run "make", play.
 * Enemies, waves, bosses and stages live in data/game.ini instead.
 */

/* --- Player ------------------------------------------------------------- */
#define START_LIVES          (3)
#define MAX_LIVES            (9)
#define PLAYER_SPEED         (3)
#define INVINCIBLE_FRAMES    (150)    /* after losing a life */
#define RESPAWN_FRAMES       (50)     /* time the explosion plays before coming back */
#define EXTRA_LIFE_EVERY     (3000)   /* in tens of points: 3000 = every 30,000 points */

/* --- Limits (sprites on screen; the console shows 64 at most) ------------ */
#define PLAYER_SHOT_MAX      (12)
#define ENEMY_MAX            (6)
#define BULLET_MAX           (12)
#define POWERUP_MAX          (3)
#define EXPLOSION_MAX        (3)
#define SPAWNER_MAX          (3)      /* waves that can be entering at the same time */

/* --- Scoring (in tens of points) ------------------------------------------- */
#define POWERUP_SCORE        (20)
#define GEM_SCORE            (50)
#define WAVE_BONUS_SCORE     (30)     /* shooting down every enemy of a wave */
#define NO_MISS_BONUS        (200)    /* clearing a stage without losing a life */

/* Every Nth completely destroyed wave drops a power-up instead of a gem. */
#define POWERUP_EVERY_WAVES  (3)

/* --- Debug helpers, compiled in while you work on a stage --------------------
 * (players get the same things with the cheat codes of the title screen;
 *  those are listed in screens.c) */
/* #define DEBUG_INVINCIBLE */
/* #define DEBUG_START_STAGE (4) */   /* 0 = first stage */

#endif /* CONFIG_H */

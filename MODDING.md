# Modding Dragon Blaster

Almost everything in the game is described by files under `data/`. Change a
file, run `make`, and the new ROM has your changes. The asset pipeline checks
what it reads and stops with a plain message when something is wrong.

| I want to...                         | Edit                                  |
|---------------------------------------|---------------------------------------|
| change lives, speeds, limits          | `config.h`                            |
| change the cheat codes                | `cheat_codes` in `screens.c`          |
| change or add enemies, waves, bosses, stages | `data/game.ini`                |
| edit a level                          | `data/map/*.tmx` with [Tiled](https://www.mapeditor.org) |
| change how enemies fly                | `data/path/*.path.txt`                |
| redraw sprites, scenery, bosses       | `data/img/*.png`                      |
| change the music                      | `data/music/*.mml`                    |
| change the sound effects              | `data/sfx/*.sfx`                      |
| rewrite the story                     | `data/text/intro.txt`, `ending.txt`   |
| change the breath weapons             | `shot.c`                              |

To jump straight to the stage you are working on, uncomment
`DEBUG_START_STAGE` (and maybe `DEBUG_INVINCIBLE`) in `config.h`, or use the
cheat codes of the title screen (listed in `README.md`). The codes themselves
are the `cheat_codes` table in `screens.c`: change them or add your own.

## data/game.ini

The file is commented section by section. In short:

* `[picture name]`: a big image for the background layer (bosses, title,
  story pages). Animated pictures stack their frames vertically.
* `[enemy name]`: sprite, hit points, score, what it shoots and how often.
* `[wave name]`: *count* enemies of one kind following one path. The wave's
  `id` is the tile used to place it in Tiled.
* `[boss name]`: picture, hit points, hit box, how it moves and its lists of
  attacks (normal and enraged).
* `[stage N]`: name, tileset, map, music, boss. Stages are played in order;
  add `[stage 6]` and the game has six stages.

Shot patterns, for enemies and bosses alike: `down`, `aimed`, `aimed3`,
`spread3`, `spread5`, `ring`, `side`, `cross`, `weave`, `spiral`, `rain`.
New patterns are added in `fire_pattern()` (`bullet.c`) and in the `PATTERNS`
list at the top of `tool/build_assets.py`.

## Levels

Open `data/map/levelN.tmx` in Tiled. A level is 16 tiles wide and as tall as
you like; the game starts at the **bottom** row and scrolls up, 16 frames per
row at the default speed. It ends at the top row, where the boss appears, so
keep the last 14 rows free of waves.

* The **Map** layer uses the stage tileset (`tileset_<name>.tsx`).
* The **Objects** layer uses `objects.tsx`: drop a tile from it where a wave
  should enter (the number is how many enemies it has) or where an item
  should appear. Waves placed on the right half of the screen fly their path
  mirrored. The object's row decides *when* it shows up.

`objects.png` is redrawn by every build from the waves in `game.ini`, so new
waves show up in Tiled after a `make`.

Save maps in the CSV layer format. A new level only needs a new `.tmx` and a
`map = ` line in a stage.

## Paths

`data/path/<name>.path.txt` is a list of waypoints, `x y` in pixels relative
to the point where the enemy enters. The enemy follows a smooth curve through
them at a constant speed.

```
speed 2        # pixels per frame from here on
0 0
0 56
wait 100       # stay still for 100 frames
speed 3
0 250
```

`corner` makes a sharp turn at the previous waypoint. When a path ends it
starts again from where the enemy is, so finish it outside the screen.
Files made with the Cardinal Spline editor (`.spline.json`) work too.

## Graphics

All images are indexed PNGs with at most 16 colours, using only the 64 colours
of the Master System (each of red, green and blue is 0, 85, 170 or 255).
Palette entry 0 is transparent / the background colour.

* `sprites.png` (256x64): every sprite, in 8x16 cells. One 16-colour palette
  for all of them. Enemies are 16 or 24 pixels wide and 16 tall, with their
  animation frames side by side.
* `tileset_<name>.png` (256x48): 16x3 scenery tiles of 16x16 pixels, with
  their own palette.
* Pictures (bosses, `title_logo.png`, `dracolich.png`): any size up to 192
  different 8x8 tiles, own palette. On story pages, palette entry 6 is also
  the colour of the text.

`data/img/original/` keeps the untouched jam assets. `tool/gen/make_art.py`
and `tool/gen/make_levels.py` are the scripts that produced the current art
and levels from them; they are not part of the build, and running them
overwrites the PNG and TMX files.

## Music

`data/music/<name>.mml` uses a small Music Macro Language; the full reference
is at the top of `tool/mml2psg.py`.

```
@tempo 150
@e1 = 15 14 13 12 11 10 9 9 8 8 7    ; volume envelope, one value per frame
A  o4 l8 v13 @e1  e4 g4 >c4 <b a | g2 e4 g4
B  o3 l8 v9  @e1  c e g e c e g e | c e g e c e g e
C  o3 l4 v11 @e1  c c g c | c c g c
D  l8 v10 @e1     c g e g c g e g | c g e g c g e g
```

Channels A, B and C are the three square waves, D is the noise (c = thump,
e = snare, g = hat). All channels must have the same length; the compiler
tells you when they do not. The lowest note is `o2 a`. Sound effects borrow
channel C and the noise channel, so keep the melody on A and B.

A stage can use any tune: `music = mytune` in `game.ini`. The game itself
needs `title`, `intro`, `boss`, `stage_clear`, `game_over` and `ending`.

## Sound effects

`data/sfx/<name>.sfx` describes a sound frame by frame (reference at the top
of `tool/mml2psg.py`):

```
channels 23            # 2 = tone, 3 = noise, 23 = both
priority 6             # louder events are not cut off by quieter ones
sweep 700 110 15 4 28 + nsweep 1 15 6 28
```

The game plays these by name (`play_sfx(SFX_EXPLOSION)`), so keep the
existing file names; add new ones freely for your own code.

## Story text

`data/text/intro.txt` and `ending.txt`: plain text, up to 30 characters per
line. A line of `----` starts a new page and `@picture name` puts a picture
above the page's text.

## Under the hood

* `tool/build_assets.py` writes `gen/tables.c` (the contents of `game.ini`),
  `gen/assets.h` (names and ROM banks of everything) and `gen/bankN.c` (the
  data itself, packed into 16 KB banks).
* Code and tables live in the first 32 KB of the ROM; the build prints how
  much is left and fails if it overflows.
* The enemy paths share bank 2, which stays mapped during play. Levels,
  graphics and music are mapped only while they are read.
* `make test` runs `test/playtest.txt`, which plays from the title screen to
  the ending using a few memory pokes to skip ahead.

# Dragon Blaster

A dragon themed shoot-em-up for the Sega Master System. Originally made for the
[SHMUP JAM 1](https://itch.io/jam/shmup-jam-1-dragons), now grown into a full game:

* 5 stages, each with its own scenery, music and enemy waves
* 5 animated bosses with several attack patterns and an enraged second phase
* 9 kinds of enemies, 9 breath weapons (3 elements and every combination of two)
* Intro story, stage cards, ending and credits
* Music and sound effects
* Lives, continues, extra lives, high score, pause
* Cheat codes
* Everything is data driven: see [MODDING.md](MODDING.md)

## How to play

| | |
|---|---|
| D-pad | fly |
| Button 2 | breathe (hold for autofire) |
| Button 1 | switch between your breath weapons / the combined one |
| Pause | pause |

Catch the elemental orbs (lightning, fire, wind). You carry two of them, and
both together make a stronger combined breath. Shooting down every enemy of a
wave leaves a gem, and every third perfect wave leaves an orb. Losing a life
costs you the second orb. An extra life is awarded every 30,000 points.

## Cheat codes

Enter them with the d-pad on the title screen. A sound and a message confirm
the code; entering it again switches the cheat off. Cheats stay on until the
console is reset, continues included.

| Code | Cheat | |
|---|---|---|
| Up Up Down Down Left Right Left Right | NINE LIVES | start (and continue) with 9 lives |
| Left Left Right Right Up Down Up Down | DRAGON SCALES | nothing hurts the dragon |
| Down Down Up Up Right Left Right Left | STAGE SELECT | then Left / Right on the title screen pick the first stage |
| Up Down Up Down Left Left Right Right | ELDER BREATH | start with lightning + fire, and keep both orbs after losing a life |

## Building

You need:

* [SDCC](https://sdcc.sourceforge.net/) 4.2 or newer
* GNU make
* Python 3 with [Pillow](https://python-pillow.org/) (`pip install pillow`)
* `ihx2sms`, from [devkitSMS](https://github.com/sverx/devkitSMS), on the PATH

SMSlib and PSGlib are bundled in `lib/` (header and library from the same
devkitSMS release; replace both together if you upgrade).

```
make          # builds dragon_blaster.sms
make test     # automated playtest, needs the smstest headless emulator
make clean
```

BMP2Tile, Node.js and the other external converters are no longer needed:
`tool/build_assets.py` converts every image, map, path, tune and text.

## Source layout

| | |
|---|---|
| `dragon_blaster.c` | main loop, player, power-ups, status display |
| `actor.c` | moving and drawing objects (the two hot routines are in assembly) |
| `shot.c`, `shots.c` | the dragon's breath weapons |
| `enemy.c`, `bullet.c`, `boss.c` | enemies and waves, enemy bullets, bosses |
| `map.c` | scrolling level |
| `screens.c` | title, story, stage cards, game over |
| `sound.c` | music and sound effects |
| `config.h` | gameplay tuning (lives, speeds, limits, debug switches) |
| `data/` | everything else: see [MODDING.md](MODDING.md) |
| `tool/` | the asset pipeline |
| `gen/` | generated C sources (not committed) |

## Art resources used on this project:

* Main character, enemy: https://opengameart.org/content/dragon-icon-pack
* Lightning: https://opengameart.org/content/space-war-man-platform-shmup-set
* Fire, wind, background: https://opengameart.org/content/dawnlike-16x16-universal-rogue-like-tileset-v181
* Bosses, from the [Stendhal](https://stendhalgame.org) project, licensed
  [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/) or later:
  * Dracolich (bone dragon), Emerald Wyrm (green dragon) and Frost Hydra
    (two-headed dragon): [Stendhal dragons](https://opengameart.org/content/stendhal-dragons),
    by Kimmo Rundelin (kiheru)
  * Infernal Reaper (golden death): [Stendhal Death](https://opengameart.org/content/stendhal-death),
    by Kimmo Rundelin (kiheru)
  * The Stendhal source sheets are in `data/img/original/stendhal/`; see the
    `CREDITS.txt` there.

The extra enemies, the Gazer and the stage tilesets were derived from those
same resources by `tool/gen/make_art.py`, which also converts the Stendhal
boss animations to Master System colours. The boss pictures are therefore
derivative works under CC BY-SA 3.0 as well.

## Tools used

* A spline editor, based on Cardinal Spline, was used for editing the original enemy path:
  * Available online at: http://www.haroldo-ok.com/cardinal-spline-js/demos/editor
  * Source code for the editor: https://github.com/haroldo-ok/cardinal-spline-js
* [Tiled](https://www.mapeditor.org) for the levels

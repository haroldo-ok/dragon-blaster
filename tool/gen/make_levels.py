#!/usr/bin/env python3
"""
One-off helper that wrote the five levels in data/map as Tiled maps.

It is NOT part of the normal build: the .tmx files are the source of truth and
are meant to be edited with Tiled (https://www.mapeditor.org). Run this only
if you want to throw the levels away and roll new ones:

    python3 tool/gen/make_levels.py

Level 1 starts with the terrain of the original game jam level.
"""
import configparser
import os
import random

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
MAPS = os.path.join(ROOT, 'data', 'map')

GROUND = [4, 20, 36]
# 9-slice, vertical strip, horizontal strip, single tile
MATERIALS = {
    'A': dict(nine=(0, 1, 2, 16, 17, 18, 32, 33, 34), vstrip=(3, 19, 35), hstrip=(12, 13, 14), single=15),
    'B': dict(nine=(5, 6, 7, 21, 22, 23, 37, 38, 39), vstrip=(8, 24, 40), hstrip=(28, 29, 30), single=31),
    'C': dict(nine=(9, 10, 11, 25, 26, 27, 41, 42, 43), vstrip=None, hstrip=None, single=44),
}

ITEM_IDS = {'lightning': 48, 'fire': 49, 'wind': 50, 'life': 51, 'gem': 52}

# The terrain of the original level (100 rows, top row first, 16 columns).
ORIGINAL_LEVEL1 = None      # filled in from data/map/original_level1.csv if present


def wave_ids():
    ini = configparser.ConfigParser(inline_comment_prefixes=(';',), interpolation=None)
    ini.read(os.path.join(ROOT, 'data', 'game.ini'))
    out = {}
    for sec in ini.sections():
        if sec.startswith('wave '):
            out[sec.split(None, 1)[1]] = int(ini[sec]['id'])
    return out


def terrain(rows, rnd, weights, density):
    grid = [[rnd.choices(GROUND, (86, 10, 4))[0] for _ in range(16)] for _ in range(rows)]
    used = [[False] * 16 for _ in range(rows)]

    def free(x, y, w, h):
        for yy in range(y - 1, y + h + 1):
            for xx in range(x - 1, x + w + 1):
                if 0 <= yy < rows and 0 <= xx < 16 and used[yy][xx]:
                    return False
        return True

    def claim(x, y, w, h):
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                used[yy][xx] = True

    target = int(rows * 16 * density)
    covered = 0
    tries = 0
    names = list(weights)
    while covered < target and tries < 4000:
        tries += 1
        mat = MATERIALS[rnd.choices(names, [weights[n] for n in names])[0]]
        kind = rnd.choices(('rect', 'vstrip', 'hstrip', 'single'), (70, 10, 10, 10))[0]
        if kind == 'rect':
            w, h = rnd.randint(2, 7), rnd.randint(2, 6)
        elif kind == 'vstrip':
            if not mat['vstrip']:
                continue
            w, h = 1, rnd.randint(2, 5)
        elif kind == 'hstrip':
            if not mat['hstrip']:
                continue
            w, h = rnd.randint(2, 6), 1
        else:
            w, h = 1, 1
        x, y = rnd.randint(0, 16 - w), rnd.randint(0, rows - h)
        if not free(x, y, w, h):
            continue
        claim(x, y, w, h)
        covered += w * h
        for yy in range(h):
            for xx in range(w):
                if kind == 'rect':
                    col = 0 if xx == 0 else (2 if xx == w - 1 else 1)
                    row = 0 if yy == 0 else (2 if yy == h - 1 else 1)
                    t = mat['nine'][row * 3 + col]
                elif kind == 'vstrip':
                    t = mat['vstrip'][0 if yy == 0 else (2 if yy == h - 1 else 1)]
                elif kind == 'hstrip':
                    t = mat['hstrip'][0 if xx == 0 else (2 if xx == w - 1 else 1)]
                else:
                    t = mat['single']
                grid[y + yy][x + xx] = t
    return grid


def write_tmx(name, grid, objects):
    rows = len(grid)
    lines = []
    for y, row in enumerate(grid):
        lines.append(','.join(str(t + 1) for t in row) + (',' if y < rows - 1 else ''))
    out = ['<?xml version="1.0" encoding="UTF-8"?>',
           '<map version="1.5" tiledversion="2021.03.23" orientation="orthogonal" renderorder="right-down" '
           'width="16" height="%d" tilewidth="16" tileheight="16" infinite="0" nextlayerid="4" nextobjectid="%d">'
           % (rows, len(objects) + 1),
           ' <tileset firstgid="1" source="tileset_%s.tsx"/>' % name[1],
           ' <tileset firstgid="49" source="objects.tsx"/>',
           ' <layer id="1" name="Map" width="16" height="%d">' % rows,
           '  <data encoding="csv">'] + lines + ['</data>', ' </layer>',
                                               ' <objectgroup id="3" name="Objects">']
    for i, (row_from_start, col, oid) in enumerate(objects):
        tmx_row = rows - 1 - row_from_start
        out.append('  <object id="%d" gid="%d" x="%d" y="%d" width="16" height="16"/>'
                   % (i + 1, 49 + oid, col * 16, (tmx_row + 1) * 16))
    out += [' </objectgroup>', '</map>', '']
    with open(os.path.join(MAPS, name[0] + '.tmx'), 'w', newline='\n') as f:
        f.write('\n'.join(out))
    with open(os.path.join(MAPS, 'tileset_%s.tsx' % name[1]), 'w', newline='\n') as f:
        f.write('<?xml version="1.0" encoding="UTF-8"?>\n'
                '<tileset version="1.5" tiledversion="2021.03.23" name="tileset_%s" tilewidth="16" '
                'tileheight="16" tilecount="48" columns="16">\n'
                ' <image source="../img/tileset_%s.png" width="256" height="48"/>\n'
                '</tileset>\n' % (name[1], name[1]))


# Paths that only make sense near the middle of the screen / near an edge.
EDGE_WAVES = {'imps_loop', 'bats_loop', 'bats_dive', 'bats_cross', 'gazers_cross', 'skulls_dive',
              'skulls_arc', 'frost_drakes_arc', 'bats_zigzag', 'wisps_uturn', 'fire_drake_swoop',
              'bone_drakes_swoop', 'wisps_circle'}


def populate(rows, rnd, ids, pool, gap_start, gap_end, items):
    """pool: list of (wave name, weight, first row fraction)."""
    objects = []
    last = rows - 16            # leave the end of the level empty for the boss
    row = 15
    while row < last:
        progress = (row - 15) / float(last - 15)
        choices = [(n, w) for n, w, start in pool if progress >= start]
        name = rnd.choices([c[0] for c in choices], [c[1] for c in choices])[0]
        right = rnd.random() < 0.5
        if name in EDGE_WAVES:
            col = rnd.randint(1, 3)
        elif name == 'tower':
            col = rnd.randint(1, 6)
        else:
            col = rnd.randint(2, 6)
        if right:
            col = 15 - col - (1 if name not in ('tower',) else 0)
        objects.append((row, col, ids[name]))
        # Now and then, a second wave comes in from the other side.
        if progress > 0.35 and rnd.random() < 0.3 + 0.3 * progress:
            other = rnd.choices([c[0] for c in choices], [c[1] for c in choices])[0]
            ocol = rnd.randint(1, 4)
            if not right:
                ocol = 14 - ocol
            objects.append((row + 2, ocol, ids[other]))
        gap = gap_start + (gap_end - gap_start) * progress
        row += max(3, int(round(gap + rnd.uniform(-1, 1))))
    for frac, col, item in items:
        objects.append((int(15 + frac * (last - 15)), col, ITEM_IDS[item]))
    return sorted(objects)


def original_level1_rows():
    path = os.path.join(MAPS, 'original_level1.csv')
    if not os.path.exists(path):
        return []
    rows = []
    with open(path) as f:
        for line in f:
            vals = [int(v) for v in line.strip().strip(',').split(',') if v.strip()]
            if len(vals) == 16:
                rows.append([v - 1 for v in vals])
    return rows


def main():
    ids = wave_ids()

    # ---- Stage 1: Emerald Plains -------------------------------------------------
    rnd = random.Random(101)
    original = original_level1_rows()
    grid = terrain(150, rnd, {'A': 60, 'B': 20, 'C': 20}, 0.30) + original
    objects = populate(len(grid), rnd, ids, [
        ('imps_loop', 30, 0), ('imps_line', 25, 0), ('imps_sway', 20, 0.1), ('bats_zigzag', 20, 0.2),
        ('bats_cross', 15, 0.3), ('wisps_sway', 12, 0.45), ('gazer_hover', 8, 0.6), ('tower', 8, 0.5),
        ('imps_fast', 10, 0.7), ('vampire_bats', 6, 0.45),
    ], 8, 5, [(0.08, 7, 'fire'), (0.3, 4, 'gem'), (0.45, 10, 'wind'), (0.7, 6, 'lightning'), (0.88, 8, 'gem')])
    write_tmx(('level1', 'plains'), grid, objects)

    # ---- Stage 2: Scorched Dunes ---------------------------------------------------
    rnd = random.Random(202)
    grid = terrain(270, rnd, {'A': 45, 'B': 40, 'C': 15}, 0.34)
    objects = populate(len(grid), rnd, ids, [
        ('bats_dive', 22, 0), ('imps_fast', 18, 0), ('wisps_sway', 18, 0), ('skulls_line', 16, 0.1),
        ('wisps_uturn', 14, 0.2), ('gazer_hover', 14, 0.25), ('tower', 14, 0.15), ('bats_loop', 14, 0.3),
        ('fire_drake_swoop', 12, 0.45), ('gazers_hover', 8, 0.7), ('vampire_bats', 8, 0.2),
        ('watchers', 6, 0.4), ('vampire_bats_sway', 6, 0.6),
    ], 7, 4.5, [(0.1, 5, 'wind'), (0.35, 9, 'fire'), (0.55, 3, 'gem'), (0.75, 11, 'lightning'), (0.9, 7, 'gem')])
    write_tmx(('level2', 'desert'), grid, objects)

    # ---- Stage 3: Frozen Peaks -------------------------------------------------------
    rnd = random.Random(303)
    grid = terrain(280, rnd, {'A': 45, 'B': 30, 'C': 25}, 0.36)
    objects = populate(len(grid), rnd, ids, [
        ('imps_sway', 16, 0), ('bats_cross', 18, 0), ('skulls_arc', 18, 0), ('frost_drake_hover', 14, 0.1),
        ('wisps_circle', 14, 0.15), ('tower', 12, 0.2), ('gazers_hover', 12, 0.3), ('frost_drakes_arc', 12, 0.5),
        ('bats_dive', 12, 0.4), ('gazers_cross', 8, 0.7), ('hellskulls', 8, 0.1),
        ('vampire_bats', 8, 0.3), ('watchers', 6, 0.5), ('hellskulls_arc', 6, 0.7),
    ], 6.5, 4, [(0.1, 8, 'lightning'), (0.3, 4, 'fire'), (0.5, 10, 'life'), (0.65, 6, 'wind'), (0.85, 9, 'gem')])
    write_tmx(('level3', 'frozen'), grid, objects)

    # ---- Stage 4: Fallen Fortress ------------------------------------------------------
    rnd = random.Random(404)
    grid = terrain(290, rnd, {'A': 55, 'B': 25, 'C': 20}, 0.42)
    objects = populate(len(grid), rnd, ids, [
        ('tower', 22, 0), ('skulls_dive', 18, 0), ('imps_fast', 14, 0), ('bats_dive', 14, 0),
        ('fire_drakes_line', 14, 0.1), ('wisps_uturn', 14, 0.1), ('gazers_cross', 12, 0.25),
        ('bone_drake_hover', 12, 0.4), ('fire_drake_swoop', 10, 0.3), ('gazers_hover', 10, 0.5),
        ('vampire_bats_sway', 8, 0.1), ('watchers_cross', 8, 0.3), ('hellskulls', 8, 0.45),
        ('watchers', 6, 0.6), ('hellskulls_arc', 6, 0.8),
    ], 6, 3.5, [(0.08, 6, 'fire'), (0.3, 10, 'wind'), (0.5, 4, 'gem'), (0.6, 8, 'lightning'), (0.8, 5, 'life')])
    write_tmx(('level4', 'fortress'), grid, objects)

    # ---- Stage 5: Lair of the Lich -------------------------------------------------------
    rnd = random.Random(505)
    grid = terrain(310, rnd, {'A': 50, 'B': 30, 'C': 20}, 0.45)
    objects = populate(len(grid), rnd, ids, [
        ('skulls_dive', 16, 0), ('bone_drake_hover', 14, 0), ('gazers_cross', 14, 0), ('bats_loop', 14, 0),
        ('wisps_circle', 14, 0), ('tower', 14, 0), ('frost_drakes_arc', 12, 0.15), ('fire_drakes_line', 12, 0.15),
        ('bone_drakes_swoop', 12, 0.4), ('skulls_arc', 12, 0.3), ('gazers_hover', 10, 0.5),
        ('hellskulls', 10, 0), ('vampire_bats_sway', 10, 0.2), ('watchers_cross', 10, 0.35),
        ('hellskulls_arc', 8, 0.5), ('watchers', 8, 0.65),
    ], 5.5, 3.5, [(0.06, 7, 'wind'), (0.25, 4, 'fire'), (0.45, 11, 'lightning'), (0.55, 6, 'life'),
                  (0.75, 9, 'fire'), (0.92, 7, 'life')])
    write_tmx(('level5', 'lair'), grid, objects)
    print('Levels written to', os.path.normpath(MAPS))


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""
Dragon Blaster asset pipeline.

Reads everything under data/ and writes C sources into gen/:

    data/game.ini            enemies, waves, bosses, stages    -> gen/tables.c
    data/img/*.png           sprites, tilesets, pictures       -> banked tiles
    data/map/*.tmx           levels made with Tiled            -> banked maps
    data/path/*.path.txt     enemy flight paths (waypoints)    -> banked paths
    data/path/*.spline.json  paths from the Cardinal Spline editor
    data/music/*.mml         music                             -> banked PSG
    data/sfx/*.sfx           sound effects                     -> gen/tables.c
    data/text/*.txt          story text                        -> banked text

It also refreshes data/map/objects.png, the tileset used to place enemy waves
and items inside Tiled.

Usage: python3 tool/build_assets.py [project_dir]
Requires Python 3.6+ and Pillow.
"""
import configparser
import glob
import json
import math
import os
import re
import struct
import sys
import xml.etree.ElementTree as ET

from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mml2psg  # noqa: E402

ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else
                       os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
DATA = os.path.join(ROOT, 'data')
GEN = os.path.join(ROOT, 'gen')

BANK_SIZE = 16384 - 64
FIRST_BANK = 2
PICTURE_BASE_TILE = 256   # pictures are always loaded at background tile 256...
PICTURE_MAIN_TILES = 192  # ...up to the end of the tile area (tile 447)
# During a boss fight these sprite tiles are not needed (they belong to the
# regular enemies), so a boss picture may use them as well.
# The game loads them back with the sprite sheet at the start of every stage.
BOSS_EXTRA_TILES = [(64, 20), (128, 64), (212, 24)]
BOSS_PICTURE_TILES = PICTURE_MAIN_TILES + sum(n for _, n in BOSS_EXTRA_TILES)
MAX_TOLERANCE = 16        # most pixels a reused tile may differ by (of 64)
WAVE_SLOTS = 48           # object ids 0..47 are waves, 48.. are items
ITEMS = ['lightning', 'fire', 'wind', 'life', 'gem']
PATTERNS = ['none', 'down', 'aimed', 'spread3', 'spread5', 'ring', 'weave',
            'spiral', 'rain', 'aimed3', 'side', 'cross']
MOVES = ['wander', 'sweep', 'chase']


class BuildError(Exception):
    pass


def fail(msg):
    raise BuildError(msg)


def c_name(name):
    return re.sub(r'[^A-Za-z0-9_]', '_', name)


# ---------------------------------------------------------------------------
# Images
# ---------------------------------------------------------------------------
def load_indexed(path):
    im = Image.open(path)
    if im.mode != 'P':
        fail('%s must be an indexed (palette) PNG with at most 16 colours' % path)
    used = max(im.tobytes())
    if used > 15:
        fail('%s uses palette entry %d; only entries 0..15 are allowed' % (path, used))
    if im.width % 8 or im.height % 8:
        fail('%s: width and height must be multiples of 8' % path)
    return im


def sms_palette(im):
    pal = im.getpalette() or []
    pal = (pal + [0] * 48)[:48]
    out = bytearray()
    for i in range(16):
        r, g, b = pal[i * 3:i * 3 + 3]
        out.append((r >> 6) | ((g >> 6) << 2) | ((b >> 6) << 4))
    return bytes(out)


def tile_bytes(px, x0, y0):
    """8x8 tile -> 32 bytes, 4 bitplanes per row."""
    out = bytearray()
    for y in range(8):
        planes = [0, 0, 0, 0]
        for x in range(8):
            c = px[x0 + x, y0 + y]
            for p in range(4):
                if c & (1 << p):
                    planes[p] |= 0x80 >> x
        out += bytes(planes)
    return bytes(out)


def flip_tile(t, h, v):
    rows = [t[i * 4:i * 4 + 4] for i in range(8)]
    if v:
        rows = rows[::-1]
    if h:
        rows = [bytes(int('{:08b}'.format(b)[::-1], 2) for b in r) for r in rows]
    return b''.join(rows)


def tiles_8x16(im):
    """Tiles in "tall sprite" order: each 8x16 cell is two consecutive tiles."""
    px = im.load()
    out = bytearray()
    for cy in range(0, im.height, 16):
        for cx in range(0, im.width, 8):
            out += tile_bytes(px, cx, cy)
            out += tile_bytes(px, cx, cy + 8)
    return bytes(out)


def tile_slot(i):
    """VRAM tile used for the i-th tile of a picture."""
    if i < PICTURE_MAIN_TILES:
        return PICTURE_BASE_TILE + i
    i -= PICTURE_MAIN_TILES
    for start, count in BOSS_EXTRA_TILES:
        if i < count:
            return start + i
        i -= count
    raise IndexError(i)


def picture(im, frames, key_frame=0, limit=PICTURE_MAIN_TILES, still=None):
    """Tiles + one tilemap per animation frame.

    Tiles are shared between frames, flipped copies included. When the
    animation needs more tiles than the limit, the frames other than the key
    frame may reuse a tile that differs from theirs by a few pixels: the
    smallest tolerance that fits is searched for automatically.
    Returns (tiles, tilemap, w, h, tile count, tolerance used)."""
    import numpy as np

    if im.height % frames or (im.height // frames) % 8:
        fail('picture height is not a multiple of 8 x frames')
    fh = im.height // frames
    if still is not None:
        if not 0 <= still < frames:
            fail('"still" must be a frame number from 0 to %d' % (frames - 1))
        im = im.crop((0, still * fh, im.width, (still + 1) * fh))
        frames, key_frame = 1, 0
    if not 0 <= key_frame < frames:
        fail('"key_frame" must be a frame number from 0 to %d' % (frames - 1))
    w, h = im.width // 8, fh // 8
    pixels = np.array(im, dtype=np.uint8)

    def tile_pixels(frame, tx, ty):
        y = frame * fh + ty * 8
        return pixels[y:y + 8, tx * 8:tx * 8 + 8]

    def encode(tolerance):
        tiles = []                      # 8x8 arrays
        stack = np.zeros((0, 4, 64), dtype=np.uint8)   # every tile in its 4 orientations
        maps = {}
        order = [key_frame] + [f for f in range(frames) if f != key_frame]
        for frame in order:
            entries = []
            for ty in range(h):
                for tx in range(w):
                    t = tile_pixels(frame, tx, ty)
                    entry = None
                    if len(tiles):
                        # differences against each tile, as is / h-flipped / v-flipped / both
                        diff = (stack != t.reshape(1, 1, 64)).sum(axis=2)
                        best = np.unravel_index(np.argmin(diff), diff.shape)
                        allowed = tolerance if frame != key_frame else 0
                        if diff[best] <= allowed:
                            orient = best[1]
                            entry = (best[0], orient & 1, orient >> 1)
                    if entry is None:
                        tiles.append(t)
                        variants = np.stack([t, t[:, ::-1], t[::-1, :], t[::-1, ::-1]]).reshape(1, 4, 64)
                        stack = np.concatenate([stack, variants])
                        entry = (len(tiles) - 1, 0, 0)
                    entries.append(entry)
            maps[frame] = entries
        return tiles, maps

    tolerance = 0
    tiles, maps = encode(0)
    if len(tiles) > limit:
        lo, hi = 1, MAX_TOLERANCE
        if len(encode(hi)[0]) > limit:
            fail('needs %d tiles even when reusing similar tiles; the limit is %d. Use fewer '
                 'frames, a smaller picture, or frames that differ less' % (len(tiles), limit))
        while lo < hi:
            mid = (lo + hi) // 2
            if len(encode(mid)[0]) <= limit:
                hi = mid
            else:
                lo = mid + 1
        tolerance = lo
        tiles, maps = encode(tolerance)

    data = b''.join(tile_bytes(_PixelAccess(t), 0, 0) for t in tiles)
    tilemap = []
    for frame in range(frames):
        for index, hf, vf in maps[frame]:
            tilemap.append(tile_slot(index) | (hf << 9) | (vf << 10))
    return data, struct.pack('<%dH' % len(tilemap), *tilemap), w, h, len(tiles), tolerance


class _PixelAccess:
    """Lets tile_bytes() read an 8x8 numpy array like a PIL pixel access object."""

    def __init__(self, arr):
        self.arr = arr

    def __getitem__(self, xy):
        return int(self.arr[xy[1], xy[0]])


# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
def spline_points(points, per_segment=24, tension=0.5):
    if len(points) < 3:
        return points
    out = []
    pts = [points[0]] + points + [points[-1]]
    for i in range(1, len(pts) - 2):
        p0, p1, p2, p3 = pts[i - 1], pts[i], pts[i + 1], pts[i + 2]
        for s in range(per_segment):
            t = s / per_segment
            t2, t3 = t * t, t * t * t
            c = []
            for a in (0, 1):
                m1 = (p2[a] - p0[a]) * tension
                m2 = (p3[a] - p1[a]) * tension
                c.append((2 * t3 - 3 * t2 + 1) * p1[a] + (t3 - 2 * t2 + t) * m1 +
                         (-2 * t3 + 3 * t2) * p2[a] + (t3 - t2) * m2)
            out.append(tuple(c))
    out.append(points[-1])
    return out


def walk(points, speed):
    """Resample a polyline so that consecutive points are 'speed' pixels apart."""
    out = [points[0]]
    pos = points[0]
    i = 1
    while i < len(points):
        target = points[i]
        d = math.hypot(target[0] - pos[0], target[1] - pos[1])
        if d < 1e-9:
            i += 1
            continue
        need = speed
        while i < len(points):
            target = points[i]
            d = math.hypot(target[0] - pos[0], target[1] - pos[1])
            if d >= need:
                pos = (pos[0] + (target[0] - pos[0]) * need / d,
                       pos[1] + (target[1] - pos[1]) * need / d)
                out.append(pos)
                break
            need -= d
            pos = target
            i += 1
    if out[-1] != points[-1]:
        out.append(points[-1])
    return out


def path_from_text(text, name):
    speed = 2.0
    segments = [[]]
    events = []           # ('seg', points, speed) | ('wait', frames)
    for raw in text.splitlines():
        line = raw.split('#')[0].strip()
        if not line:
            continue
        words = line.split()
        if words[0] == 'speed':
            if len(segments[-1]) > 1:
                events.append(('seg', segments[-1], speed))
                segments.append([segments[-1][-1]])
            speed = float(words[1])
        elif words[0] in ('wait', 'corner'):
            if len(segments[-1]) > 1:
                events.append(('seg', segments[-1], speed))
                segments.append([segments[-1][-1]])
            if words[0] == 'wait':
                events.append(('wait', int(words[1])))
        else:
            try:
                segments[-1].append((float(words[0]), float(words[1])))
            except (ValueError, IndexError):
                fail('path %s: cannot understand "%s"' % (name, raw))
    if len(segments[-1]) > 1:
        events.append(('seg', segments[-1], speed))
    steps = []
    cur = None
    for ev in events:
        if ev[0] == 'wait':
            steps += [(0, 0)] * ev[1]
            continue
        pts = walk(spline_points(ev[1]), ev[2])
        if cur is None:
            cur = (round(pts[0][0]), round(pts[0][1]))
        for p in pts[1:]:
            q = (round(p[0]), round(p[1]))
            steps.append((q[0] - cur[0], q[1] - cur[1]))
            cur = q
    return steps


def path_from_spline_json(text):
    pts = json.loads(text)['calculatedPoints']
    steps = []
    prev = pts[0]
    for p in pts:
        steps.append((int(round((p['x'] - prev['x']) / 2.0)), int(round((p['y'] - prev['y']) / 2.0))))
        prev = p
    return steps


def path_bytes(steps, name):
    # A path restarts when it ends, so a repeating movement only needs one period.
    for period in range(1, len(steps) // 2 + 1):
        if len(steps) % period == 0 and steps == steps[:period] * (len(steps) // period):
            steps = steps[:period]
            break
    out = bytearray()
    for dx, dy in steps:
        if not (-127 <= dx <= 127 and -127 <= dy <= 127):
            fail('path %s: a step is too fast' % name)
        out += struct.pack('bb', dx, dy)
    out += struct.pack('bb', -128, -128)
    return bytes(out)


# ---------------------------------------------------------------------------
# Levels (Tiled .tmx)
# ---------------------------------------------------------------------------
def level_from_tmx(path):
    name = os.path.basename(path)
    root = ET.parse(path).getroot()
    width, height = int(root.get('width')), int(root.get('height'))
    if width != 16:
        fail('%s: the map must be 16 tiles wide' % name)
    tilesets = sorted(((int(t.get('firstgid')), t.get('source') or t.get('name') or '')
                       for t in root.findall('tileset')), reverse=True)

    def split_gid(gid):
        gid &= 0x1FFFFFFF
        for first, src in tilesets:
            if gid >= first:
                return gid - first, src
        return 0, ''

    layer = root.find('layer')
    if layer is None:
        fail('%s: no tile layer' % name)
    data = layer.find('data')
    if data.get('encoding') != 'csv':
        fail('%s: please save the map with the CSV tile layer format' % name)
    cells = [int(v) for v in data.text.replace('\n', '').split(',') if v.strip()]
    rows = []
    for y in range(height):
        row = []
        for x in range(width):
            gid = cells[y * width + x]
            tid = split_gid(gid)[0] if gid else 4
            if tid > 47:
                fail('%s: tile %d at %d,%d is outside the 48 tile tileset' % (name, tid, x, y))
            row.append(tid)
        rows.append(row)

    objects = {}
    count = 0
    for group in root.findall('objectgroup'):
        for obj in group.findall('object'):
            if obj.get('gid') is None:
                continue
            oid, src = split_gid(int(obj.get('gid')))
            if 'objects' not in src:
                fail('%s: object %s does not come from the objects tileset' % (name, obj.get('id')))
            col = int(float(obj.get('x')) // 16)
            row = int(math.ceil(float(obj.get('y')) / 16.0)) - 1    # tile objects are anchored at the bottom
            col = max(0, min(15, col))
            row = max(0, min(height - 1, row))
            objects.setdefault(row, []).append((col, oid))
            count += 1

    out = bytearray()
    for y in range(height - 1, -1, -1):
        for col, oid in sorted(objects.get(y, [])):
            out += bytes((0x40 | col, oid))
        row = rows[y]
        x = 0
        while x < 16:
            n = 1
            while x + n < 16 and row[x + n] == row[x]:
                n += 1
            if n > 1:
                out += bytes((0x80 | n, row[x]))
            else:
                out.append(row[x])
            x += n
    out.append(0xFF)
    return bytes(out), height, count


# ---------------------------------------------------------------------------
# Text
# ---------------------------------------------------------------------------
def text_bytes(text, name, picture_ids):
    out = bytearray()
    page_lines = 0
    page_start = 0
    for raw in text.splitlines():
        line = raw.rstrip()
        if line.startswith('#'):
            continue
        if line.startswith('----'):
            while out and out[-1] == 10:
                out.pop()
            out.append(0x0C)
            page_lines = 0
            page_start = len(out)
            continue
        m = re.match(r'@picture\s+(\w+)', line)
        if m:
            if m.group(1) not in picture_ids:
                fail('%s: unknown picture "%s"' % (name, m.group(1)))
            out += bytes((0x01, picture_ids[m.group(1)] + 1))
            continue
        if len(line) > 30:
            fail('%s: line longer than 30 characters: "%s"' % (name, line))
        if any(ord(ch) < 32 or ord(ch) > 126 for ch in line):
            fail('%s: only plain ASCII characters are supported: "%s"' % (name, line))
        out += line.encode('ascii') + b'\n'
        page_lines += 1
        if len(out) - page_start > 400:
            fail('%s: too much text on one page (400 characters at most)' % name)
        if page_lines > 20:
            fail('%s: too many lines on one page' % name)
    while out and out[-1] in (10, 0x0C):
        out.pop()
    out.append(0)
    return bytes(out)


# ---------------------------------------------------------------------------
# Bank packing
# ---------------------------------------------------------------------------
class Packer:
    def __init__(self):
        self.banks = []           # list of [used, [(symbol, ctype, data)]]
        self.where = {}

    def add(self, unit, pinned=None):
        """unit: list of (symbol, ctype, bytes). Kept together in one bank."""
        size = sum(len(d) for _, _, d in unit)
        if size > BANK_SIZE:
            fail('%s is too big for a 16 KB bank (%d bytes)' % (unit[0][0], size))
        if pinned is not None:
            while len(self.banks) <= pinned:
                self.banks.append([0, []])
            target = pinned
            if self.banks[target][0] + size > BANK_SIZE:
                fail('the enemy paths no longer fit in one 16 KB bank')
        else:
            target = None
            for i, (used, _) in enumerate(self.banks):
                if i != 0 and used + size <= BANK_SIZE:      # bank index 0 is the gameplay bank
                    target = i
                    break
            if target is None:
                self.banks.append([0, []])
                target = len(self.banks) - 1
        self.banks[target][0] += size
        self.banks[target][1] += unit
        for sym, _, _ in unit:
            self.where[sym] = target + FIRST_BANK
        return target + FIRST_BANK


def c_array(sym, ctype, data):
    lines = []
    if ctype == 'unsigned int':
        vals = struct.unpack('<%dH' % (len(data) // 2), data)
        fmt = '0x%04X'
    elif ctype == 'path_step':
        vals = struct.unpack('%db' % len(data), data)
        body = ', '.join('{%d,%d}' % (vals[i], vals[i + 1]) for i in range(0, len(vals), 2))
        return 'const path_step %s[] = {%s};\n' % (sym, body)
    else:
        vals = data
        fmt = '0x%02X'
    for i in range(0, len(vals), 16):
        lines.append('\t' + ','.join(fmt % v for v in vals[i:i + 16]))
    return 'const %s %s[] = {\n%s\n};\n' % (ctype, sym, ',\n'.join(lines))


# ---------------------------------------------------------------------------
# game.ini helpers
# ---------------------------------------------------------------------------
def parse_attacks(text, where):
    out = []
    for tok in text.replace('\n', ',').split(','):
        tok = tok.strip()
        if not tok:
            continue
        parts = tok.split()
        if len(parts) != 3 or parts[0] not in PATTERNS:
            fail('%s: attack "%s" should look like "pattern delay duration"; patterns: %s'
                 % (where, tok, ', '.join(PATTERNS[1:])))
        delay, duration = int(parts[1]), int(parts[2])
        if not (1 <= delay <= 255 and 4 <= duration <= 1020):
            fail('%s: attack "%s": delay must be 1..255 and duration 4..1020 frames' % (where, tok))
        out.append((PATTERNS.index(parts[0]), delay, duration // 4))
    if not out:
        fail('%s: at least one attack is needed' % where)
    return out


def rgb_byte(text, where):
    try:
        r, g, b = [int(v) for v in text.replace(',', ' ').split()]
        assert 0 <= r <= 3 and 0 <= g <= 3 and 0 <= b <= 3
    except Exception:
        fail('%s: a colour is three numbers from 0 to 3 (red green blue)' % where)
    return r | (g << 2) | (b << 4)


def sprite_tile(text, where):
    try:
        x, y = [int(v) for v in text.replace(',', ' ').split()]
        assert x % 8 == 0 and y % 16 == 0
    except Exception:
        fail('%s: sprite must be "x y" in pixels inside sprites.png (x multiple of 8, y of 16)' % where)
    return ((y // 16) * 32 + x // 8) * 2


DIGITS = ['111101101101111', '010110010010111', '111001111100111', '111001111001111', '101101111001001',
          '111100111001111', '111100111101111', '111001001001001', '111101111101111', '111101111001111']


def make_objects_png(enemies, waves, sprites_im):
    """The tileset used inside Tiled to place waves and items."""
    rgb = sprites_im.convert('RGB')
    out = Image.new('RGBA', (256, 64), (0, 0, 0, 0))
    d = ImageDraw.Draw(out)

    def paste_sprite(tile, wpx, dx, dy):
        cell = tile // 2
        sx, sy = (cell % 32) * 8, (cell // 32) * 16
        sx += (wpx - 16) // 2
        crop = rgb.crop((sx, sy, sx + 16, sy + 16)).convert('RGBA')
        bg = sprites_im.getpalette()[0:3]
        px = crop.load()
        for y in range(16):
            for x in range(16):
                if list(px[x, y][:3]) == list(bg):
                    px[x, y] = (0, 0, 0, 0)
        out.paste(crop, (dx, dy), crop)

    def number(n, dx, dy):
        for k, ch in enumerate(str(n)):
            g = DIGITS[int(ch)]
            d.rectangle((dx + k * 4 - 1, dy - 1, dx + k * 4 + 3, dy + 5), fill=(0, 0, 0, 255))
            for i, bit in enumerate(g):
                if bit == '1':
                    out.putpixel((dx + k * 4 + i % 3, dy + i // 3), (255, 255, 0, 255))

    for wid, wave in waves.items():
        e = enemies[wave['enemy']]
        dx, dy = (wid % 16) * 16, (wid // 16) * 16
        paste_sprite(e['tile'], e['width'], dx, dy)
        number(wave['count'], dx + 1, dy + 10)
    item_tiles = [100, 108, 116, 204, 208]
    for i, tile in enumerate(item_tiles):
        paste_sprite(tile, 16, i * 16, 48)
    path = os.path.join(DATA, 'map', 'objects.png')
    out.save(path)
    with open(os.path.join(DATA, 'map', 'objects.tsx'), 'w', newline='\n') as f:
        f.write('<?xml version="1.0" encoding="UTF-8"?>\n'
                '<tileset version="1.5" tiledversion="2021.03.23" name="objects" tilewidth="16" '
                'tileheight="16" tilecount="64" columns="16">\n'
                ' <image source="objects.png" width="256" height="64"/>\n'
                '</tileset>\n')


# ---------------------------------------------------------------------------
# Main build
# ---------------------------------------------------------------------------
def build():
    os.makedirs(GEN, exist_ok=True)
    packer = Packer()
    report = []
    h = []                 # assets.h lines
    tables = []            # tables.c chunks

    ini = configparser.ConfigParser(inline_comment_prefixes=(';',), interpolation=None)
    ini.optionxform = str
    if not ini.read(os.path.join(DATA, 'game.ini')):
        fail('data/game.ini is missing')

    def sections(kind):
        out = []
        for sec in ini.sections():
            parts = sec.split(None, 1)
            if parts[0] == kind:
                if len(parts) != 2:
                    fail('[%s] needs a name' % sec)
                out.append((parts[1].strip(), ini[sec]))
        return out

    def need(sec, name, key):
        if key not in sec:
            fail('[%s]: "%s" is missing' % (name, key))
        return sec[key]

    # --- gameplay bank: enemy paths (always mapped while playing) --------------
    paths = {}
    for p in sorted(glob.glob(os.path.join(DATA, 'path', '*'))):
        base = os.path.basename(p)
        with open(p) as f:
            text = f.read()
        if base.endswith('.path.txt'):
            name = base[:-9]
            steps = path_from_text(text, name)
        elif base.endswith('.spline.json'):
            name = base[:-12]
            steps = path_from_spline_json(text)
        else:
            continue
        if not steps:
            fail('path %s is empty' % name)
        sym = 'path_' + c_name(name)
        paths[name] = sym
        packer.add([(sym, 'path_step', path_bytes(steps, name))], pinned=0)
        report.append('path    %-18s %4d frames' % (name, len(steps)))

    levels = {}
    for p in sorted(glob.glob(os.path.join(DATA, 'map', '*.tmx'))):
        name = os.path.basename(p)[:-4]
        data, rows, nobj = level_from_tmx(p)
        sym = 'level_' + c_name(name)
        levels[name] = sym
        packer.add([(sym, 'unsigned char', data)])
        report.append('level   %-18s %4d rows, %3d objects, %5d bytes' % (name, rows, nobj, len(data)))

    # --- graphics -----------------------------------------------------------------
    sprites_im = load_indexed(os.path.join(DATA, 'img', 'sprites.png'))
    if sprites_im.size != (256, 64):
        fail('sprites.png must be 256x64')
    packer.add([('sprites_tiles', 'unsigned char', tiles_8x16(sprites_im)),
                ('sprites_palette', 'unsigned char', sms_palette(sprites_im))])

    tilesets = {}
    for p in sorted(glob.glob(os.path.join(DATA, 'img', 'tileset_*.png'))):
        name = os.path.basename(p)[8:-4]
        im = load_indexed(p)
        if im.size != (256, 48):
            fail('%s must be 256x48 (16x3 tiles of 16x16)' % p)
        sym = 'tileset_' + c_name(name)
        packer.add([(sym + '_tiles', 'unsigned char', tiles_8x16(im)),
                    (sym + '_palette', 'unsigned char', sms_palette(im))])
        tilesets[name] = len(tilesets)
    tileset_names = list(tilesets)

    # Pictures shown by the title and story screens share the screen with the
    # font, so only bosses get the bigger budget.
    boss_pictures = set()
    for sec_name in ini.sections():
        if sec_name.startswith('boss ') and 'picture' in ini[sec_name]:
            boss_pictures.add(ini[sec_name]['picture'])
    screen_pictures = {'title_logo'}
    for p in glob.glob(os.path.join(DATA, 'text', '*.txt')):
        with open(p) as f:
            screen_pictures |= set(re.findall(r'^@picture\s+(\w+)', f.read(), re.M))

    pictures = {}
    picture_info = []
    for name, sec in sections('picture'):
        where = 'picture ' + name
        fname = need(sec, where, 'file')
        frames = int(sec.get('frames', '1'))
        still = int(sec['still']) if 'still' in sec else None
        if name in boss_pictures and name not in screen_pictures:
            limit = BOSS_PICTURE_TILES
        else:
            limit = PICTURE_MAIN_TILES
        im = load_indexed(os.path.join(DATA, 'img', fname))
        try:
            tiles, tilemap, w, hh, count, tol = picture(im, frames, int(sec.get('key_frame', '0')), limit, still)
        except BuildError as e:
            fail('[%s] (%s): %s%s' % (where, fname, e, '' if limit == BOSS_PICTURE_TILES else
                 '. Pictures shown on the title or story screens can use 192 tiles at most; '
                 'make a "still" copy of a boss picture for those (see game.ini)'))
        if still is not None:
            frames = 1
        sym = 'pic_' + c_name(name)
        packer.add([(sym + '_tiles', 'unsigned char', tiles),
                    (sym + '_tilemap', 'unsigned int', tilemap),
                    (sym + '_palette', 'unsigned char', sms_palette(im))])
        pictures[name] = len(pictures)
        picture_info.append((name, sym, len(tiles), w, hh, frames, count))
        report.append('picture %-18s %2dx%-2d tiles, %d frame(s), %3d/%d tiles%s' % (
            name, w, hh, frames, count, limit,
            ', similar tiles reused (up to %d pixels apart)' % tol if tol else ''))

    # --- music ------------------------------------------------------------------
    music = {}
    music_info = []
    for p in sorted(glob.glob(os.path.join(DATA, 'music', '*.mml'))):
        name = os.path.basename(p)[:-4]
        with open(p) as f:
            data, loop, frames = mml2psg.compile_mml(f.read(), name)
        sym = 'music_' + c_name(name)
        packer.add([(sym, 'unsigned char', data)])
        music[name] = len(music)
        music_info.append((name, sym, loop))
        report.append('music   %-18s %5.1f s, %5d bytes' % (name, frames / 60.0, len(data)))

    # --- text -------------------------------------------------------------------
    texts = {}
    for p in sorted(glob.glob(os.path.join(DATA, 'text', '*.txt'))):
        name = os.path.basename(p)[:-4]
        with open(p) as f:
            data = text_bytes(f.read(), name, pictures)
        sym = 'text_' + c_name(name)
        packer.add([(sym, 'unsigned char', data)])
        texts[name] = sym

    # --- sound effects (fixed ROM: they must be reachable whatever bank is mapped) --
    sfx = {}
    sfx_info = []
    for p in sorted(glob.glob(os.path.join(DATA, 'sfx', '*.sfx'))):
        name = os.path.basename(p)[:-4]
        with open(p) as f:
            data, channels, priority = mml2psg.compile_sfx(f.read(), name)
        sym = 'sfx_' + c_name(name)
        tables.append(c_array(sym, 'unsigned char', data).replace('const ', 'static const ', 1))
        sfx[name] = len(sfx)
        sfx_info.append((name, sym, channels, priority))

    # --- game.ini tables -----------------------------------------------------------
    def lookup(table, value, what, where):
        if value not in table:
            fail('[%s]: unknown %s "%s" (known: %s)' % (where, what, value, ', '.join(sorted(table))))
        return table[value]

    enemies = {}
    enemy_rows = []
    for name, sec in sections('enemy'):
        where = 'enemy ' + name
        width = int(sec.get('width', '16'))
        if width not in (8, 16, 24, 32):
            fail('[%s]: width must be 8, 16, 24 or 32' % where)
        flags = 0
        for fl in sec.get('flags', '').split():
            if fl == 'ground':
                flags |= 1
            elif fl == 'tough':
                flags |= 2
            else:
                fail('[%s]: unknown flag "%s"' % (where, fl))
        e = dict(tile=sprite_tile(need(sec, where, 'sprite'), where), width=width,
                 frames=int(sec.get('frames', '1')), anim=int(sec.get('anim', '8')),
                 hp=int(sec.get('hp', '1')), score=int(sec.get('score', '10')) // 10,
                 pattern=lookup({p: i for i, p in enumerate(PATTERNS)}, sec.get('shot', 'none'), 'shot', where),
                 fire=int(sec.get('fire', '0')), flags=flags, index=len(enemies))
        if not (0 <= e['hp'] <= 255 and 0 <= e['score'] <= 255 and 0 <= e['fire'] <= 255):
            fail('[%s]: hp and fire must be 0..255, score 0..2550' % where)
        homing = sec.get('move', 'path')
        if homing not in ('path', 'homing'):
            fail('[%s]: move must be "path" or "homing"' % where)
        if homing == 'homing':
            try:
                lock = int(sec.get('lock', '30'))
                chase = int(sec.get('chase', '120'))
                max_speed = int(round(float(sec.get('speed', '2')) * 16))
                turn = int(round(float(sec.get('turn', '0.2')) * 16))
            except ValueError:
                fail('[%s]: lock and chase are frames; speed and turn are pixels per frame' % where)
            if not (0 <= lock <= 255 and 1 <= chase <= 255):
                fail('[%s]: lock must be 0..255 and chase 1..255 frames' % where)
            if not (1 <= max_speed <= 96 and 1 <= turn <= 32):
                fail('[%s]: speed must be from 0.0625 to 6, and turn from 0.0625 to 2 pixels per frame' % where)
        else:
            lock = chase = max_speed = turn = 0
        enemies[name] = e
        enemy_rows.append('\t{%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d},\t/* %s */' % (
            e['tile'], width // 8, e['frames'], e['anim'], e['hp'], e['score'], e['pattern'], e['fire'], flags,
            lock, chase, max_speed, turn, name))

    waves = {}
    for name, sec in sections('wave'):
        where = 'wave ' + name
        wid = int(need(sec, where, 'id'))
        if not 0 <= wid < WAVE_SLOTS or wid in waves:
            fail('[%s]: id must be unique and 0..%d' % (where, WAVE_SLOTS - 1))
        waves[wid] = dict(name=name, enemy=need(sec, where, 'enemy'), count=int(sec.get('count', '1')),
                          spacing=int(sec.get('spacing', '12')), path=need(sec, where, 'path'))
        lookup(enemies, waves[wid]['enemy'], 'enemy', where)
        lookup(paths, waves[wid]['path'], 'path', where)
    wave_rows = []
    for wid in range(WAVE_SLOTS):
        if wid in waves:
            w = waves[wid]
            wave_rows.append('\t{%d, %d, %d, %s},\t/* %d: %s */' % (
                enemies[w['enemy']]['index'], w['count'], w['spacing'], paths[w['path']], wid, w['name']))
        else:
            wave_rows.append('\t{0, 0, 0, 0},')

    bosses = {}
    boss_rows = []
    attack_chunks = []
    for name, sec in sections('boss'):
        where = 'boss ' + name
        pic = lookup(pictures, need(sec, where, 'picture'), 'picture', where)
        _, _, _, pw, ph, _, _ = picture_info[pic]
        hit = [int(v) for v in sec.get('hitbox', '8 8 %d %d' % (pw * 8 - 16, ph * 8 - 16)).split()]
        muzzle = [int(v) for v in sec.get('muzzle', '%d %d' % (pw * 4 - 4, ph * 8 - 16)).split()]
        hp = int(need(sec, where, 'hp'))
        rage_hp = int(sec.get('rage_hp', str(hp // 3)))
        attacks = parse_attacks(need(sec, where, 'attacks'), where)
        rage = parse_attacks(sec.get('rage_attacks', sec['attacks']), where)
        sym = 'boss_%s' % c_name(name)
        for suffix, lst in (('_attacks', attacks), ('_rage_attacks', rage)):
            attack_chunks.append('static const boss_attack %s%s[] = {%s, {0, 0, 0}};\n' % (
                sym, suffix, ', '.join('{%d, %d, %d}' % a for a in lst)))
        move = lookup({m: i for i, m in enumerate(MOVES)}, sec.get('move', 'wander'), 'move', where)
        animation = lookup({'loop': 0, 'pingpong': 1}, sec.get('animation', 'loop'), 'animation', where)
        boss_rows.append('\t{"%s", %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %s_attacks, %s_rage_attacks, %d},' % (
            sec.get('name', name.upper()), pic, hp, rage_hp, hit[0], hit[1], hit[2], hit[3],
            muzzle[0], muzzle[1], int(sec.get('anim', '16')), animation, move,
            int(sec.get('speed', '1')), int(sec.get('rage_speed', '2')), sym, sym,
            int(sec.get('score', '5000')) // 100))
        bosses[name] = len(bosses)

    stage_rows = []
    stage_list = sorted(sections('stage'), key=lambda s: int(s[0]))
    for i, (name, sec) in enumerate(stage_list):
        where = 'stage ' + name
        if int(name) != i + 1:
            fail('stages must be numbered 1, 2, 3... without gaps')
        level_sym = lookup(levels, need(sec, where, 'map'), 'map', where)
        stage_rows.append('\t{"%s", %d, %s, %d, %d, %d, %d, %d, %d, %d},' % (
            need(sec, where, 'name'),
            lookup(tilesets, need(sec, where, 'tileset'), 'tileset', where),
            level_sym, packer.where[level_sym],
            lookup(music, need(sec, where, 'music'), 'music', where),
            lookup(bosses, need(sec, where, 'boss'), 'boss', where),
            lookup(music, sec.get('boss_music', 'boss'), 'music', where),
            rgb_byte(sec.get('sky', '0 0 0'), where),
            int(sec.get('scroll', '1')),
            int(sec.get('bonus', '1000')) // 100))
    if not stage_rows:
        fail('game.ini defines no [stage N] section')

    make_objects_png(enemies, waves, sprites_im)

    # --- write the banks -----------------------------------------------------------
    for old in glob.glob(os.path.join(GEN, 'bank*.c')):
        os.remove(old)
    for i, (used, items) in enumerate(packer.banks):
        n = i + FIRST_BANK
        with open(os.path.join(GEN, 'bank%d.c' % n), 'w', newline='\n') as f:
            f.write('/* Generated by tool/build_assets.py - do not edit. */\n#include "../actor.h"\n\n')
            for sym, ctype, data in items:
                f.write(c_array(sym, ctype, data))
                f.write('\n')
        report.append('bank %d: %5d / %d bytes' % (n, used, BANK_SIZE))

    # --- assets.h ---------------------------------------------------------------------
    h.append('/* Generated by tool/build_assets.py - do not edit. */')
    h.append('#ifndef ASSETS_H\n#define ASSETS_H\n')
    h.append('#include "../actor.h"\n#include "../defs.h"\n')
    h.append('#define GAME_BANK (%d)' % FIRST_BANK)
    for i, (used, items) in enumerate(packer.banks):
        for sym, ctype, data in items:
            h.append('extern const %s %s[];' % (ctype, sym))
            h.append('#define %s_size (%d)' % (sym, len(data)))
            h.append('#define %s_bank (%d)' % (sym, i + FIRST_BANK))
    h.append('')

    def enum(prefix, names, count_name):
        h.append('enum {')
        for i, n in enumerate(names):
            h.append('\t%s_%s = %d,' % (prefix, c_name(n).upper(), i))
        h.append('\t%s = %d' % (count_name, len(names)))
        h.append('};\n')

    enum('PIC', [p[0] for p in picture_info], 'PICTURE_COUNT')
    enum('TILESET', tileset_names, 'TILESET_COUNT')
    enum('MUSIC', [m[0] for m in music_info], 'MUSIC_COUNT')
    enum('SFX', [s[0] for s in sfx_info], 'SFX_COUNT')
    enum('ENEMY', list(enemies), 'ENEMY_TYPE_COUNT')
    enum('BOSS', list(bosses), 'BOSS_COUNT')
    enum('PATTERN', PATTERNS, 'PATTERN_COUNT')
    enum('MOVE', MOVES, 'MOVE_COUNT')
    h.append('#define STAGE_COUNT (%d)' % len(stage_rows))
    h.append('#define WAVE_COUNT (%d)' % WAVE_SLOTS)
    h.append('#define ITEM_OBJECT_BASE (%d)\n' % WAVE_SLOTS)
    h.append('#define PICTURE_MAIN_TILES (%d)' % PICTURE_MAIN_TILES)
    h.append('extern const unsigned char boss_extra_tiles[];')
    for t in ('picture_def pictures', 'tileset_def tilesets', 'music_def musics', 'sfx_def sfxs',
              'enemy_type enemy_types', 'wave_def waves', 'boss_def bosses', 'stage_def stages'):
        h.append('extern const %s[];' % t)
    h.append('\n#endif /* ASSETS_H */')
    with open(os.path.join(GEN, 'assets.h'), 'w', newline='\n') as f:
        f.write('\n'.join(h) + '\n')

    # --- tables.c -----------------------------------------------------------------------
    with open(os.path.join(GEN, 'tables.c'), 'w', newline='\n') as f:
        f.write('/* Generated by tool/build_assets.py from data/game.ini - do not edit. */\n')
        f.write('#include "assets.h"\n\n')
        f.write(''.join(tables))
        f.write('\n/* Sprite tiles a boss picture may borrow: first tile, count. */\n')
        f.write('const unsigned char boss_extra_tiles[] = {%s, 0};\n' % ', '.join(
            '%d, %d' % r for r in BOSS_EXTRA_TILES))
        f.write('\nconst picture_def pictures[] = {\n')
        for name, sym, size, w, hh, frames, count in picture_info:
            f.write('\t{%s_tiles, %d, %s_tilemap, %s_palette, %d, %d, %d, %d, %d},\t/* %s */\n' % (
                sym, size, sym, sym, packer.where[sym + '_tiles'], w, hh, frames, count, name))
        f.write('};\n\nconst tileset_def tilesets[] = {\n')
        for name in tileset_names:
            sym = 'tileset_' + c_name(name)
            f.write('\t{%s_tiles, %s_tiles_size, %s_palette, %d},\n' % (sym, sym, sym, packer.where[sym + '_tiles']))
        f.write('};\n\nconst music_def musics[] = {\n')
        for name, sym, loop in music_info:
            f.write('\t{%s, %d, %d},\n' % (sym, packer.where[sym], 1 if loop else 0))
        f.write('};\n\nconst sfx_def sfxs[] = {\n')
        for name, sym, channels, priority in sfx_info:
            f.write('\t{%s, %d, %d},\n' % (sym, channels, priority))
        f.write('};\n\nconst enemy_type enemy_types[] = {\n' + '\n'.join(enemy_rows))
        f.write('\n};\n\nconst wave_def waves[] = {\n' + '\n'.join(wave_rows))
        f.write('\n};\n\n' + ''.join(attack_chunks))
        f.write('\nconst boss_def bosses[] = {\n' + '\n'.join(boss_rows))
        f.write('\n};\n\nconst stage_def stages[] = {\n' + '\n'.join(stage_rows))
        f.write('\n};\n')

    # --- banks.mk ------------------------------------------------------------------------
    nums = [i + FIRST_BANK for i in range(len(packer.banks))]
    with open(os.path.join(GEN, 'banks.mk'), 'w', newline='\n') as f:
        f.write('# Generated by tool/build_assets.py - do not edit.\n')
        f.write('BANKS := %s\n' % ' '.join(str(n) for n in nums))
        f.write('BANKFLAGS := %s\n' % ' '.join('-Wl-b_BANK%d=0x%X' % (n, n * 0x10000 + 0x8000) for n in nums))

    required_music = ['title', 'intro', 'boss', 'stage_clear', 'game_over', 'ending']
    for m in required_music:
        if m not in music:
            fail('data/music/%s.mml is required by the game' % m)
    for t in ('intro', 'ending'):
        if t not in texts:
            fail('data/text/%s.txt is required by the game' % t)
    print('\n'.join(report))


if __name__ == '__main__':
    try:
        build()
    except (BuildError, mml2psg.MMLError) as e:
        sys.stderr.write('\nASSET ERROR: %s\n\n' % e)
        sys.exit(1)

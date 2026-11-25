#!/usr/bin/env python3
"""
One-off helper that (re)creates the game's PNG art from the original jam
assets (data/img/original/*.png, data/img/Floor.png, data/img/dracolich.png).

It is NOT part of the normal build: the PNGs in data/img are the source of
truth and you can simply edit them with any paint program. Run this only if
you want to regenerate everything from scratch:

    python3 tool/gen/make_art.py

Requires Pillow.
"""
import math
import os
import random
from PIL import Image, ImageDraw

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
IMG = os.path.join(ROOT, 'data', 'img')

# ---------------------------------------------------------------------------
# Sprite palette letters (same order as the original sprites.png palette)
# ---------------------------------------------------------------------------
CH = '.kdwroyGglLcCbBp'
#     0 transparent  k black   d dark grey  w white   r red-orange  o orange
#     y yellow  G dark green  g green  l bright green  L light green
#     c cyan  C light cyan  b blue  B mid blue  p periwinkle
IDX = {c: i for i, c in enumerate(CH)}


def ascii_half(rows, w=8):
    """Builds a symmetric sprite from its left half."""
    h = len(rows)
    im = Image.new('P', (w * 2, h), 0)
    for y, row in enumerate(rows):
        assert len(row) == w, (row, len(row))
        for x, c in enumerate(row):
            im.putpixel((x, y), IDX[c])
            im.putpixel((w * 2 - 1 - x, y), IDX[c])
    return im


def ascii_full(rows):
    w = len(rows[0])
    im = Image.new('P', (w, len(rows)), 0)
    for y, row in enumerate(rows):
        assert len(row) == w, (row, len(row))
        for x, c in enumerate(row):
            im.putpixel((x, y), IDX[c])
    return im


def remap(im, table):
    lut = list(range(256))
    for k, v in table.items():
        lut[IDX[k] if isinstance(k, str) else k] = IDX[v] if isinstance(v, str) else v
    return im.point(lut)


# ---------------------------------------------------------------------------
# Small enemies (16x16, two frames each)
# ---------------------------------------------------------------------------
BAT_A = [
    "........",
    "k.......",
    "kk......",
    "kpk...k.",
    "kppk.kpk",
    "kpppkppp",
    "kBppppyp",
    "kBBpkppp",
    ".kBBkkpw",
    "..kBk.kp",
    "...k..kk",
    "........",
    "........",
    "........",
    "........",
    "........",
]
BAT_B = [
    "........",
    "........",
    "........",
    "......k.",
    ".....kpk",
    "..kkkppp",
    ".kpppryp"[0:5] + "pyp",
    "kpppkppp",
    "kBpkkkpw",
    "kBBk.kkp",
    "kBBk..kk",
    "kBk.....",
    "kk......",
    "k.......",
    "........",
    "........",
]
SKULL_A = [
    "........",
    "....kkkk",
    "..kkwwww",
    ".kwwwwww",
    ".kwwwwww",
    "kwwwwwww",
    "kwdkkkww",
    "kwkkrkkw",
    "kwdkkkww",
    ".kwwwwwk",
    ".kdwwwww",
    "..kkwkwk",
    "...kwkwk",
    "...kdwdw",
    "....kkkk",
    "........",
]
SKULL_B = [
    "....kkkk",
    "..kkwwww",
    ".kwwwwww",
    ".kwwwwww",
    "kwwwwwww",
    "kwdkkkww",
    "kwkkykkw",
    "kwdkkkww",
    ".kwwwwwk",
    ".kdwwwww",
    "..kkwkwk",
    "...kkkkk",
    "...kwkwk",
    "...kdwdw",
    "....kkkk",
    "........",
]
WISP_A = [
    ".......k",
    "......kr",
    "......kr",
    ".....kro",
    "....kroo",
    "...krooy",
    "..krooyy",
    "..kroyyy",
    ".kroykyy",
    ".kroykyw",
    ".kroyyyw",
    ".kroyyyy",
    "..kroyyy",
    "..kkrooo",
    "....kkkk",
    "........",
]
WISP_B = [
    "........",
    ".....k..",
    "....krk.",
    "....krrk",
    "....kror",
    "...krooo",
    "..krooyy",
    ".kkroyyy",
    ".kroykyy",
    ".kroykyw",
    ".kroyyyw",
    ".kkoyyyy",
    "..kroyyy",
    "..kkrooo",
    "....kkkk",
    "........",
]


def gazer(frame):
    im = Image.new('P', (16, 16), 0)
    d = ImageDraw.Draw(im)
    d.ellipse((0, 1, 15, 15), fill=IDX['B'], outline=IDX['k'])
    d.ellipse((2, 3, 13, 13), fill=IDX['b'])
    if frame == 0:
        d.ellipse((3, 4, 12, 12), fill=IDX['w'])
        d.ellipse((5, 6, 10, 11), fill=IDX['r'])
        d.rectangle((7, 7, 8, 10), fill=IDX['k'])
        im.putpixel((6, 7), IDX['y'])
    else:
        d.ellipse((3, 7, 12, 11), fill=IDX['w'])
        d.rectangle((6, 8, 9, 10), fill=IDX['r'])
        d.rectangle((7, 8, 8, 10), fill=IDX['k'])
    # little horns
    for x in (1, 14):
        im.putpixel((x, 1), IDX['k'])
        im.putpixel((x, 0), IDX['k'])
    for x in (2, 13):
        im.putpixel((x, 1), IDX['p'])
    im.putpixel((4, 2), IDX['p'])
    im.putpixel((5, 2), IDX['p'])
    return im


def turret(frame):
    im = Image.new('P', (16, 16), 0)
    d = ImageDraw.Draw(im)
    d.ellipse((0, 0, 15, 15), fill=IDX['d'], outline=IDX['k'])
    d.ellipse((2, 2, 13, 13), fill=IDX['p'], outline=IDX['d'])
    d.ellipse((4, 4, 11, 11), fill=IDX['k'])
    d.ellipse((5, 5, 10, 10), fill=IDX['r' if frame == 0 else 'y'])
    d.rectangle((7, 7, 8, 8), fill=IDX['y' if frame == 0 else 'w'])
    for (x, y) in ((3, 3), (4, 3), (3, 4)):
        im.putpixel((x, y), IDX['w'])
    # battlements
    for (x, y) in ((7, 0), (8, 0), (0, 7), (0, 8), (15, 7), (15, 8), (7, 15), (8, 15)):
        im.putpixel((x, y), IDX['k'])
    for (x, y) in ((7, 1), (8, 1), (1, 7), (1, 8), (14, 7), (14, 8), (7, 14), (8, 14)):
        im.putpixel((x, y), IDX['k'])
    return im


def explosion(frame):
    rnd = random.Random(7 + frame)
    im = Image.new('P', (16, 16), 0)
    d = ImageDraw.Draw(im)
    if frame == 0:
        d.ellipse((4, 4, 11, 11), fill=IDX['y'], outline=IDX['o'])
        d.ellipse((6, 6, 9, 9), fill=IDX['w'])
    elif frame == 1:
        d.ellipse((1, 1, 14, 14), fill=IDX['o'], outline=IDX['r'])
        d.ellipse((4, 4, 11, 11), fill=IDX['y'])
        d.ellipse((6, 6, 9, 9), fill=IDX['w'])
        for i in range(8):
            a = i * math.pi / 4
            im.putpixel((int(7.5 + 7.4 * math.cos(a)), int(7.5 + 7.4 * math.sin(a))), IDX['y'])
    else:
        for i in range(26):
            a = rnd.random() * 2 * math.pi
            r = 3 + rnd.random() * 4.5
            x, y = int(7.5 + r * math.cos(a)), int(7.5 + r * math.sin(a))
            c = rnd.choice('rrood')
            d.rectangle((x, y, min(15, x + rnd.randint(0, 1)), min(15, y + rnd.randint(0, 1))), fill=IDX[c])
    return im


ORB_A = [
    "........",
    "........",
    "........",
    "........",
    "..kkkk..",
    ".krrrrk.",
    "krryyrrk",
    "krywwyrk",
    "krywwyrk",
    "krryyrrk",
    ".krrrrk.",
    "..kkkk..",
    "........",
    "........",
    "........",
    "........",
]
ORB_B = [
    "........",
    "........",
    "........",
    "........",
    "..kkkk..",
    ".kyyyyk.",
    "kyywwyyk",
    "kywwwwyk",
    "kywwwwyk",
    "kyywwyyk",
    ".kyyyyk.",
    "..kkkk..",
    "........",
    "........",
    "........",
    "........",
]
NEEDLE = [
    "........",
    "........",
    "...kk...",
    "..kCCk..",
    "..kCCk..",
    ".kCwwCk.",
    ".kCwwCk.",
    ".kCwwCk.",
    ".kCwwCk.",
    ".kbwwbk.",
    "..kbbk..",
    "..kbbk..",
    "...kk...",
    "........",
    "........",
    "........",
]
SHARD = [
    "........",
    "........",
    "........",
    "...kk...",
    "..kppk..",
    ".kpwwpk.",
    "kpwwwwpk",
    "kpwwwwpk",
    ".kpwwpk.",
    "..kppk..",
    "...kk...",
    "........",
    "........",
    "........",
    "........",
    "........",
]
HEART = [
    "........",
    "........",
    "........",
    "........",
    ".kk.kk..",
    "krrkrrk.",
    "krwrrrk.",
    "krrrrrk.",
    ".krrrk..",
    "..krk...",
    "...k....",
    "........",
    "........",
    "........",
    "........",
    "........",
]
HP_FULL = [
    "wwwwwww.",
    "wrrrrrw.",
    "wrooorw.",
    "wrooorw.",
    "wrooorw.",
    "wrooorw.",
    "wrooorw.",
    "wrooorw.",
    "wrooorw.",
    "wrooorw.",
    "wrooorw.",
    "wrooorw.",
    "wrooorw.",
    "wrooorw.",
    "wrrrrrw.",
    "wwwwwww.",
]
HP_EMPTY = [r.replace('r', 'k').replace('o', 'k') for r in HP_FULL]
BIG_HEART = [
    "..kk..kk..",
    ".krrkkrrk.",
    "krwrrrrrrk",
    "krwrrrrrrk",
    "krrrrrrrrk",
    ".krrrrrrk.",
    "..krrrrk..",
    "...krrk...",
    "....kk....",
]
GEM = [
    "....kk....",
    "...kyyk...",
    "..kywyyk..",
    ".kywyyyok.",
    "kyyyyyyook",
    ".koyyyook.",
    "..koyook..",
    "...kook...",
    "....kk....",
]


# ---------------------------------------------------------------------------
# Sprite sheet
# ---------------------------------------------------------------------------
def cell_xy(cell):
    """8x16 cell number (sprite tile number / 2) -> pixel position."""
    return (cell % 32) * 8, (cell // 32) * 16


def make_sprites():
    src = Image.open(os.path.join(IMG, 'original', 'sprites.png'))
    pal = src.getpalette()[:48]
    sheet = src.copy()
    d = ImageDraw.Draw(sheet)

    # Drop the old "Time Over" label and hourglass: the timer is gone.
    d.rectangle((0, 32, 255, 47), fill=0)

    def put(im, cell):
        sheet.paste(im, cell_xy(cell))

    # --- Recoloured drakes, built from the player's own frames ---------------
    frames = [src.crop((8 + 24 * i, 0, 32 + 24 * i, 16)).transpose(Image.FLIP_TOP_BOTTOM)
              for i in range(4)]
    fire = {'G': 'r', 'g': 'o', 'l': 'y', 'L': 'y', 'r': 'w', 'w': 'k'}
    frost = {'G': 'b', 'g': 'B', 'l': 'C', 'L': 'w', 'r': 'p'}
    bone = {'G': 'd', 'g': 'p', 'l': 'w', 'L': 'w', 'r': 'r', 'w': 'd'}
    for i, f in enumerate((0, 1)):
        put(remap(frames[f], fire), 36 + i * 3)      # tiles 72, 78
        put(remap(frames[f], frost), 64 + i * 3)     # tiles 128, 134
        put(remap(frames[f], bone), 70 + i * 3)      # tiles 140, 146

    # --- Small 16x16 enemies ---------------------------------------------------
    put(ascii_half(BAT_A), 76)        # tile 152
    put(ascii_half(BAT_B), 78)
    put(gazer(0), 80)                 # tile 160
    put(gazer(1), 82)
    put(ascii_half(SKULL_A), 84)      # tile 168
    put(ascii_half(SKULL_B), 86)
    put(ascii_half(WISP_A), 88)       # tile 176
    put(ascii_half(WISP_B), 90)
    put(turret(0), 92)                # tile 184
    put(turret(1), 94)

    # --- Bullets, HUD bits ------------------------------------------------------
    put(ascii_full(ORB_A), 42)        # tile 84
    put(ascii_full(ORB_B), 43)        # tile 86
    put(ascii_full(NEEDLE), 44)       # tile 88
    put(ascii_full(HEART), 45)        # tile 90
    put(ascii_full(HP_FULL), 29)      # tile 58
    put(ascii_full(HP_EMPTY), 30)     # tile 60
    put(ascii_full(SHARD), 31)        # tile 62

    # --- Explosion (3 frames) ----------------------------------------------------
    for i in range(3):
        put(explosion(i), 96 + i * 2)  # tiles 192, 196, 200

    # --- Extra pickups, drawn inside the "empty" icon ring -------------------------
    ring = src.crop((240, 16, 256, 32))
    one_up = ring.copy()
    one_up.paste(ascii_full(BIG_HEART), (3, 4))
    put(one_up, 102)                   # tile 204
    gem = ring.copy()
    gem.paste(ascii_full(GEM), (3, 4))
    put(gem, 104)                      # tile 208

    # --- Drop shadow under the score digits and the heart, so that they can be
    #     read over bright scenery ------------------------------------------------
    px = sheet.load()
    for (x0, y0, x1, y1) in ((176, 48, 256, 64), (104, 16, 112, 32)):
        for cx in range(x0, x1, 8):
            for y in range(y1 - 1, y0, -1):
                for x in range(cx + 7, cx, -1):
                    if px[x, y] == 0 and (px[x - 1, y - 1] not in (0, IDX['k'])):
                        px[x, y] = IDX['k']

    sheet.putpalette(pal + [0] * (768 - 48))
    sheet.save(os.path.join(IMG, 'sprites.png'))
    return sheet


# ---------------------------------------------------------------------------
# Bosses
# ---------------------------------------------------------------------------
def sms(r, g, b):
    return (r * 85, g * 85, b * 85)


# Front-facing animations from the Stendhal sprites (CC BY-SA 3.0), kept in
# data/img/original/stendhal. Each sheet has 3 walking frames per direction;
# row 2 faces the viewer.
STENDHAL = os.path.join(IMG, 'original', 'stendhal')

STENDHAL_BOSSES = [
    # source sheet, output picture
    ('green_dragon.png', 'boss_emerald_wyrm.png'),
    ('twin_head_dragon.png', 'boss_frost_hydra.png'),
    ('golden_death.png', 'boss_reaper.png'),
    ('bone_dragon.png', 'dracolich.png'),
]


def sms_level(c):
    return min(range(4), key=lambda i: abs(i * 85 - c))


def stendhal_boss(sheet, out_name, row=2, max_colours=15):
    """Converts the front-facing frames of a Stendhal sheet to a picture with
    the frames stacked vertically, in SMS colours."""
    im = Image.open(os.path.join(STENDHAL, sheet)).convert('RGBA')
    fw, fh = im.width // 3, im.height // 4
    frames = [im.crop((c * fw, row * fh, (c + 1) * fw, (row + 1) * fh)) for c in range(3)]

    # Common bounding box, grown to whole 8x8 tiles.
    box = None
    for f in frames:
        b = f.getbbox()
        box = b if box is None else (min(box[0], b[0]), min(box[1], b[1]), max(box[2], b[2]), max(box[3], b[3]))
    w = (box[2] - box[0] + 7) // 8 * 8
    h = (box[3] - box[1] + 7) // 8 * 8
    x0 = max(0, min(box[0] - (w - (box[2] - box[0])) // 2, fw - w))
    y0 = max(0, min(box[1] - (h - (box[3] - box[1])) // 2, fh - h))
    frames = [f.crop((x0, y0, x0 + w, y0 + h)) for f in frames]

    # Every pixel to the nearest of the 64 SMS colours; the rarest colours are
    # merged into their closest neighbours until 15 are left (+ the sky).
    counts = {}
    for f in frames:
        for r, g, b, a in f.getdata():
            if a >= 128:
                c = (sms_level(r), sms_level(g), sms_level(b))
                counts[c] = counts.get(c, 0) + 1
    pal = sorted(counts, key=lambda c: -counts[c])
    remap = {c: c for c in pal}
    while len(pal) > max_colours:
        victim = pal.pop()
        best = min(pal, key=lambda c: sum((p - q) ** 2 for p, q in zip(c, victim)))
        for k, v in remap.items():
            if v == victim:
                remap[k] = best
    pal.sort(key=lambda c: sum(c))
    index = {c: i + 1 for i, c in enumerate(pal)}

    out = Image.new('P', (w, h * 3), 0)
    for i, f in enumerate(frames):
        data = []
        for r, g, b, a in f.getdata():
            data.append(index[remap[(sms_level(r), sms_level(g), sms_level(b))]] if a >= 128 else 0)
        frame = Image.new('P', (w, h))
        frame.putdata(data)
        out.paste(frame, (0, i * h))
    flat = [0, 255, 255]
    for c in pal:
        flat += [v * 85 for v in c]
    out.putpalette(flat + [0] * (768 - len(flat)))
    out.save(os.path.join(IMG, out_name))
    return out


def make_stendhal_bosses():
    for sheet, name in STENDHAL_BOSSES:
        stendhal_boss(sheet, name)


def make_gazer_boss():
    W, H = 80, 64
    pal = [(0, 255, 255), sms(0, 0, 0), sms(1, 0, 1), sms(2, 0, 2), sms(3, 1, 3),
           sms(3, 3, 3), sms(2, 2, 3), sms(3, 0, 0), sms(3, 2, 0), sms(3, 3, 0),
           sms(1, 0, 0), sms(2, 0, 0)]
    K, D, M, L, WH, SH, R, O, Y, DR, MR = range(1, 12)
    out = Image.new('P', (W, H * 2), 0)
    for frame in range(2):
        im = Image.new('P', (W, H), 0)
        d = ImageDraw.Draw(im)
        cx, cy = W // 2, H // 2
        # spikes
        for i in range(12):
            a = i * math.pi / 6 + math.pi / 12
            tip = (cx + 39 * math.cos(a), cy + 31 * math.sin(a))
            l = (cx + 24 * math.cos(a - 0.22), cy + 24 * math.sin(a - 0.22))
            r = (cx + 24 * math.cos(a + 0.22), cy + 24 * math.sin(a + 0.22))
            d.polygon([tip, l, r], fill=M, outline=K)
        d.ellipse((cx - 28, cy - 28, cx + 28, cy + 28), fill=D, outline=K)
        d.ellipse((cx - 26, cy - 27, cx + 24, cy + 23), fill=M)
        d.ellipse((cx - 23, cy - 25, cx + 12, cy + 4), fill=L)
        d.ellipse((cx - 22, cy - 22, cx + 22, cy + 22), fill=M)
        # warts
        for (x, y) in ((-22, -14), (20, -16), (-18, 19), (17, 20), (0, -25), (24, 2), (-25, 3)):
            d.ellipse((cx + x - 2, cy + y - 2, cx + x + 2, cy + y + 2), fill=L, outline=D)
        if frame == 0:
            d.ellipse((cx - 20, cy - 15, cx + 20, cy + 15), fill=WH, outline=K)
            d.ellipse((cx - 17, cy + 2, cx + 17, cy + 14), fill=SH)
            d.ellipse((cx - 18, cy - 13, cx + 18, cy + 10), fill=WH)
            d.ellipse((cx - 11, cy - 11, cx + 11, cy + 11), fill=DR, outline=K)
            d.ellipse((cx - 9, cy - 9, cx + 9, cy + 9), fill=R)
            d.ellipse((cx - 7, cy - 7, cx + 7, cy + 7), fill=O)
            d.ellipse((cx - 3, cy - 9, cx + 3, cy + 9), fill=K)
            d.ellipse((cx - 8, cy - 8, cx - 5, cy - 5), fill=WH)
            # veins
            for sx in (-1, 1):
                d.line((cx + sx * 19, cy, cx + sx * 14, cy - 3), fill=R)
                d.line((cx + sx * 18, cy + 4, cx + sx * 13, cy + 6), fill=R)
        else:
            d.ellipse((cx - 20, cy - 15, cx + 20, cy + 15), fill=L, outline=K)
            d.ellipse((cx - 20, cy - 4, cx + 20, cy + 6), fill=WH, outline=K)
            d.ellipse((cx - 6, cy - 3, cx + 6, cy + 5), fill=R)
            d.ellipse((cx - 2, cy - 3, cx + 2, cy + 5), fill=K)
            d.arc((cx - 18, cy - 13, cx + 18, cy + 4), 200, 340, fill=D)
        out.paste(im, (0, frame * H))
    flat = []
    for c in pal:
        flat += list(c)
    out.putpalette(flat + [0] * (768 - len(flat)))
    out.save(os.path.join(IMG, 'boss_gazer.png'))


# ---------------------------------------------------------------------------
# Title logo
# ---------------------------------------------------------------------------
GLYPHS = {
    'D': ["####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####."],
    'R': ["####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"],
    'A': [".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    'G': [".####", "#....", "#....", "#.###", "#...#", "#...#", ".###."],
    'O': [".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    'N': ["#...#", "##..#", "##..#", "#.#.#", "#..##", "#..##", "#...#"],
    'B': ["####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."],
    'L': ["#....", "#....", "#....", "#....", "#....", "#....", "#####"],
    'S': [".####", "#....", "#....", ".###.", "....#", "....#", "####."],
    'T': ["#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."],
    'E': ["#####", "#....", "#....", "####.", "#....", "#....", "#####"],
}


def make_logo():
    W, H = 256, 88
    # entry 6 is white: it doubles as the text colour of the title screen
    pal = [sms(0, 0, 0), sms(0, 0, 0), sms(3, 0, 0), sms(3, 3, 0), sms(3, 2, 0),
           sms(3, 1, 0), sms(3, 3, 3), sms(2, 0, 0), sms(1, 0, 0), sms(1, 1, 1),
           sms(0, 1, 2), sms(1, 2, 3)]
    im = Image.new('P', (W, H), 0)
    px = im.load()
    ramp = [6, 3, 3, 4, 4, 5, 2]

    def word(text, y0, scale_x, scale_y, colors):
        width = len(text) * 6 * scale_x - scale_x
        x0 = (W - width) // 2
        mask = Image.new('1', (W, H), 0)
        m = mask.load()
        for i, ch in enumerate(text):
            g = GLYPHS[ch]
            for gy in range(7):
                for gx in range(5):
                    if g[gy][gx] == '#':
                        for yy in range(scale_y):
                            for xx in range(scale_x):
                                m[x0 + (i * 6 + gx) * scale_x + xx, y0 + gy * scale_y + yy] = 1
        # drop shadow + outline, then the gradient face
        for y in range(H):
            for x in range(W):
                if m[x, y]:
                    for dx in range(-2, 5):
                        for dy in range(-2, 5):
                            nx, ny = x + dx, y + dy
                            if 0 <= nx < W and 0 <= ny < H and px[nx, ny] == 0:
                                px[nx, ny] = 9 if (dx > 2 or dy > 2) else 8
        for y in range(H):
            for x in range(W):
                if m[x, y]:
                    row = (y - y0) // scale_y
                    c = colors[row]
                    # top-left highlight on every block
                    if (y - y0) % scale_y == 0 and row < 3:
                        c = 6
                    px[x, y] = c

    word('DRAGON', 4, 6, 5, ramp)
    word('BLASTER', 48, 5, 5, ramp)
    # shadow pixels (9) were only placeholders to mark the far edge: make them black
    im = im.point([0 if i == 9 else i for i in range(256)])
    flat = []
    for c in pal:
        flat += list(c)
    im.putpalette(flat + [0] * (768 - len(flat)))
    im.save(os.path.join(IMG, 'title_logo.png'))


# ---------------------------------------------------------------------------
# Stage tilesets, cut out of the DawnLike "Floor.png"
# ---------------------------------------------------------------------------
DAWN = {
    'black': (20, 12, 28), 'maroon': (69, 36, 52), 'navy': (48, 52, 109), 'dgrey': (77, 73, 77),
    'brown': (134, 77, 48), 'dgreen': (52, 101, 36), 'red': (211, 69, 73), 'grey': (117, 113, 97),
    'blue': (89, 125, 207), 'orange': (211, 125, 44), 'lgrey': (134, 150, 162), 'green': (109, 170, 44),
    'pink': (211, 170, 154), 'cyan': (109, 195, 203), 'yellow': (219, 215, 93), 'white': (223, 239, 215),
}

# name: (ground colour, speckle colour, blocks A B C, colour translation)
# Blocks are (column, row) in Floor.png; every block of a stage must sit on the
# same ground colour.
TILESETS = {
    'plains': dict(
        ground='pink', speckle='orange', blocks=[(1, 0), (2, 0), (2, 4)],
        colors={'pink': (3, 3, 2), 'orange': (3, 2, 0), 'green': (1, 2, 0), 'blue': (0, 3, 2),
                'grey': (1, 1, 1), 'yellow': (3, 3, 1), 'white': (1, 2, 3)}),
    'desert': dict(
        ground='orange', speckle='brown', blocks=[(0, 4), (2, 1), (1, 1)],
        colors={'orange': (3, 2, 1), 'brown': (2, 1, 0), 'pink': (3, 3, 2), 'yellow': (3, 3, 3),
                'grey': (2, 1, 1), 'green': (1, 2, 0), 'dgreen': (0, 1, 0)}),
    'frozen': dict(
        ground='brown', speckle='lgrey', blocks=[(2, 6), (1, 2), (2, 2)],
        colors={'brown': (2, 3, 3), 'lgrey': (3, 3, 3), 'cyan': (0, 2, 3), 'blue': (0, 1, 3),
                'dgrey': (1, 2, 2), 'grey': (0, 2, 1), 'dgreen': (0, 1, 1), 'red': (2, 2, 3),
                'maroon': (1, 1, 2)}),
    'fortress': dict(
        ground='dgrey', speckle='maroon', blocks=[(1, 6), (2, 3), (0, 6)],
        colors={'dgrey': (1, 1, 1), 'maroon': (0, 0, 0), 'grey': (2, 2, 2), 'brown': (3, 1, 0),
                'navy': (0, 0, 1), 'red': (3, 3, 0)}),
    'lair': dict(
        ground='navy', speckle='maroon', blocks=[(0, 3), (1, 7), (0, 7)],
        colors={'navy': (1, 0, 1), 'maroon': (0, 0, 0), 'dgrey': (2, 0, 2), 'brown': (3, 0, 0),
                'black': (0, 0, 0)}),
}


def nearest_sms(rgb):
    return tuple(min(3, max(0, int(round(c / 85.0)))) for c in rgb)


def make_tileset(name, spec):
    floor = Image.open(os.path.join(IMG, 'Floor.png')).convert('RGBA')
    trans = {}
    for k, rgb in DAWN.items():
        trans[rgb] = spec['colors'].get(k, nearest_sms(rgb))
    ground = trans[DAWN[spec['ground']]]
    speck = trans[DAWN[spec['speckle']]]
    out = Image.new('RGB', (256, 48), sms(*ground))

    def tile(block, tx, ty):
        bx, by = block
        x0, y0 = bx * 112 + tx * 16, 48 + by * 48 + ty * 16
        t = Image.new('RGB', (16, 16), sms(*ground))
        for y in range(16):
            for x in range(16):
                r, g, b, a = floor.getpixel((x0 + x, y0 + y))
                if a:
                    t.putpixel((x, y), sms(*trans.get((r, g, b), nearest_sms((r, g, b)))))
        return t

    def put(t, col, row):
        out.paste(t, (col * 16, row * 16))

    a, b, c = spec['blocks']
    for row in range(3):
        for col in range(3):
            put(tile(a, col, row), col, row)
            put(tile(b, col, row), 5 + col, row)
            put(tile(c, col, row), 9 + col, row)
        put(tile(a, 3, row), 3, row)
        put(tile(b, 3, row), 8, row)
    for col in range(3):
        put(tile(a, 4 + col, 1), 12 + col, 0)
        put(tile(b, 4 + col, 1), 12 + col, 1)
    put(tile(a, 5, 0), 15, 0)
    put(tile(b, 5, 0), 15, 1)
    put(tile(c, 5, 0), 12, 2)

    # Synthesised ground tiles: plain, speckled, pebbles
    rnd = random.Random(hash(name) & 0xFFFF)
    rnd = random.Random(sum(ord(ch) for ch in name))
    for row, n in enumerate((5, 12, 20)):
        t = Image.new('RGB', (16, 16), sms(*ground))
        for _ in range(n):
            x, y = rnd.randint(0, 15), rnd.randint(0, 15)
            t.putpixel((x, y), sms(*speck))
            if row == 2 and rnd.random() < 0.5:
                t.putpixel(((x + 1) & 15, y), sms(*speck))
                t.putpixel((x, (y + 1) & 15), sms(*speck))
        put(t, 4, row)

    # Quantise to an indexed image whose palette entry 0 is the ground colour.
    colors = [sms(*ground)]
    for px in out.getdata():
        if px not in colors:
            colors.append(px)
    assert len(colors) <= 16, (name, len(colors))
    pim = Image.new('P', out.size, 0)
    pim.putdata([colors.index(px) for px in out.getdata()])
    flat = []
    for cc in colors:
        flat += list(cc)
    pim.putpalette(flat + [0] * (768 - len(flat)))
    pim.save(os.path.join(IMG, 'tileset_%s.png' % name))
    return pim


def main():
    make_sprites()
    make_stendhal_bosses()
    make_gazer_boss()
    make_logo()
    for name, spec in TILESETS.items():
        make_tileset(name, spec)
    print('Art regenerated in', os.path.normpath(IMG))


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""
Tiny MML -> PSGlib (.psg) compiler for the SMS sound chip (SN76489), plus the
compiler for the frame-by-frame sound effect scripts.

Music files (data/music/*.mml)
------------------------------
    # comment (whole line), or ; comment at the end of a line
    @tempo 140              quarter notes per minute
    @loop off               play once (jingles); default is to loop forever
    @e1 = 15 14 12 10 9 8   volume envelope, one level (0..15) per frame;
                            the last level is held until the note ends
    A  o4 l8 v12 @e1  c d e f  g4 r4      channel A (first square wave)
    B  ...                                 channel B (second square wave)
    C  ...                                 channel C (third square wave)
    D  ...                                 channel D (noise: c = low thump,
                                           e = mid snare, g = high hat)

Commands inside a channel:
    c d e f g a b   note; add + or # for sharp, - for flat, then an optional
                    length (4 = quarter, 8 = eighth, ...) and dots
    r               rest
    o4  < >         set octave / octave down / octave up
    l8              default note length
    v12             volume 0..15
    @e2             select envelope
    q6              gate: play 6/8 of every note, then silence (q8 = legato)
    &               tie the previous note to the next one
    [ ... ]4        repeat a block 4 times
    L               loop point (the song restarts from here)
    |               bar line, ignored

Sound effects (data/sfx/*.sfx)
------------------------------
    channels 3              2 = tone channel, 3 = noise channel, 23 = both
    priority 2              higher priority effects are not interrupted
    tone 440 15             one frame: frequency in Hz, volume 0..15
    sweep 880 220 15 0 12   slide from 880 Hz/vol 15 to 220 Hz/vol 0 in 12 frames
    noise 1 15              one frame: noise rate 0 (high)..2 (low), volume
    nsweep 0 15 0 20        noise rate, volume from..to, frames
    wait 3                  keep the current sound for 3 more frames
    A line may hold a tone part and a noise part separated by '+'.
"""
import re
import sys

CLOCK = 3579545.0
NOTE_OFFSETS = {'c': 0, 'd': 2, 'e': 4, 'f': 5, 'g': 7, 'a': 9, 'b': 11}
TICKS_PER_WHOLE = 192


class MMLError(Exception):
    pass


def note_period(midi):
    f = 440.0 * 2 ** ((midi - 69) / 12.0)
    period = int(round(CLOCK / (32.0 * f)))
    if period > 1023:
        raise MMLError('a note is too low for the sound chip (the lowest one is "o2 a")')
    return max(1, period)


def hz_period(hz):
    return max(1, min(1023, int(round(CLOCK / (32.0 * hz)))))


# ---------------------------------------------------------------------------
# Frame list -> PSG byte stream
# ---------------------------------------------------------------------------
class Stream:
    """Turns per-frame channel states into the PSGlib byte format."""

    def __init__(self, channels=(0, 1, 2, 3)):
        self.channels = channels
        self.out = bytearray()
        self.forget()
        self.pending_waits = 0

    def forget(self):
        self.period = {c: None for c in range(4)}
        self.volume = {c: None for c in range(4)}

    def flush_waits(self):
        n = self.pending_waits
        while n > 0:
            k = min(n, 8)
            self.out.append(0x38 + k - 1)
            n -= k
        self.pending_waits = 0

    def frame(self, state):
        """state: {channel: (period_or_noise_mode, volume)}"""
        data = bytearray()
        for ch in self.channels:
            if ch not in state:
                continue
            period, vol = state[ch]
            if period is not None and vol > 0 and period != self.period[ch]:
                if ch == 3:
                    data.append(0xE0 | (period & 0x07))
                else:
                    data.append(0x80 | (ch << 5) | (period & 0x0F))
                    data.append(0x40 | ((period >> 4) & 0x3F))
                self.period[ch] = period
            if vol != self.volume[ch]:
                data.append(0x90 | (ch << 5) | (15 - vol))
                self.volume[ch] = vol
        if data:
            self.flush_waits()
            self.out += data
        self.pending_waits += 1

    def mark_loop(self):
        self.flush_waits()
        self.out.append(0x01)
        self.forget()

    def finish(self):
        self.flush_waits()
        self.out.append(0x00)
        return bytes(self.out)


def compress(data):
    """PSGlib substring compression: 0x08..0x37 = copy (n-4) bytes from offset."""
    out = bytearray()
    literal = []          # literal[i] is True if out[i] may be part of a match
    i = 0
    n = len(data)
    while i < n:
        b = data[i]
        if b in (0x00, 0x01):
            out.append(b)
            literal.append(False)
            i += 1
            continue
        best_len, best_pos = 0, -1
        max_len = 0
        while i + max_len < n and max_len < 51 and data[i + max_len] not in (0x00, 0x01):
            max_len += 1
        if max_len >= 4:
            key = bytes(data[i:i + 4])
            pos = out.find(key)
            while pos >= 0:
                l = 0
                while (l < max_len and pos + l < len(out) and literal[pos + l]
                       and out[pos + l] == data[i + l]):
                    l += 1
                if l > best_len:
                    best_len, best_pos = l, pos
                    if l == max_len:
                        break
                pos = out.find(key, pos + 1)
        if best_len >= 5:
            out.append(0x08 + best_len - 4)
            out.append(best_pos & 0xFF)
            out.append(best_pos >> 8)
            literal += [False, False, False]
            i += best_len
        else:
            out.append(b)
            literal.append(True)
            i += 1
    return bytes(out)


# ---------------------------------------------------------------------------
# MML
# ---------------------------------------------------------------------------
class Note:
    __slots__ = ('start', 'length', 'pitch', 'volume', 'env', 'gate')

    def __init__(self, start, length, pitch, volume, env, gate):
        self.start, self.length, self.pitch = start, length, pitch
        self.volume, self.env, self.gate = volume, env, gate


def parse_channel(text, name, envelopes):
    pos = 0
    octave, default_len, volume, env, gate = 4, TICKS_PER_WHOLE // 4, 12, None, 8
    tick = 0
    notes = []
    loop_tick = None
    tie = False

    def read_int():
        nonlocal pos
        m = re.match(r'\d+', text[pos:])
        if not m:
            return None
        pos += m.end()
        return int(m.group())

    def read_length():
        nonlocal pos
        n = read_int()
        length = default_len if n is None else TICKS_PER_WHOLE // n
        add = length
        while pos < len(text) and text[pos] == '.':
            add //= 2
            length += add
            pos += 1
        return length

    # expand [ ... ]n repeats first
    def expand(s):
        while True:
            m = re.search(r'\[([^\[\]]*)\](\d+)', s)
            if not m:
                if '[' in s or ']' in s:
                    raise MMLError('channel %s: unbalanced [ ]' % name)
                return s
            s = s[:m.start()] + (m.group(1) + ' ') * int(m.group(2)) + s[m.end():]

    text = expand(text)
    while pos < len(text):
        c = text[pos]
        pos += 1
        if c in ' \t|':
            continue
        lc = c.lower()
        if lc in NOTE_OFFSETS and c != 'L':
            semis = NOTE_OFFSETS[lc]
            while pos < len(text) and text[pos] in '+#-':
                semis += -1 if text[pos] == '-' else 1
                pos += 1
            length = read_length()
            pitch = 12 * (octave + 1) + semis
            if tie and notes and notes[-1].pitch == pitch:
                notes[-1].length += length
            else:
                notes.append(Note(tick, length, pitch, volume, env, gate))
            tie = False
            tick += length
        elif lc == 'r':
            tick += read_length()
            tie = False
        elif c == 'o':
            octave = read_int()
        elif c == '>':
            octave += 1
        elif c == '<':
            octave -= 1
        elif c == 'l':
            default_len = read_length()
        elif c == 'v':
            volume = read_int()
        elif c == 'q':
            gate = read_int()
        elif c == '&':
            tie = True
        elif c == 'L':
            loop_tick = tick
        elif c == '@':
            if text[pos] != 'e':
                raise MMLError('channel %s: unknown command @%s' % (name, text[pos]))
            pos += 1
            env = read_int()
            if env not in envelopes:
                raise MMLError('channel %s: envelope @e%s is not defined' % (name, env))
        else:
            raise MMLError('channel %s: unexpected "%s" near "%s"' % (name, c, text[max(0, pos - 12):pos + 8]))
    return notes, tick, loop_tick


def compile_mml(source, name='song'):
    tempo = 120
    loop = True
    envelopes = {}
    channels = {'A': '', 'B': '', 'C': '', 'D': ''}
    for raw in source.splitlines():
        line = '' if raw.lstrip().startswith('#') else raw.split(';')[0].rstrip()
        if not line.strip():
            continue
        m = re.match(r'\s*@tempo\s+(\d+)', line)
        if m:
            tempo = int(m.group(1))
            continue
        m = re.match(r'\s*@loop\s+(\w+)', line)
        if m:
            loop = m.group(1).lower() not in ('off', 'no', '0')
            continue
        m = re.match(r'\s*@e(\d+)\s*=\s*(.*)', line)
        if m:
            envelopes[int(m.group(1))] = [int(v) for v in m.group(2).split()]
            continue
        m = re.match(r'\s*([ABCD]+)\s+(.*)', line)
        if m:
            for ch in m.group(1):
                channels[ch] += ' ' + m.group(2)
            continue
        raise MMLError('%s: cannot understand line: %s' % (name, raw))

    parsed = {}
    total = 0
    loop_tick = None
    for ch, text in channels.items():
        if not text.strip():
            continue
        notes, length, lt = parse_channel(text, ch, envelopes)
        parsed[ch] = notes
        if total and length != total:
            raise MMLError('%s: channel %s is %d ticks long, but the others are %d '
                           '(192 ticks = one whole note)' % (name, ch, length, total))
        total = max(total, length)
        if lt is not None:
            if loop_tick is not None and lt != loop_tick:
                raise MMLError('%s: the L loop points do not line up' % name)
            loop_tick = lt
    if not parsed:
        raise MMLError('%s: no channel data' % name)

    frames_per_tick = 3600.0 / (tempo * (TICKS_PER_WHOLE / 4))

    def frame_of(tick):
        return int(round(tick * frames_per_tick))

    total_frames = frame_of(total)
    # per channel, per frame state
    states = [dict() for _ in range(total_frames)]
    for ch, notes in parsed.items():
        num = 'ABCD'.index(ch)
        for f in range(total_frames):
            states[f][num] = (None, 0)
        for note in notes:
            f0 = frame_of(note.start)
            f1 = frame_of(note.start + note.length)
            sounding = max(1, ((f1 - f0) * note.gate) // 8) if note.gate < 8 else (f1 - f0)
            if num == 3:
                semis = note.pitch % 12
                period = 0x04 | (2 if semis < 4 else (1 if semis < 7 else 0))
            else:
                period = note_period(note.pitch)
            env = envelopes.get(note.env)
            for f in range(f0, min(f1, total_frames)):
                k = f - f0
                if k >= sounding:
                    vol = 0
                elif env:
                    vol = (note.volume * env[min(k, len(env) - 1)] + 7) // 15
                else:
                    vol = note.volume
                states[f][num] = (period, vol)

    stream = Stream()
    loop_frame = frame_of(loop_tick) if loop_tick is not None else 0
    for f in range(total_frames):
        if loop and f == loop_frame:
            stream.mark_loop()
        stream.frame(states[f])
    if not loop:
        # leave everything silent at the end of a jingle
        stream.frame({c: (None, 0) for c in range(4)})
    data = stream.finish()
    return compress(data), loop, total_frames


# ---------------------------------------------------------------------------
# SFX
# ---------------------------------------------------------------------------
def compile_sfx(source, name='sfx'):
    channels = 3
    priority = 1
    frames = []          # list of {ch: (period, vol)}

    def lerp(a, b, i, n):
        return a + (b - a) * i / float(max(1, n - 1))

    def part_frames(words):
        cmd = words[0]
        args = [float(w) for w in words[1:]]
        if cmd == 'tone':
            return [{2: (hz_period(args[0]), int(args[1]))}]
        if cmd == 'sweep':
            f0, f1, v0, v1, n = args
            n = int(n)
            return [{2: (hz_period(lerp(f0, f1, i, n)), int(round(lerp(v0, v1, i, n))))} for i in range(n)]
        if cmd == 'noise':
            return [{3: (0x04 | int(args[0]), int(args[1]))}]
        if cmd == 'nsweep':
            rate, v0, v1, n = args
            n = int(n)
            return [{3: (0x04 | int(rate), int(round(lerp(v0, v1, i, n))))} for i in range(n)]
        if cmd == 'wait':
            return [{} for _ in range(int(args[0]))]
        raise MMLError('%s: unknown sfx command "%s"' % (name, cmd))

    for raw in source.splitlines():
        line = '' if raw.lstrip().startswith('#') else raw.split(';')[0].strip()
        if not line:
            continue
        words = line.split()
        if words[0] == 'channels':
            channels = {'2': 1, '3': 2, '23': 3}[words[1]]
            continue
        if words[0] == 'priority':
            priority = int(words[1])
            continue
        parts = [part_frames(p.split()) for p in line.split('+')]
        n = max(len(p) for p in parts)
        for i in range(n):
            merged = {}
            for p in parts:
                if i < len(p):
                    merged.update(p[i])
            frames.append(merged)

    used = [c for c, bit in ((2, 1), (3, 2)) if channels & bit]
    stream = Stream(channels=used)
    for st in frames:
        stream.frame({c: st[c] for c in st if c in used})
    stream.frame({c: (None, 0) for c in used})
    return stream.finish(), channels, priority


if __name__ == '__main__':
    src, dst = sys.argv[1:3]
    with open(src) as f:
        text = f.read()
    if src.endswith('.sfx'):
        data = compile_sfx(text, src)[0]
    else:
        data = compile_mml(text, src)[0]
    with open(dst, 'wb') as f:
        f.write(data)
    print('%s: %d bytes' % (dst, len(data)))

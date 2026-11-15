#!/usr/bin/env python3
"""Fails the build if the program no longer fits in the fixed 32 KB of ROM
or if the variables run into the stack. Reads the linker's .map file."""
import re
import sys

CODE_LIMIT = 0x7FE0      # ihx2sms writes the SDSC/SEGA headers from here on
RAM_LIMIT = 0xDE00       # the stack grows down from 0xDFF0

areas = {}
for line in open(sys.argv[1]):
    m = re.match(r'^(_\w+)\s+([0-9A-F]{8})\s+([0-9A-F]{8})\s+=', line)
    if m:
        areas[m.group(1)] = (int(m.group(2), 16), int(m.group(3), 16))

code_end = max(a + s for n, (a, s) in areas.items() if a < 0x8000 and s and not n.startswith('_BANK'))
ram_end = max(a + s for n, (a, s) in areas.items() if 0xC000 <= a < 0xE000 and s)
print('fixed ROM: %d bytes used, %d free   RAM: %d bytes used, %d free' % (
    code_end, CODE_LIMIT - code_end, ram_end - 0xC000, RAM_LIMIT - ram_end))
if code_end > CODE_LIMIT:
    sys.exit('ERROR: the code is %d bytes too big for the fixed ROM area' % (code_end - CODE_LIMIT))
if ram_end > RAM_LIMIT:
    sys.exit('ERROR: too many variables: %d bytes over the limit' % (ram_end - RAM_LIMIT))

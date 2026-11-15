# ---------------------------------------------------------------------------
# Dragon Blaster - Sega Master System
#
# Needs: SDCC 4.2 or newer, GNU make, Python 3 with Pillow, and the devkitSMS
# tool "ihx2sms" on the PATH. SMSlib and PSGlib are bundled in lib/.
#
#   make          builds dragon_blaster.sms
#   make test     runs the automated playtest (needs smstest)
#   make clean
# ---------------------------------------------------------------------------
PRJNAME := dragon_blaster
PYTHON  ?= python3

SRCS := actor.c shot.c shots.c bullet.c enemy.c boss.c map.c score.c sound.c screens.c dragon_blaster.c
OBJS := $(SRCS:.c=.rel) gen/tables.rel

CFLAGS  := -mz80 --opt-code-size --peep-file lib/peep-rules.txt
LDFLAGS := -mz80 --no-std-crt0 --data-loc 0xC000

# Everything the asset pipeline reads.
ASSETS := data/game.ini $(wildcard data/img/*.png data/map/*.tmx data/path/* data/music/*.mml \
            data/sfx/*.sfx data/text/*.txt) $(wildcard tool/*.py)

all: $(PRJNAME).sms

# gen/banks.mk lists the ROM banks the asset pipeline filled (BANKS, BANKFLAGS).
-include gen/banks.mk
BANKRELS := $(foreach n,$(BANKS),gen/bank$(n).rel)

# One run of the pipeline writes all of gen/.
gen/banks.mk: $(ASSETS)
	$(PYTHON) tool/build_assets.py

gen/assets.h gen/tables.c: gen/banks.mk

%.rel: %.c gen/banks.mk $(wildcard *.h)
	sdcc -c $(CFLAGS) $< -o $@

gen/bank%.rel: gen/bank%.c
	sdcc -c -mz80 --constseg BANK$* $< -o $@

.PHONY: all clean test $(PRJNAME).ihx
$(PRJNAME).ihx: $(OBJS) $(BANKRELS)
	sdcc -o $(PRJNAME).ihx $(LDFLAGS) $(BANKFLAGS) lib/crt0_sms.rel $(OBJS) lib/SMSlib.lib lib/PSGlib.lib $(BANKRELS)
	$(PYTHON) tool/check_rom.py $(PRJNAME).map

$(PRJNAME).sms: $(PRJNAME).ihx
	ihx2sms $(PRJNAME).ihx $(PRJNAME).sms

test: $(PRJNAME).sms
	smstest $(PRJNAME).sms test/playtest.txt

clean:
	rm -f *.sms *.sav *.asm *.sym *.rel *.noi *.map *.lst *.lk *.ihx *.rst
	rm -rf gen

PRJNAME := dragon_blaster
OBJS := data.rel actor.rel shot.rel dragon_blaster.rel

all: $(PRJNAME).sms

data.c: data/* data/sprites_tiles.psgcompr data/background_tiles.psgcompr data/path1.path
	folder2c data data
	
data/sprites_tiles.psgcompr: data/img/sprites.png
	BMP2Tile.exe data/img/sprites.png -noremovedupes -8x16 -palsms -fullpalette -savetiles data/sprites_tiles.psgcompr -savepalette data/sprites_palette.bin

data/background_tiles.psgcompr: data/img/background.png
	BMP2Tile.exe data/img/background.png -palsms -fullpalette -savetiles data/background_tiles.psgcompr -savetilemap data/background_tilemap.bin -savepalette data/background_palette.bin
	
data/path1.path: data/path/path1.spline.json
	node tool/convert_splines.js data/path/path1.spline.json data/path1.path
	
%.vgm: %.wav
	psgtalk -r 512 -u 1 -m vgm $<

%.rel : %.c
	sdcc -c -mz80 --peep-file lib/peep-rules.txt $<

$(PRJNAME).sms: $(OBJS)
	sdcc -o $(PRJNAME).ihx -mz80 --no-std-crt0 --data-loc 0xC000 lib/crt0_sms.rel $(OBJS) SMSlib.lib lib/PSGlib.rel
	ihx2sms $(PRJNAME).ihx $(PRJNAME).sms	

clean:
	rm *.sms *.sav *.asm *.sym *.rel *.noi *.map *.lst *.lk *.ihx data.*

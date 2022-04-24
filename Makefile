PRJNAME := dragon_blaster
OBJS := data.rel actor.rel shot.rel boss_shot.rel shots.rel map.rel score.rel dragon_blaster.rel

all: $(PRJNAME).sms

data.c: data/* data/sprites_tiles.psgcompr data/tileset_tiles.psgcompr data/background_tiles.psgcompr data/dracolich_tiles.psgcompr \
		data/path1.path data/level1.bin
	folder2c data data
	
data/sprites_tiles.psgcompr: data/img/sprites.png
	BMP2Tile.exe data/img/sprites.png -noremovedupes -8x16 -palsms -fullpalette -savetiles data/sprites_tiles.psgcompr -savepalette data/sprites_palette.bin
	
data/tileset_tiles.psgcompr: data/img/tileset.png
	BMP2Tile.exe data/img/tileset.png -noremovedupes -8x16 -palsms -fullpalette -savetiles data/tileset_tiles.psgcompr -savepalette data/tileset_palette.bin

data/background_tiles.psgcompr: data/img/background.png
	BMP2Tile.exe data/img/background.png -palsms -fullpalette -savetiles data/background_tiles.psgcompr -savetilemap data/background_tilemap.bin -savepalette data/background_palette.bin
	
data/dracolich_tiles.psgcompr: data/img/dracolich.png
	BMP2Tile.exe data/img/dracolich.png -palsms -fullpalette -savetiles data/dracolich_tiles.psgcompr -savetilemap data/dracolich_tilemap.bin -savepalette data/dracolich_palette.bin

data/%.path: data/path/%.spline.json
	node tool/convert_splines.js $< $@

data/%.bin: data/map/%.tmx
	node tool/convert_map.js $< $@
	
%.vgm: %.wav
	psgtalk -r 512 -u 1 -m vgm $<

%.rel : %.c
	sdcc -c -mz80 --peep-file lib/peep-rules.txt $<

$(PRJNAME).sms: $(OBJS)
	sdcc -o $(PRJNAME).ihx -mz80 --no-std-crt0 --data-loc 0xC000 lib/crt0_sms.rel $(OBJS) SMSlib.lib lib/PSGlib.rel
	ihx2sms $(PRJNAME).ihx $(PRJNAME).sms	

clean:
	rm *.sms *.sav *.asm *.sym *.rel *.noi *.map *.lst *.lk *.ihx data.*

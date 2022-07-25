const fs = require('fs');
const tmx = require('tmx-parser');
const {groupBy, mapValues} = require('lodash');

const arguments = process.argv.slice(2);
const [ sourcePath, destPath ] = arguments;

const mustBeTrue = (value, message) => {
	if (!value) {
		throw new Error(message);
	}
}; 

const validateMap = map => {
  mustBeTrue(map.orientation === 'orthogonal', 'This is not an orthogonal map.');
  mustBeTrue(map.width === 16, 'The width must be 16.');
  mustBeTrue(map.layers, 'There must be layers.');
  mustBeTrue(map.layers.length > 0, 'There must be at least one layer.');
}

const firstLayerOfType = (map, layerType) => map.layers.filter(({type}) => type == layerType)[0];

const getFirstGid = (map, gid) => gid && Math.max.apply(Math, 
	map.tileSets
		.map(({ firstGid }) => firstGid)
		.filter(firstGid => gid >= firstGid));

const processBackground = map => {
	const layer = firstLayerOfType(map, 'tile');
	const rows = [];
	for (let y = 0; y < map.height; y++) {
		const row = [];				
		for (let x = 0; x < map.width; x++) {
			row.push(layer.tileAt(x, y).id);
		}
		
		rows.push(row);
	}
	
	const uncompressed = rows.slice().reverse();
	const compressed = uncompressed.map(row => {
		return row
			.reduce((groups, tile) => {
				const lastGroup = groups[groups.length - 1];
				if (lastGroup && lastGroup.tile === tile) {
					lastGroup.count++;
				} else {
					groups.push({ tile, count: 1 });
				}
				return groups;
			}, [])
			.map(({ tile, count }) => count > 1 ? [ 0x80 | count, tile ] : [ tile ])
			.flat();
	});
	
	return compressed;
}

const processSprites = map => {
	const layer = firstLayerOfType(map, 'object');
	if (!layer) {
		console.warn('No object layer found!');
		return {};
	}
	
	const grouped = groupBy(layer.objects.map(object => {
		const rowNumber = Math.floor(object.y / 16);
		const colNumber = Math.floor(object.x / 16);
		const tileId = object.gid && object.gid - getFirstGid(map, object.gid);
		return {rowNumber, colNumber, tileId};
	}), 'rowNumber');
	
	const encoded = mapValues(grouped, lineObjs => lineObjs.map(({colNumber, tileId}) => [colNumber & 0x3F | 0x40, tileId]).flat());
	
	return encoded;
}

tmx.parseFile(sourcePath, function(err, map) {
	if (err) throw err;

	validateMap(map);
	
	const compressedRows = processBackground(map);
	const spritesPerRow = processSprites(map);
	
	const combinedRows = compressedRows.map((rowTiles, rowNumber) => {
		const rowSprites = spritesPerRow[compressedRows.length - rowNumber - 1] || [];
		return [...rowSprites, ...rowTiles];
	});
	
	const outputArray = Int8Array.from([...combinedRows.flat(), 255]);
	fs.writeFile(destPath, Buffer.from(outputArray), (err) => {
		if (err) throw err;
	});	
});

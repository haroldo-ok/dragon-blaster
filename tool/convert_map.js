const fs = require('fs');
const tmx = require('tmx-parser');

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

tmx.parseFile(sourcePath, function(err, map) {
	if (err) throw err;

	validateMap(map);

	const layer = map.layers[0];
	const rows = [];
	for (let y = 0; y < map.height; y++) {
		const row = [];				
		for (let x = 0; x < map.width; x++) {
			row.push(layer.tileAt(x, y).id);
		}
		
		rows.push(row);
	}
	
	const outputArray = Int8Array.from(rows.slice().reverse().flat());
	fs.writeFile(destPath, Buffer.from(outputArray), (err) => {
		if (err) throw err;
	});	
});

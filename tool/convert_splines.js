// Converts a spline saved by http://www.haroldo-ok.com/cardinal-spline-js/demos/editor into a format compatible with the engine
const fs = require('fs');

const arguments = process.argv.slice(2);
const [ sourcePath, destPath ] = arguments;

fs.readFile(sourcePath, 'utf8', (err, data) => {
	if (err) throw err;
	
	const { calculatedPoints } = JSON.parse(data);
	const prev = { ...calculatedPoints[0] };
	const deltas = calculatedPoints.map(({x, y}) => {
		const calc = (n, prevN) => {
			const res = Math.round((n - prevN) / 2);
			return res == -0 ? 0 : res;
		};
		
		const next = {
			x: calc(x, prev.x),
			y: calc(y, prev.y)
		};
		
		prev.x = x;
		prev.y = y;
		
		return next;
	});
	deltas.push({ x: -128, y: -128 });
	
	const outputArray = Int8Array.from(deltas.map(({ x, y }) => [x, y]).flat());
	fs.writeFile(destPath, Buffer.from(outputArray), (err) => {
		if (err) throw err;
	});	

});


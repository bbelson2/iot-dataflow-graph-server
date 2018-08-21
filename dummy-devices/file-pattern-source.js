/*
//  IoT Dataflow Graph Server
//  Copyright (c) 2015, Adam Rehn & Bruce Belson
//  
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//  
//  The above copyright notice and this permission notice shall be included in all
//  copies or substantial portions of the Software.
//  
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//  SOFTWARE.
*/

/*
	This is a dummy device for testing the IoT Dataflow Graph Server.
	Run it at node command line as follows:

	node file-pattern-source.js [file-name, [source-name]]

	If source-name is omitted, then DEFAULT_SOURCE_ID will be used.
	If file-name is omitted, then DEFAULT_SOURCE_FILE (in the current directory) will be used).

	Ensure that the dummy source is registered in server/nodes/sources.json, e.g.:

	// Default values
	{
		"id":    "file-source",
		"label": "File Source"
	},
	// Values if the script is run as, e.g.:
	//   node file-pattern-source.js pattern2.txt file-source2
	{
		"id":    "file-source2",
		"label": "File Source 2"
	}

	The sourcefile should contain one row for each data item. 
	Each row contains a space separated pair: value ticks.
	The script iterates through the file, and transmits the value for ticks milliseconds.
	Thus the following file would transmit 100 for 1 second and then 0 for 500 ms:

	100 1000
	0 500

	At the end of the file, the script releads the file and starts again. 
*/
var dgram = require('dgram');
var sock = dgram.createSocket('udp4');

//The default source identifier
DEFAULT_SOURCE_ID = 'file-source';

//The file to read from
DEFAULT_SOURCE_FILE = 'pattern.txt'

//Fixed settings, do not modify
MULTICAST_ADDRESS = '224.0.0.114';
MULTICAST_PORT    = 7070;

TICK_LENGTH = 10;
DEFAULT_PAIR = [0, 100];

var fs = require('fs');

var parseFile = function(fileName) {
	var contents = fs.readFileSync(fileName).toString();
	var lines = contents.split('\n');
	var values = lines.map(function(line) {
		var parts = line.split(' ');
		var pair = parts.map(p => isNaN(parseInt(p)) ? 0 : parseInt(p)).slice(0, 2);
		return (pair.length == 2) ? pair : DEFAULT_PAIR;
	});
	if (values.length == 0) {
		values = [ DEFAULT_PAIR ];
	}
	return values;
};


var values = [];
var index = -1, tick = 0;

var source_file = process.argv.length > 2 ? process.argv[2] : DEFAULT_SOURCE_FILE;
var source_id = process.argv.length > 3 ? process.argv[3] : DEFAULT_SOURCE_ID;

setInterval(function() {
	if (index < 0) {
		values = parseFile(source_file);
		index = 0;
		tick = 0;
		//console.log("values.length=" + values.length.toString());
	}
	
	var message = new Buffer(source_id + '\n' + values[index][0].toString());
	sock.send(message, 0, message.length, MULTICAST_PORT, MULTICAST_ADDRESS, function(err,sent){});
	
	if ((++tick * TICK_LENGTH) >= values[index][1]) {
		tick = 0;
		if (++index >= values.length) {
			index = -1;
		}
	}
}, TICK_LENGTH);

/*
setInterval(function()
{
	var fileContents = fs.readFileSync(SOURCE_FILE);
	var message = new Buffer(SOURCE_ID + '\n' + DATA_TRANSFORM(fileContents));
	sock.send(message, 0, message.length, MULTICAST_PORT, MULTICAST_ADDRESS, function(err,sent){});
},
READ_INTERVAL);
*/


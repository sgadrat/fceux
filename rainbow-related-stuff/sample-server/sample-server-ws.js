/*
  Prerequisites:

    1. Install node.js and npm
    2. npm install ws

  See also,

    http://einaros.github.com/ws/

  To run,

    node example-server.js
*/

"use strict"; // http://ejohn.org/blog/ecmascript-5-strict-mode-json-and-more/
var WebSocketServer = require("ws").Server;
var http = require("http");
require("./flashed-rom.js");

var server = http.createServer();
var wss = new WebSocketServer({ server: server, path: "" });
wss.on("connection", function(ws) {
  console.log("connected");
  ws.on("message", function(data, flags) {
    console.log("message");
    console.log(flags);
    //if (flags.binary) {
    //      return;
    //}
    if (data[0] == 2) console.log("CREATE ROOM");
    console.log(data);
    if (data == "goodbye") {
      console.log("<<< galaxy");
      ws.send("galaxy");
    }else if (data == "hello") {
      console.log("<<< world");
      ws.send("world");
	}else if (data == "upgradeprg") {
		for (var i = 0; i < flashed_prg_rom.length; ++i) {
			console.log("<<< prg_rom["+ i +"]");
			ws.send(flashed_prg_rom[i]);
		}
	}else if (data == "upgradechr") {
		for (var i = 0; i < flashed_chr_rom.length; ++i) {
			console.log("<<< chr_rom["+ i +"]");
			ws.send(flashed_chr_rom[i]);
		}
    }else {
      console.log("<<< what");
	  ws.send("i agree with you");
    }
  });
  ws.on("close", function() {
    console.log("Connection closed!");
  });
  ws.on("error", function(e) {});
});
server.listen(3000);
console.log("Listening on port 3000...");

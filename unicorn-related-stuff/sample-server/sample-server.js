"use strict";

process.title = 'sample-server';
var webSocketsServerPort = 3000;

// websocket and http servers
var webSocketServer = require('websocket').server;
var http = require('http');

/**
 * Global variables
 */

// list of currently connected clients (users)
var clients = [];

/**
 * HTTP server
 */
var server = http.createServer(function(request, response) {
});
server.listen(webSocketsServerPort, function() {
	console.log((new Date()) + " Server is listening on port " + webSocketsServerPort);
});

/**
 * WebSocket server
 */
var wsServer = new webSocketServer({
	httpServer: server
});

// This callback function is called every time someone
// tries to connect to the WebSocket server
wsServer.on('request', function(request) {
	console.log((new Date()) + ' Connection from origin ' + request.origin + '.');

	// Accept connection
	var connection = request.accept(null, request.origin); 

	// We need to know client index to remove them on 'close' event
	var index = clients.push(connection) - 1;
	console.log((new Date()) + ' Connection accepted.');

	// User sent some message
	connection.on('message', function(message) {
		console.log((new Date()) + ' got message, type = "' + message.type + '"');
		if (message.type === 'binary') {
			// Log received message
			console.log((new Date()) + ' Received Binary Message of ' + message.binaryData.length + ' bytes: ' + message.binaryData);

			// Send back a message to the client
			var buf = Buffer.alloc(19, "message from server");
			clients[index].sendBytes(buf);
		}
	});

	// User disconnected
	connection.on('close', function(connection) {
		console.log((new Date()) + " Peer " + connection.remoteAddress + " disconnected.");

		// Remove user from the list of connected clients
		clients[index] = null;
	});
});

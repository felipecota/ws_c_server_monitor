# WebSocket

WebSocket Server C++ to monitor CPU, RAM and Network usage for Ubuntu/Linux

## Compile 

command: g++ sMonitor.cpp -lcrypto -lpthread -o sMonitor

## Client

<script>
	ws = new WebSocket('ws://localhost:8082');
	ws.onopen = function () { ws.send('Ping'); };
	ws.onerror = function (error) {
		console.log(error);
	};
	ws.onmessage = function (e) {
		console.log(e.data);
	};
</script>

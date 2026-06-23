import WebSocket from 'ws';
import { SocketMessage, encodeMessage, parseMessage } from './lib/socket.js';

function start() {


  const ws = new WebSocket('ws://localhost:3010?board_id=hki-ltm&version=100&mode_id=lines');

  ws.on('error', console.error);

  ws.on('open', function open() {
    console.log("Requested ping")
    ws.send(encodeMessage({ type: "ping_req" }))
  });

  ws.on("close", () => {
    console.log("disconnected, trying to reconnect in 5 seconds")
    setTimeout(start, 5000)
  })

  ws.on('message', function message(data) {
    const result = parseMessage(data)
    if (Object.hasOwn(result, "error")) {
      console.log("Received unknown data:", data)
    } else if (Object.hasOwn(result, "message")) {
      const message = (result as { message: SocketMessage }).message
      switch (message.type) {
        case "error":
          console.error(`Received error: ${message.message}`)
          break
        case "uuid":
          console.error(`Connection was assigned UUID: ${message.uuid}`)
          break
        case "ping_req":
          ws.send(encodeMessage({ type: "ping_res" }))
          break
        case "ping_res":
          console.log("Received ping response")
          break
        case "events":
          console.log(`Received ${message.updates.length} updates`)
          message.updates.forEach(u => {
            if (u.t == "remove") {
              console.log(`${u.d.id} X`)
            } else {
              console.log(`${u.d.id} => ${u.d.idx}`)
            }
          })
          break
        default:
          console.log("[WS SERVER] Received data of unknown type: " + message.type)
          break
      }
    }
  });
}
start()
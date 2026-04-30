import { WebSocketServer } from "ws"
import { DigitrafficDataCollector } from "./lib/digitraffic.js"
import { MapEvent } from "./lib/mapEvent.js";

const digitraffic = new DigitrafficDataCollector(startAPI)

function startAPI() {
  const socket = new WebSocketServer({ port: 3010 })


  socket.on('connection', function connection(c) {
    c.on('error', console.error);

    c.on('message', function message(data) {
      console.log('received: %s', data);
    });

    c.send('something');
  });



}
export type SocketMessage = SocketInitialMessage

export interface SocketInitialMessage {
  type: "initial_data",
  updates: MapEvent[]
}
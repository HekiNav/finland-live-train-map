import { WebSocketServer } from "ws"
import { DigitrafficDataCollector } from "./lib/digitraffic.js"
import { MapEvent } from "./lib/mapEvent.js";
import { DataTranslator } from "./lib/translator.js";
import { dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
    
const __dirname = dirname(fileURLToPath(import.meta.url));

const digitraffic = new DigitrafficDataCollector(startAPI)
const translator = new DataTranslator(__dirname+"/data/")
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
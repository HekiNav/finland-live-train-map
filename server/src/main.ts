import { WebSocketServer } from "ws"

const socket = new WebSocketServer({port: 3010})


socket.on('connection', function connection(c) {
  c.on('error', console.error);

  c.on('message', function message(data) {
    console.log('received: %s', data);
  });

  c.send('something');
});

export type SocketMessage = SocketInitialMessage

export interface SocketInitialMessage {
    type: "initial_data",
    
}
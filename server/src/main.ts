import { WebSocketServer, WebSocket } from "ws"
import { DigitrafficDataCollector, Train, TrainNotRunning } from "./lib/digitraffic.js"
import { DataTranslator } from "./lib/translator.js";
import { dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { SocketMessage, encodeMessage, parseMessage } from "./lib/socket.js";

const __dirname = dirname(fileURLToPath(import.meta.url));



const [digitraffic, translator] = await Promise.all([
  new Promise<DigitrafficDataCollector>(res => {
    new DigitrafficDataCollector(res)
  }),
  new Promise<DataTranslator>(res => {
    new DataTranslator(__dirname + "/data/", res)
  })
])

const socket = new WebSocketServer({ port: 3010 })

socket.on("error", console.error)
socket.on("listening", () => console.log("[WS SERVER] Listening"))

socket.on('connection', function connection(c,r) {
  c.on('error', console.error);

  const cid = crypto.randomUUID()

  c.send(encodeMessage({
    type: "uuid",
    uuid: cid
  }))

  const url = new URL(`http://${process.env.HOST ?? 'localhost'}${r.url}`)
  const {board_id, version, mode_id} = Object.fromEntries(url.searchParams.entries())

  const board = translator.getBoardConfig(board_id)
  if (!board) {
    c.send(encodeMessage({
      type: "error",
      message: `Invalid board id (${board}). Valid values are ${translator.listBoards().join(", ")}`
    }))
    return c.close()
  }
  if (!board.config.available) {
    c.send(encodeMessage({
      type: "error",
      message: `Board unavailable`
    }))
    return c.close()
  }
  if (!board.config.versions.some(v => v == version)) {
    c.send(encodeMessage({
      type: "error",
      message: `Invalid board version (${version}). Valid values are ${board.config.versions.join(", ")}`
    }))
    return c.close()
  }
  if (!board.config.modes.some(m => m.id == mode_id && !(m.versions_not_available || []).some(v => v == version))) {
    c.send(encodeMessage({
      type: "error",
      message: `Invalid board mode id (${mode_id}). Valid values are ${
        board.config.modes
        .filter(m => (m.versions_not_available || []).every(v => v != version))
        .map(m => m.id)
        .join(", ")
      }`
    }))
    return c.close()
  }
  let current_mode = mode_id

  console.log(`[WS SERVER] New connection: ${board.config.id}/${version}/${mode_id}`)


  let updateQueue = new Array<Train | TrainNotRunning>()
  let timeout: null | NodeJS.Timeout = null
  digitraffic.onUpdate(cid, (update) => {
    // clump updates together
    updateQueue.push(update)
    if (!timeout) timeout = setTimeout(() => {
      timeout = null
      console.log(`sending ${updateQueue.length} events`)
    }, 5000)
  })
  c.on("close", () => digitraffic.offUpdate(cid))

  c.on('message', function message(data) {
    const result = parseMessage(data)
    if ((result as { error: unknown }).error) c.send(encodeMessage({
      type: "error",
      message: `Failed to parse message: ${(result as { error: unknown }).error}`
    }))
    else {
      const message = (result as { message: SocketMessage }).message
      switch (message.type) {
        case "ping_req":
          c.send(encodeMessage({type: "ping_res"}))
          break
        default:
          console.log("[WS SERVER] Received data of unknown type: " + message.type)
          break;
      }
    }
  });
});


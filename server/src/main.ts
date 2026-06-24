import { WebSocketServer, WebSocket } from "ws"
import { DigitrafficDataCollector, Train } from "./lib/digitraffic.js"
import { DataTranslator } from "./lib/translator.js";
import { dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { SocketMessage, encodeMessage, parseMessage } from "./lib/socket.js";
import { MapEvent } from "./lib/mapEvent.js";

const __dirname = dirname(fileURLToPath(import.meta.url));

export type RGBArray = [number, number, number]

// some colors are skipped due to them looking too similar to other colors in testing
export const colors = {
  0: [0,0,0] as RGBArray,          // Black
  1: [255, 0, 0] as RGBArray,     // Red
  2: [255, 128, 0] as RGBArray,   // Orange
  3: [255, 255, 0] as RGBArray,   // Yellow
  // SKIPPED 3: [128, 255, 0], // Yellow-green
  4: [0, 255, 0] as RGBArray,     // Green
  // SKIPPED 5: [0, 255, 128], // Turqoise
  5: [0, 255, 255] as RGBArray,   // Cyan
  // SKIPPED 7: [0, 128, 255], // Almost blue
  6: [0, 0, 255] as RGBArray,     // Blue
  7: [128, 0, 255] as RGBArray,   // Purple
  8: [255, 0, 255] as RGBArray,   // Magenta
  9: [255, 0, 128] as RGBArray,   // Pink
  10: [255, 255, 255] as RGBArray, // White
}

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

socket.on('connection', function connection(c, r) {
  c.on('error', console.error);

  const cid = crypto.randomUUID()

  c.send(encodeMessage({
    type: "uuid",
    uuid: cid
  }))

  const url = new URL(`http://${process.env.HOST ?? 'localhost'}${r.url}`)
  const { board_id, version, mode_id } = Object.fromEntries(url.searchParams.entries())

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
      message: `Invalid board mode id (${mode_id}). Valid values are ${board.config.modes
        .filter(m => (m.versions_not_available || []).every(v => v != version))
        .map(m => m.id)
        .join(", ")
        }`
    }))
    return c.close()
  }
  
  c.send(encodeMessage({
    type: "colors",
    colors: colors
  }))

  let current_mode = mode_id
  let updateQueue = new Array<Train>()
  let timeout: null | NodeJS.Timeout = null

  let eventsToSend = new Array<MapEvent>()
  let eventTimeout: null | NodeJS.Timeout = null

  // initial state
  sendEvents(Array.from(digitraffic.state.values()), true)

  console.log(`[WS SERVER] New connection: ${board.config.id}/${version}/${mode_id}`)



  digitraffic.onUpdate(cid, (update) => {
    // clump updates together
    updateQueue.push(update)
    if (!timeout) timeout = setTimeout(() => {
      const trains = updateQueue

      timeout = null
      updateQueue = []


      sendEvents(trains);
    }, 100)
  })

  function sendEvents(trains: Train[], skipBuffer = false) {
    if (!board) return
    const filters = board.config.modes.find(m => m.id == current_mode)?.filters!;
    const colors = board.config.modes.find(m => m.id == current_mode)?.colors!;
    const updates = translator.generateUpdates(trains, current_mode, filters, board.sections, colors);
    if (updates.length == 0) return
    if (skipBuffer) {
      console.log(`[WS SERVER] sending ${updates.length} events`);
      c.send(encodeMessage({
        type: "events",
        updates: updates
      }));
      return
    }
    send(updates)
    function send(events: MapEvent[]) {
      eventsToSend.push(...events)
      if (!eventTimeout) eventTimeout = setTimeout(() => {
        const ev = eventsToSend

        eventTimeout = null
        eventsToSend = []

        console.log(`[WS SERVER] sending ${events.length} events`);

        c.send(encodeMessage({
          type: "events",
          updates: ev
        }));
      }, 1000)

    }
  }


  c.on("close", () => {
    console.log("[WS SERVER] CLosed connection")
    digitraffic.offUpdate(cid)
  })

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
          c.send(encodeMessage({ type: "ping_res" }))
          break
        default:
          console.log("[WS SERVER] Received data of unknown type: " + message.type)
          break;
      }
    }
  });
});


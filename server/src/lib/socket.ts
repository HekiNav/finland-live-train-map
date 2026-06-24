import { RGBArray } from "../main.js"
import { MapEvent } from "./mapEvent.js"
import { WebSocket } from "ws"

export function parseMessage(msg: WebSocket.RawData): {message: SocketMessage} | {error: unknown} {
    try {
        const data = JSON.parse(msg.toString())
        return { message: data as SocketMessage}
    } catch (error) {
        return { error: error }
    }
}
export function encodeMessage(msg: SocketMessage): WebSocket.RawData {
    return Buffer.from(JSON.stringify(msg))
}


export type SocketMessage = 
SocketEventsMessage | SocketErrorMessage | SocketPingReqMessage | 
SocketPingResMessage | SocketUUIDMessage | {type: "null"}| SocketColorsMessage

export interface SocketErrorMessage {
    type: "error",
    message: string
}

export interface SocketColorsMessage {
    type: "colors",
    colors: {[k: number]: RGBArray}
}

export interface SocketEventsMessage {
    type: "events",
    updates: MapEvent[]
}

export interface SocketPingReqMessage {
    type: "ping_req",
}
export interface SocketPingResMessage {
    type: "ping_res",
}

export interface SocketUUIDMessage {
    type: "uuid",
    uuid: string
}
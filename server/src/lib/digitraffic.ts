import * as mqtt from "mqtt"

export class DigitrafficDataCollector {
    state = new Map<number, Train>()
    #client: mqtt.MqttClient
    #listeners = new Map<string, (t: Train) => void>()
    constructor(onready: (c: DigitrafficDataCollector) => void) {
        console.log("[DIGITRAFFIC] Getting initial data")
        this.#getInitialData().then(() => {
            onready(this)
        })
        console.log("[DIGITRAFFIC] Connecting to MQTT")
        this.#client = mqtt.connect("wss://rata.digitraffic.fi/mqtt")
        this.#client.on("connect", () => {
            this.#client.subscribe("trains/#", (err) => {
                if (err) {
                    console.error("[DIGITRAFFIC] MQTT subcription error %s", err)
                } else {
                    console.log("[DIGITRAFFIC] Connected to MQTT")
                }
            });
        });
        this.#client.on("error", console.error)
        this.#client.on("message", (topic, payload) => {
            //console.log("[DIGITRAFFIC]"+topic)
            const data = parseTrain(JSON.parse(payload.toString()))
            if (!data) return
            this.#updateTrain(data)
        })
    }
    async #getInitialData() {
        const data = await (await fetch("https://rata.digitraffic.fi/api/v1/live-trains/")).json() as TrainData[]
        let i = 0
        data.forEach(train => {
            const data = parseTrain(train)
            if (!data) return
            if (data.running) i++ && this.state.set(data.id, data)
        });
    }
    #updateTrain(t: Train) {
        this.#listeners.forEach(l => l(t))
        if (t.running) {
            this.state.delete(t.id)
        } else {
            this.state.set(t.id, t)
        }
    }
    onUpdate(id: string, fn: (t: Train) => void) {
        this.#listeners.set(id, fn)
    }
    offUpdate(id: string) {
        this.#listeners.delete(id)
    }
}
export function parseTrain(data: TrainData): Train | null {
    const lastIndex = data.timeTableRows.length - 1 - [...data.timeTableRows].reverse().findIndex(r => Object.hasOwn(r, "actualTime"))
    if (lastIndex < 0 || lastIndex >= data.timeTableRows.length) return null
    const running = lastIndex > 0 && lastIndex < data.timeTableRows.length - 1
    const last = data.timeTableRows[lastIndex]

    const props: TrainProperties = {
        commuter_line_id: data.commuterLineID || null,
        start_point: data.timeTableRows[0].stationShortCode,
        end_point: data.timeTableRows[data.timeTableRows.length - 1].stationShortCode,
        stations: data.timeTableRows.filter((r, i, a) => r.type == "DEPARTURE" || i == a.length - 1 || i == 0).map(r => ({ station: r.stationShortCode, time: r.actualTime ? new Date(r.actualTime) : new Date(new Date(r.scheduledTime).getTime() + (r.differenceInMinutes || 0) * 60_000) }))
    }


    if (last.type == "ARRIVAL") return {
        id: data.trainNumber,
        type: data.trainType,
        running,
        state: {
            type: "at_station",
            current: {
                station: last.stationShortCode,
                time: last.actualTime ? new Date(last.actualTime) : new Date(new Date(last.scheduledTime).getTime() + (last.differenceInMinutes || 0) * 60_000)
            }
        },
        properties: props
    }
    const next = data.timeTableRows[lastIndex + 1] || last
    return {
        id: data.trainNumber,
        type: data.trainType,
        running,
        state: {
            type: "between",
            last: {
                station: last.stationShortCode,
                time: last.actualTime ? new Date(last.actualTime) : new Date(new Date(last.scheduledTime).getTime() + (last.differenceInMinutes || 0) * 60_000)
            },
            next: {
                station: next.stationShortCode,
                time: next.actualTime ? new Date(next.actualTime) : new Date(new Date(next.scheduledTime).getTime() + (next.differenceInMinutes || 0) * 60_000)
            }
        },
        properties: props
    }
}
export interface Train {
    id: number
    type: TrainType
    state: AnyTrainState
    properties: TrainProperties,
    running: boolean
}
export interface TrainProperties {
    start_point: string,
    end_point: string,
    commuter_line_id: string | null,
    stations: TrainPos[]
}
export type AnyTrainState = TrainAtStationState | TrainBetweenStationsState
export interface TrainAtStationState {
    type: "at_station"
    current: TrainPos
}
export interface TrainBetweenStationsState {
    type: "between"
    last: TrainPos
    next: TrainPos
}
export interface TrainPos {
    station: string
    time: Date
}
export interface TrainData {
    trainNumber: number
    departureDate: string
    operatorUICCode: number
    operatorShortCode: string
    trainType: TrainType
    trainCategory: Traincategory
    commuterLineID?: string
    runningCurrently: boolean
    cancelled: boolean
    version: number
    timetableType: "REGULAR" | "ADHOC"
    timetableAcceptanceDate: string
    deleted?: boolean
    timeTableRows: TimetableRow[]
}
export type TrainType =
    "PAR" | "HL" | "VET" | "VEV" | "H" | "PVS" | "HV" | "P" | "HDM" | "PVV" | "VLI" | "S" | "HLV" | "T" | "V" | "W" | "IC2" | "IC" | "HSM" | "AE" | "PYO" | "MV" | "MUS" | "TYO" | "MUV" | "SAA" | "LIV" | "RJ" | "PAI"
export type Traincategory = "Long-distance" | "Commuter" | "Cargo" | "Locomotive" | "Test drive" | "On-track machines" | "Shunting"
export interface TimetableRow {
    trainStopping: boolean
    stationShortCode: string
    stationUICCode: number
    countryCode: "FI" | "RU"
    type: "ARRIVAL" | "DEPARTURE"
    commercialStop?: boolean
    commercialTrack?: string
    cancelled: boolean
    scheduledTime: string
    liveEstimateTime?: string
    estimateSource?: string
    unknownDelay?: boolean
    actualTime?: string
    differenceInMinutes?: number
    causes: DelayCause[]
    stopSector?: string
    trainReady?: ReadyState

}
export interface DelayCause {
    categoryCodeId: string
    categoryCode: string
    detailedCategoryCodeId?: string
    detailedCategoryCode?: string
    thirdCategoryCodeId?: string
    thirdCategoryCode?: string
}
export interface ReadyState {
    source: string
    accepted: string
    timestamp: string
}
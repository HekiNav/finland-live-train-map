import * as mqtt from "mqtt"

export class DigitrafficDataCollector {
    state = new Map<number, Train>()
    client: mqtt.MqttClient
    constructor(onready: () => void) {
        console.log("Getting initial data")
        this.#getInitialData().then(() => {
            onready()
        })
        console.log("Connecting to MQTT")
        this.client = mqtt.connect("wss://rata.digitraffic.fi/mqtt")
        this.client.on("connect", () => {
            this.client.subscribe("trains/#", (err) => {
                if (err) {
                    console.error("MQTT subcription error %s", err)
                } else {
                    console.log("Connected to MQTT")
                }
            });
        });
        this.client.on("error", console.error)
        this.client.on("message", (topic, payload) => {
            console.log(topic)
            const data = parseTrain(JSON.parse(payload.toString()))
            if (data.type == "NOT_RUNNING") {
                this.state.delete(data.id)
            } else {
                this.state.set(data.id, data)
            }
            console.log(data)
        })
    }
    async #getInitialData() {

    }
}
// return string if needs removal
export function parseTrain(data: TrainData): Train | { id: number, type: "NOT_RUNNING" } {
    const lastIndex = data.timeTableRows.reverse().findIndex(r => Object.hasOwn(r, "actualTime"))
    if (lastIndex < 0 || lastIndex == data.timeTableRows.length - 1) return { id: data.trainNumber, type: "NOT_RUNNING" }
    const last = data.timeTableRows[lastIndex]
    if (last.type == "ARRIVAL") return {
        id: data.trainNumber,
        type: data.trainType,
        state: {
            type: "at_station",
            current: {
                station: last.stationShortCode,
                time: new Date(last.actualTime || last.scheduledTime)
            }
        }
    }
    const next = data.timeTableRows[lastIndex + 1]
    console.log(last, next)
    return {
        id: data.trainNumber,
        type: data.trainType,
        state: {
            type: "between",
            last: {
                station: last.stationShortCode,
                time: new Date(last.actualTime || last.scheduledTime)
            },
            next: {
                station: next.stationShortCode,
                time: new Date(next.actualTime || next.scheduledTime)
            }
        }
    }
}
export interface Train {
    id: number
    type: TrainType
    state: AnyTrainState
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
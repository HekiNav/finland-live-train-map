function generateInitialState(): MapEvent[] {
    return []
}

export type MapMode = "trains" | "delay"
export type MapEvent = MapEventAdd | MapEventRemove | MapEventUpdate
export interface MapEventBase<T extends string> {
    ty: T
}
export interface MapEventAdd extends MapEventBase<"add"> {
    tr: MapEventTrain
}
export interface MapEventRemove extends MapEventBase<"remove"> {
    tr: Omit<MapEventTrain, "mi" | "ti" | "ct">
}
export interface MapEventUpdate extends MapEventBase<"update"> {
    tr: MapEventTrain
}
export interface MapEventTrain {
    mi: number, // main index 
    ti: number, // tail index
    id: number, // train id
    ts: number, // timestamp
    ct: number, // color type
}

// temporary
export const mapModeColors = {
    trains: {   
        1: "A",
        2: "B"
    },
    delay: {
        1: "A",
        2: "B"
    }
}
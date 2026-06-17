export type MapEvent = MapEventRemove | MapEventUpdate
export interface MapEventBase<T extends string> {
    t: T
}
export interface MapEventRemove extends MapEventBase<"remove"> {
    d: Omit<MapEventTrain, "idx" | "clr">
}
export interface MapEventUpdate extends MapEventBase<"update"> {
    d: MapEventTrain
}
export interface MapEventTrain {
    idx: number, // main index 
    //ti: number, // tail index (maybe later)
    id: number, // train id
    clr: number, // color type
}
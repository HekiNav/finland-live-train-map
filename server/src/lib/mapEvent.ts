export interface MapEventBase<T extends string> {
    type: T 
    led: number
}
export interface MapEventNew extends MapEventBase<"add"> {

}
export interface MapEventClear extends MapEventBase<"remove"> {
    
}
import { AnyTrainState, Train, TrainBetweenStationsState } from "./digitraffic.js"
import { importJSONC } from "./jsonc.js"
import { MapEvent } from "./mapEvent.js"

export interface BoardConfig {
    id: string,
    name: string,
    desc?: string,
    versions: string[],
    available: boolean,
    modes: BoardMode[]
}
export type ColorTable = string[][]
export interface BoardMode {
    id: string,
    name: string,
    colors: ColorTable,
    filters: AnyBoardModeTrainFilter[],
    versions_not_available?: string[]
}
export interface BoardModeTrainFilter<T extends string> {
    type: T,
    allow: boolean,
    list: string[]
}
export type BoardModeTrainTypeFilter = BoardModeTrainFilter<"train_type">

export type AnyBoardModeTrainFilter = BoardModeTrainTypeFilter

export type BoardsConfig = BoardConfig[]

type BoardBlock = {
    index: number
    filters?: AnySectionFilter[]
}

export interface BoardSection<T extends string, P extends { [K: string]: string | number }> {
    type: T,
    properties: P,
    blocks: BoardBlock[]
}
export interface SectionFilter<T extends string> {
    type: T
}
export interface SectionFilterTrainVia extends SectionFilter<"train_via"> {
    station_code: string
}

export interface SectionFilterCommuterLine extends SectionFilter<"commuter_line"> {
    lines: string[]
}
export type AnySectionFilter = SectionFilterTrainVia | SectionFilterCommuterLine

export type BoardsSectionBetween = BoardSection<"between", { station_1_code: string, station_2_code: string }>
export type BoardsSectionStation = BoardSection<"station", { station_code: string }>

export type AnyBoardSection = BoardsSectionBetween | BoardsSectionStation

export interface MultiBetweenHandler {
    interval: NodeJS.Timeout,
    state: TrainBetweenStationsState
}

export class DataTranslator {
    #boards_config: BoardsConfig | null = null
    #board_configs: Map<string, { config: BoardConfig, sections: AnyBoardSection[] }> = new Map()
    constructor(config_path = "/data/", callback = (t: DataTranslator) => { }) {
        console.log("[TRANSLATOR] Loading boards.jsonc")
        importJSONC<BoardsConfig>(config_path + "boards.jsonc").then((data) => {
            this.#boards_config = data
            if (!this.#boards_config) throw "Failed to load boards.jsonc"
            Promise.all(this.#boards_config.map(async board => {
                await Promise.all(board.versions.map(async ver => {
                    console.log(`[TRANSLATOR] Loading ${board.id}/${ver}.jsonc`)
                    const sections = await importJSONC(`${config_path}${board.id}/${ver}.jsonc`)
                    this.#board_configs.set(board.id, {
                        config: board,
                        sections: sections
                    })
                }))
            })).then(() => {
                console.log("[TRANSLATOR] Loaded config files")
                callback(this)
            })
        })
    }
    getBoardConfig(board_id: string) {
        return this.#board_configs.get(board_id) || null
    }
    listBoards() {
        return Array.from(this.#board_configs.keys())
    }
    generateUpdates(trains: Train[], mode: string, filters: AnyBoardModeTrainFilter[], board_sections: AnyBoardSection[], colorTable: ColorTable): MapEvent[] {
        let events: MapEvent[] = []
        const filteredTrains = trains
            .filter(t => filters.every(f => this.#checkBoardModeTrainFilter(t, f)))
            .reduce<Train[]>((a, t) => {
                if (t.running) return [...a, t]
                events.push({
                    t: "remove",
                    d: {
                        id: t.id
                    }
                })
                return a
            }, [])
        filteredTrains.forEach(t => {
            let color = 0
            switch (mode) {
                case "route":
                    color = 1
                    break
                case "lines":
                    color = colorTable.findIndex(c => c.some(l => l == t.properties.commuter_line_id)) + 1 || 10
                    break
                default:
                    console.error(`Unknown mode (${mode}) - Cannot process`)
            }

            const section = this.#findSection(t, board_sections, (s) => ({
                t: "update",
                d: {
                    idx: s,
                    id: t.id,
                    clr: color
                }
            }))

            if (!section) return null

            events.push({
                t: "update",
                d: {
                    idx: section,
                    id: t.id,
                    clr: color
                }
            })
        })
        return events
    }
    #checkBoardModeTrainFilter(t: Train, filter: AnyBoardModeTrainFilter) {
        switch (filter.type) {
            case "train_type":
                const in_train_type_list = filter.list.some(e => e == t.type)
                return filter.allow ? in_train_type_list : !in_train_type_list
            default:
                return false
        }
    }
    #findSection(t: Train, sections: AnyBoardSection[], send: (n: number) => void): number | null {
        const state = this.#convertState(t, sections)
        if (!state) {
            return null
        }
        let section: AnyBoardSection
        let blocks: BoardBlock[] = []
        if (state.type == "at_station") {
            section = sections.find(s => s.type == "station" && s.properties.station_code == state.current.station)!
            blocks = section.blocks
        } else {
            section = sections.find(s => s.type == "between" && (
                (s.properties.station_1_code == state.next.station && s.properties.station_2_code == state.last.station) ||
                (s.properties.station_1_code == state.last.station && s.properties.station_2_code == state.next.station)
            ))!
            blocks = section.blocks
        }
        if (blocks.length == 1) return blocks[0].index
        blocks = blocks.filter(b => !b.filters || b.filters.length == 0 || b.filters.every(f => this.#checkBlockFilter(t, f)))
        if (blocks.length == 1) return blocks[0].index
        if (blocks.length == 0) return null

        if (state.type == "at_station" || section.type == "station") return null
        // multi between handling

        let i = 0
        const totalTime = state.next.time.getTime() - state.last.time.getTime()
        const sectionReversed = state.next.station == section.properties.station_1_code
        if (sectionReversed) blocks.reverse()

        const handler: MultiBetweenHandler = {
            interval: setInterval(handleMultiBetween, totalTime / blocks.length),
            state: state
        }
        function handleMultiBetween() {
            i++
            if (!blocks[i]) return clearInterval(handler.interval)
            send(blocks[i].index)
            if (i >= blocks.length) return clearInterval(handler.interval)
        }
        return blocks[i].index
    }
    #checkBlockFilter(t: Train, filter: AnySectionFilter) {
        switch (filter.type) {
            case "train_via":
                return t.properties.stations.some(s => s.station == filter.station_code)
            case "commuter_line":
                return filter.lines.some(l => l == t.properties.commuter_line_id) || filter.lines.some(l => l == "-") && (!t.properties.commuter_line_id || t.properties.commuter_line_id == "V")
            default:
                console.error(`Unknown block filter type ${(filter as AnySectionFilter).type}`)
                return false
        }
    }
    #convertState(t: Train, sections: AnyBoardSection[]): AnyTrainState | null {
        const state = t.state
        if (state.type == "at_station") {
            if (sections.find(s => s.type == "station" && s.properties.station_code == state.current.station)) {
                return state
            } else {
                const split_index = t.properties.stations.findIndex(s => s.station == state.current.station)
                const previous_stations = t.properties.stations.slice(0, split_index).reverse()
                const next_stations = t.properties.stations.slice(split_index + 1, t.properties.stations.length)
                for (let i = 0; i < previous_stations.length; i++) {
                    const p = previous_stations[i];
                    for (let j = 0; j < next_stations.length; j++) {
                        const n = next_stations[j];
                        if (sections.find(s => s.type == "between" && (
                            (s.properties.station_1_code == n.station && s.properties.station_2_code == p.station) ||
                            (s.properties.station_1_code == p.station && s.properties.station_2_code == n.station)
                        ))) {
                            return {
                                type: "between",
                                last: p,
                                next: n
                            }
                        }
                    }
                }
                return null
            }
        } else {
            if (sections.find(s => s.type == "between" &&
                (
                    (s.properties.station_1_code == state.next.station && s.properties.station_2_code == state.last.station) ||
                    (s.properties.station_1_code == state.last.station && s.properties.station_2_code == state.next.station)
                )
            )) {
                return state
            } else {
                const split_index = t.properties.stations.findIndex(s => s.station == state.last.station)
                const previous_stations = t.properties.stations.slice(0, split_index).reverse()
                const next_stations = t.properties.stations.slice(split_index + 1, t.properties.stations.length)
                for (let i = 0; i < previous_stations.length; i++) {
                    const p = previous_stations[i];
                    for (let j = 0; j < next_stations.length; j++) {
                        const n = next_stations[j];
                        if (sections.find(s => s.type == "between" && (
                            (s.properties.station_1_code == n.station && s.properties.station_2_code == p.station) ||
                            (s.properties.station_1_code == p.station && s.properties.station_2_code == n.station)
                        ))) {
                            return {
                                type: "between",
                                last: p,
                                next: n
                            }
                        }
                    }
                }
                return null
            }
        }
    }
}
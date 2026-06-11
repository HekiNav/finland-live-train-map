import { importJSONC } from "./jsonc.js"

export interface BoardConfig {
    id: string,
    name: string,
    desc?: string,
    versions: string[],
    available: boolean,
    modes: BoardMode[]
}
export interface BoardMode {
    id: string,
    name: string,
    versions_not_available?: string[]
}
export type BoardsConfig = BoardConfig[]

export interface BoardSection<T extends string, P extends { [K: string]: string | number }> {
    type: T,
    properties: P,
    blocks: {
        index: number,
        filters: AnySectionFilter[]
    }[]
}
export interface SectionFilter<T extends string> {
    type: T
}
export interface SectionFilterTrainVia extends SectionFilter<"train_via"> {
    station_code: string
}
export type AnySectionFilter = SectionFilterTrainVia

export type BoardsSectionBetween = BoardSection<"between", { station_1_code: string, station_2_code: string }>
export type BoardsSectionStation = BoardSection<"station", { station_code: string }>

export type AnyBoardSection = BoardsSectionBetween | BoardsSectionStation

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
    generateUpdates() {
        
    }
}
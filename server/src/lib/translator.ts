import { importJSONC } from "./jsonc.js"

export class DataTranslator {
    #board_config
    constructor(config_path="/data/") {
        this.#board_config = importJSONC(config_path+"boards.jsonc")
    }
}
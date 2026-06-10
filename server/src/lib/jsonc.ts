import JSONC from "jsonc-simple-parser"
import * as fs from "node:fs/promises"
export async function importJSONC(path: string) {
    const file = await fs.readFile(path)
    JSONC.parse(file.toString())
}
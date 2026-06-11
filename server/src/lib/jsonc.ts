import JSONC from "jsonc-simple-parser"
import * as fs from "node:fs/promises"
export async function importJSONC<T = any>(path: string): Promise<T | null> {
    try {
        const file = await fs.readFile(path)
        return JSONC.parse(file.toString()) as T
    } catch (error) {
        console.error(error)
        return null
    }

}
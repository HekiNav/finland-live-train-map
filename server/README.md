[Back to main page](./)
# Server
Processes train data into a format that is easier to consume for the map.

[[Fintraffic API](https://www.digitraffic.fi/rautatieliikenne/)] -> [Server] -> [ESP32 on Map] -> [NeoPixel LEDs]

## Board configs
The server uses board config JSON files to translate the train state to packets that get sent to the led maps. This way the server can serve multiple configs at the same time.

### `src/data/boards.jsonc`
Defines boards to support

### `src/data/[board_id]/[board_version].jsonc`
- Versions are currently in a number format ("1.0.0" => 100)
- The format is an array of station/between objects


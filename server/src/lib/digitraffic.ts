import * as mqtt from "mqtt"

export class DigitrafficDataCollector {
    state = new Map()
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
        })
    }
    async #getInitialData() {

    }
}
#include <Arduino.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <WebSocketsClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <time.h>
#include <vector>
#include <map>

#include "WiFiConfig.h"

#include "brightness.h"

#include "buttons.h"
#include "ledManager.h"

Preferences preferences;
BrightnessManager brightness;
ButtonManager buttons;

WiFiClient client;

const char *ntpServers[] = {"pool.ntp.org"};
const char *time_zone = "EET-2EEST,M3.5.0/3,M10.5.0/4";

WebSocketsClient ws;
bool wsConnecting = false;

CRGB black = CRGB::Black;
std::vector<CRGB> colorTable;

#if defined(HKI_LTM)
String mapModes[] =
	{
		"lines"};
#elif defined(FIN_LTM)
String mapModes[] =
	{
		"routes"};
#else
String mapModes[] =
	{
		"null"};
#endif
int16_t currentMapMode = 0;

String serverURL = String("/?board_id=") + CITY_CODE + "-ltm&version=" + BACKEND_VERSION + "&mode_id=" + mapModes[currentMapMode];
String serverHost = "ltm-api-v2.hekinav.dev";


bool ledUpdateScheduled = false;

enum statusLedCommand
{
	LED_OFF = 0,
	LED_ON_GREEN = 1,
	LED_ON_RED = 2,
	LED_BLINK_GREEN_SLOW = 3, // 1Hz
	LED_BLINK_GREEN_FAST = 4, // 5Hz
	LED_BLINK_RED_SLOW = 5,	  // 1Hz
	LED_BLINK_RED_FAST = 6	  // 5Hz
};

typedef struct
{
	uint8_t pin;
	statusLedCommand command;
	bool currentState;
	unsigned long lastToggle;
} statusLed;

TaskHandle_t statusLedTaskHandle;

std::map<int, std::array<int,2>> trains;

uint8_t serverConnectionTries = 0;
unsigned long lastDrawTime = 0;

const char *getLocalTime(time_t epoch)
{
	struct tm timeinfo;
	static char buffer[64];

	// Convert epoch to local time
	if (!localtime_r(&epoch, &timeinfo))
	{
		return "No time available";
	}
	if (strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo))
	{
		return buffer;
	}
	return "Format error";
}

void timeavailable(struct timeval *t)
{
	Serial.println("NTP Synced");
}

void setCharlieplexedLED(uint8_t pin, statusLedCommand state)
{
	switch (state)
	{
	case LED_ON_GREEN:
		pinMode(pin, OUTPUT);
		digitalWrite(pin, HIGH);
		break;

	case LED_ON_RED:
		pinMode(pin, OUTPUT);
		digitalWrite(pin, LOW);
		break;

	case LED_OFF:
		// Set as input (High Resistance) to disable output driver
		pinMode(pin, INPUT);
		break;
	}
}

void statusLedManagerTask(void *pvParameters)
{
	statusLed leds[] = {{WIFI_LED_PIN, LED_OFF, false, 0}, {SERVER_LED_PIN, LED_OFF, false, 0}};
	const int numLeds = sizeof(leds) / sizeof(leds[0]);

	while (1)
	{
		// Check for notifications
		uint32_t notification;
		if (xTaskNotifyWait(0, ULONG_MAX, &notification, 0) == pdTRUE)
		{
			// Process up to two commands
			for (int cmdIdx = 0; cmdIdx < 2; cmdIdx++)
			{
				uint8_t pin = (notification >> (24 - (cmdIdx * 16))) & 0xFF;
				statusLedCommand cmd = statusLedCommand((notification >> (16 - (cmdIdx * 16))) & 0xFF);

				// Skip invalid pins (0 means no command)
				if (pin == 0)
					continue;

				for (int i = 0; i < numLeds; i++)
				{
					if (leds[i].pin == pin)
					{
						leds[i].command = cmd;
						if (cmd == LED_ON_GREEN || cmd == LED_ON_RED || cmd == LED_OFF)
						{
							setCharlieplexedLED(pin, cmd);
						}
						break;
					}
				}
			}
		}

		// Handle blinking
		unsigned long now = millis();
		for (int i = 0; i < numLeds; i++)
		{
			if (leds[i].command >= LED_BLINK_GREEN_SLOW)
			{
				const bool isGreen = (leds[i].command == LED_BLINK_GREEN_SLOW || leds[i].command == LED_BLINK_GREEN_FAST);
				const bool isRed = (leds[i].command == LED_BLINK_RED_SLOW || leds[i].command == LED_BLINK_RED_FAST);
				const bool isSlow = (leds[i].command == LED_BLINK_GREEN_SLOW || leds[i].command == LED_BLINK_RED_SLOW);

				if (isGreen || isRed)
				{
					const int interval = isSlow ? 500 : 100;
					const statusLedCommand color = isGreen ? LED_ON_GREEN : LED_ON_RED;

					if (now - leds[i].lastToggle >= interval)
					{
						leds[i].currentState = !leds[i].currentState;
						setCharlieplexedLED(leds[i].pin, leds[i].currentState ? color : LED_OFF);
						leds[i].lastToggle = now;
					}
				}
			}
		}

		vTaskDelay(pdMS_TO_TICKS(25));
	}
}

void setStatusLedState(uint8_t pin1, statusLedCommand cmd1, uint8_t pin2, statusLedCommand cmd2)
{
	uint32_t notification = (pin1 << 24) | (cmd1 << 16) | (pin2 << 8) | cmd2;
	xTaskNotify(statusLedTaskHandle, notification, eSetValueWithOverwrite);
}

void setStatusLedState(uint8_t pin, statusLedCommand command)
{
	setStatusLedState(pin, command, 0, LED_OFF);
}

String getSystemInfo()
{
	FlashMode_t mode = (FlashMode_t)ESP.getFlashChipMode();
	String flashMode;

	// Convert flash mode to human-readable string
	switch (mode)
	{
	case FM_QIO:
		flashMode = "Quad I/O (QIO)";
		break;
	case FM_QOUT:
		flashMode = "Quad Output (QOUT)";
		break;
	case FM_DIO:
		flashMode = "Dual I/O (DIO)";
		break;
	case FM_DOUT:
		flashMode = "Dual Output (DOUT)";
		break;
	case FM_FAST_READ:
		flashMode = "Fast Read";
		break;
	case FM_SLOW_READ:
		flashMode = "Slow Read";
		break;
	default:
		flashMode = "Unknown";
		break;
	}

	String info = "\n";
	info += String(ARDUINO_BOARD) + "\n";
	info += String(CITY_CODE) + "-ltm V" + BACKEND_VERSION + "\n";
	info += "Built: " + String(__DATE__) + " " + __TIME__ + "\n";
	info += String(ESP.getChipModel()) + "-Rev" + ESP.getChipRevision() + "\n";
	info += String(ESP.getChipCores()) + " Core @ " + ESP.getCpuFreqMHz() + "MHz\n";
	info += String(ESP.getFlashChipSize() / (1024 * 1024)) + "MiB Flash @ " + (ESP.getFlashChipSpeed() / (1000 * 1000)) + "MHz in " + flashMode + " Mode\n";
	info += "RAM Heap: " + String(ESP.getHeapSize() / 1024) + "kiB\n";
	info += "IDF SDK: " + String(ESP.getSdkVersion()) + "\n";

	return info;
}

void setBlockColorId(uint16_t block, int colorId)
{
	CRGB color = (colorId >= 0 && colorId < static_cast<int>(colorTable.size())) ? colorTable[colorId] : black;

	setBlockColorRGB(block, color);
}

void drawMap()
{
	suspendDithering();
	clearLEDs();

	for (auto const& t : trains)
	{
		setBlockColorId(t.second[0], t.second[1]);
	}

	ledUpdateScheduled = false;

	resumeDithering();
}

void parseEvent(uint8_t *payload, size_t length)
{
	JsonDocument doc;
	DeserializationError error = deserializeJson(doc, payload, length);

	if (error)
	{
		Serial.printf("JSON parse error: %s\n", error.c_str());
		return;
	}

	const String type = doc["type"];

	if (type == "error")
	{
		Serial.printf("Error from server: %s \n", doc["message"].as<const char *>());
	}
	else if (type == "uuid")
	{
		Serial.printf("UUID assigned by server: %s \n", doc["uuid"].as<const char *>());
	}
	else if (type == "events")
	{
		const JsonArray updates = doc["updates"];
		Serial.printf("Received %d updates \n", updates.size());
		for (JsonObject update : updates)
		{
			if (update["t"] == "remove")
			{
				trains.erase(update["d"]["id"].as<int>());
			}
			else
			{
				int block = update["d"]["idx"].as<int>();
				int colorId = update["d"]["clr"].as<int>();

				trains[update["d"]["id"]] = {block, colorId};
			}
		}
		ledUpdateScheduled = true;
	}
	else if (type == "colors")
	{
		Serial.printf("Received colortable \n");
		const JsonObject colors = doc["colors"];
		for (JsonPair kv : colors)
		{
			JsonArray rgb = kv.value().as<JsonArray>();
			colorTable.push_back(CRGB(rgb[0] | 0, rgb[1] | 0, rgb[2] | 0));
		}
	}
	else
	{
		Serial.printf("Unknown event type: %s \n", type);
	}
}

void onBrightnessDown()
{
	brightness.decrease();
}

void onBrightnessUp()
{
	brightness.increase();
}

void onPower()
{
	brightness.toggle();
	if (brightness.isOn())
	{
		setStatusLedState(WIFI_LED_PIN, LED_ON_GREEN, SERVER_LED_PIN, LED_ON_GREEN);
		vTaskDelay(pdMS_TO_TICKS(50));
		ledUpdateScheduled = true;
	}
	else
	{
		setStatusLedState(WIFI_LED_PIN, LED_OFF, SERVER_LED_PIN, LED_OFF);
	}
}

void onMode()
{
	// Cycle through modes
	currentMapMode = (currentMapMode + 1) % mapModes->length();
	brightness.setPower(true); // Ensure brightness is on when changing modes
	Serial.println("Mode button pressed");
}

void onEvent(WStype_t type, uint8_t *payload, size_t length)
{
	switch (type)
	{
	case WStype_DISCONNECTED:
		wsConnecting = false;
		Serial.println("DISCONNECTED");
		break;
	case WStype_CONNECTED:
		Serial.println("CONNECTED");
		break;
	case WStype_ERROR:
	case WStype_BIN:
	case WStype_TEXT:
		parseEvent(payload, length);
		Serial.printf(
			"MCU:%2.0f°C WiFi:%idBm\n", temperatureRead(), WiFi.RSSI());
		Serial.flush();
		break;
	default:
		Serial.println("unknown type:" + String(type));
		break;
	}
}

void setup()
{
	// Hardware Serial
	// Serial0.begin(115200);

	// USB Serial
	Serial.begin();
	Serial.setDebugOutput(true);
	xTaskCreate(improvSerialTask, "Improv Serial Task", 4096, nullptr, 3, nullptr);

	setupLeds();

	// --- Setup Buttons ---
	buttons.add(BRIGHTNESS_DOWN_BUTTON, onBrightnessDown);
	buttons.add(BRIGHTNESS_UP_BUTTON, onBrightnessUp);
	buttons.add(POWER_BUTTON, onPower);
	buttons.add(MODE_BUTTON, onMode);
	buttons.begin();

	Serial.println(getSystemInfo());

	// --- Time Setup ---
	sntp_set_time_sync_notification_cb(timeavailable);
	sntp_set_sync_interval(1000 * 60 * 15); // Set sync interval to 15 minutes
	sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
	configTzTime(time_zone, ntpServers[0]);

	ws.onEvent(onEvent);

	// --- WiFi Setup ---
	xTaskCreate(statusLedManagerTask, "Status LED Manager", 1024, NULL, 2, &statusLedTaskHandle);

	if (WiFiImprovSetup())
	{
		Serial.println("WiFi credentials found...");
		setStatusLedState(WIFI_LED_PIN, LED_BLINK_GREEN_FAST);
	}
	else
	{
		Serial.println("No WiFi credentials found...");
		setStatusLedState(WIFI_LED_PIN, LED_ON_RED);
	}

	brightness.begin();
}

void loop()
{
	bool wiFiConnected = (WiFi.status() == WL_CONNECTED);
	if (!wiFiConnected)
	{
		manageWiFiConnection();
		if (brightness.isOn())
		{
			if (millis() > 60 * 1000)
			{
				setStatusLedState(WIFI_LED_PIN, LED_ON_RED, SERVER_LED_PIN, LED_OFF);
			}
		}
	}
	else if (!ws.isConnected() && !wsConnecting && serverConnectionTries < 3)
	{
		wsConnecting = true;
		if (brightness.isOn())
		{
			setStatusLedState(WIFI_LED_PIN, LED_ON_GREEN, SERVER_LED_PIN, LED_BLINK_GREEN_FAST);
		}
		serverConnectionTries++;
		Serial.printf("Trying to connect, attempt %i \n", serverConnectionTries);
		ws.beginSSL(serverHost, 443, serverURL);
	}
	else if (!ws.isConnected())
	{
		if (brightness.isOn())
		{
			setStatusLedState(WIFI_LED_PIN, LED_ON_GREEN, SERVER_LED_PIN, LED_BLINK_GREEN_FAST);
		}
		if (brightness.isOn() && serverConnectionTries >= 3)
		{
			setStatusLedState(WIFI_LED_PIN, LED_ON_GREEN, SERVER_LED_PIN, LED_ON_RED);
		}
	}
	else
	{
		if (brightness.isOn())
		{
			setStatusLedState(WIFI_LED_PIN, LED_ON_GREEN, SERVER_LED_PIN, LED_ON_GREEN);
		}
	}
	if (ledUpdateScheduled && (millis() - lastDrawTime > DEBOUNCE_MS))
	{
		Serial.println("Drawing map");
		drawMap();
		lastDrawTime = millis();
	}

	brightness.update();
	ws.loop();
	vTaskDelay(pdMS_TO_TICKS(10));
}
#include <Arduino.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <time.h>
#include <vector>

#include "WiFiConfig.h"

#include "brightness.h"

#include "buttons.h"
#include "ledManager.h"

Preferences preferences;
BrightnessManager brightness;
ButtonManager buttons;

// Array of server URLs for failover
String serverURLs[] = {
	String("https://ltm-api-v2.hekinav.dev/") + CITY_CODE + "-ltm/" + BACKEND_VERSION + ".json",
};
const int numServers = sizeof(serverURLs) / sizeof(serverURLs[0]);
int currentServerIndex = 0;
int failedFetchCount = 0;

const char *ntpServers[] = {"pool.ntp.org"};
const char *time_zone = "EET-2EEST,M3.5.0/3,M10.5.0/4";

time_t lastMapDrawTime = 0;	 // Tracks the last time the map was drawn
time_t nextFetchTime = 0;	 // Tracks when the next update should occur
uint32_t modeStartTime = 0;	 // Tracks when the current mode started (for fast forward mode timing)
uint8_t fetchOffset = 0;	 // Random time ms to fetch (reduces server load)
uint8_t updateInterval = 30; // Default update interval in seconds

CRGB black = CRGB::Black;
std::vector<CRGB> colorTable;

#if defined(HKI_LTM)
String mapModes[] =
{
	"lines"
};
#elif defined(FIN_LTM)
String mapModes[] =
{
	"routes"
};
#else
String mapModes[] =
{
	"null"
};
#endif
int16_t currentMapMode = 0;


// --- Data structure for scheduled LED updates ---
struct LedUpdate
{
	uint16_t block;
	int colorId;
	time_t timestamp; // Timestamp for when the update should occur
};

std::vector<LedUpdate> ledUpdateSchedule;

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

String downloadJSON()
{
	HTTPClient http;
	String payload;

	String url = serverURLs[currentServerIndex];
	http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
	Serial.println(url + "?mode=" + mapModes[currentMapMode]);
	http.begin(url + "?mode=" + mapModes[currentMapMode]);

	int httpCode = http.GET();
	if (httpCode == HTTP_CODE_OK)
	{
		payload = http.getString();
		http.end();
		if (payload.length() == 0)
		{
			Serial.printf("Fetch from %s returned too little data (%d bytes)\n", url.c_str(), payload.length());
		}
		else
		{
			failedFetchCount = 0; // Reset failed fetch count on success
			return payload;
		}
	}

	Serial.printf("Fetch from %s returned: %i\n", url.c_str(), httpCode);
	http.end();
	failedFetchCount++;
	if (failedFetchCount > 3)
	{
		currentServerIndex++; // Try the next server on the next attempt
		currentServerIndex = currentServerIndex % numServers;
	}

	return String();
}

void setBlockColorId(uint8_t *blockColorIds, uint16_t block, int colorId)
{
	if (colorId < blockColorIds[block])
	{
		return; // Do not update if the new color is lower priority
	}

	blockColorIds[block] = colorId; // Update the color ID for the block

	// Get the actual color from the color table, defaulting to black if out of range
	CRGB color = (colorId >= 0 && colorId < static_cast<int>(colorTable.size())) ? colorTable[colorId] : black;

	setBlockColorRGB(block, color);
}

void drawRealtimeMap(time_t epoch)
{
	suspendDithering();
	clearLEDs();

	uint8_t blockColorIds[2000] = {0}; // Initialize all elements to 0

	// Draw the map based on the current LED update schedule
	for (const auto &update : ledUpdateSchedule)
	{
		setBlockColorId(blockColorIds, update.block, update.colorId);
	}

	resumeDithering();
}

float timetableRenderTime = 0.0f;
uint8_t printoutCounter = 0;

time_t parseLEDMap(const String &downloadedJson)
{
	JsonDocument doc;
	DeserializationError error = deserializeJson(doc, downloadedJson);

	if (error)
	{
		Serial.printf("JSON parse error: %s\n", error.c_str());
		return 0;
	}

	String version = doc["version"] | "";
	time_t baseTimestamp = doc["timestamp"] | 0;
	updateInterval = doc["update"] | updateInterval;
	JsonObject colors = doc["colors"];
	JsonArray updates = doc["updates"];

	if (String(BACKEND_VERSION) != version)
	{
		Serial.printf("Backend version mismatch: expected %s, got %s\n", BACKEND_VERSION, version.c_str());
	}

	// Serial.printf("%ld Base timestamp: %ld, Update offset: %d, Next fetch time: %ld\n",
	// 			  time(nullptr),
	// 			  baseTimestamp,
	// 			  updateInterval,
	// 			  nextFetchTime);

	// Populate colorTable from the JSON colors object
	colorTable.clear();
	for (JsonPair kv : colors)
	{
		JsonArray rgb = kv.value().as<JsonArray>();
		colorTable.push_back(CRGB(rgb[0] | 0, rgb[1] | 0, rgb[2] | 0));
	}

	ledUpdateSchedule.clear();
	for (JsonObject update : updates)
	{
		int block = update["b"];
		int colorId = update["c"][0];
		int offset = update["t"];

		// Schedule color update
		LedUpdate ledUpdate;
		ledUpdate.block = block;
		if (offset > 0)
		{
			ledUpdate.timestamp = baseTimestamp + offset;
		}
		else
		{
			ledUpdate.timestamp = 0;
		}
		ledUpdate.colorId = colorId;
		ledUpdateSchedule.push_back(ledUpdate);
	}

	return baseTimestamp;
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
		lastMapDrawTime = 0;	  // Force redraw
		modeStartTime = millis(); // Reset start time for fast forward mode
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
	modeStartTime = millis();  // Reset start time for fast forward mode
	lastMapDrawTime = 0;	   // Force immediate redraw
	brightness.setPower(true); // Ensure brightness is on when changing modes
	Serial.println("Mode button pressed");
}

void realtimeMode(time_t epoch, bool wiFiConnected)
{
	if (wiFiConnected)
	{
		// --- Fetch new data periodically ---
		if (epoch > nextFetchTime && millis() % 1000 > fetchOffset)
		{
			if (epoch > nextFetchTime + updateInterval && brightness.isOn())
			{
				setStatusLedState(WIFI_LED_PIN, LED_ON_GREEN, SERVER_LED_PIN, LED_BLINK_GREEN_FAST);
			}

			time_t timeOffset = 0;
			String downloadedJson = downloadJSON();
			if (downloadedJson.length() > 0)
			{
				if (brightness.isOn())
				{
					setStatusLedState(WIFI_LED_PIN, LED_ON_GREEN, SERVER_LED_PIN, LED_ON_GREEN);
				}
				timeOffset = epoch - parseLEDMap(downloadedJson);
			}
			else
			{
				if (failedFetchCount > 3 + numServers)
				{
					Serial.println("All servers failed to provide data.");
					if (brightness.isOn())
					{
						setStatusLedState(WIFI_LED_PIN, LED_ON_RED, SERVER_LED_PIN, LED_ON_RED);
					}
				}
			}

			nextFetchTime = constrain(nextFetchTime, epoch + 6, epoch + updateInterval);

			Serial.printf(
				"%s fetchDelay:%is MCU:%2.0f°C WiFi:%idBm\n", getLocalTime(epoch), timeOffset, temperatureRead(), WiFi.RSSI());
			Serial.flush();
		}

		// --- Push updates to the LED strips only if changes were made ---
		if (lastMapDrawTime < epoch)
		{
			drawRealtimeMap(epoch); // Draw the map with the current updates
			lastMapDrawTime = epoch;
		}
	}
	else
	{
		if (brightness.isOn())
		{
			if (millis() > 60 * 1000)
			{
				setStatusLedState(WIFI_LED_PIN, LED_ON_RED, SERVER_LED_PIN, LED_OFF);
			}
		}
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

	// --- WiFi Setup ---
	xTaskCreate(statusLedManagerTask, "Status LED Manager", 1024, NULL, 2, &statusLedTaskHandle);

	fetchOffset = random(0, 999); // Random delay between 0 and 999 ms to reduce server load

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
	time_t epoch = time(nullptr); // Get current time
	bool wiFiConnected = (WiFi.status() == WL_CONNECTED);
	if (!wiFiConnected)
	{
		manageWiFiConnection();

		realtimeMode(epoch, wiFiConnected);
	}
	realtimeMode(epoch, wiFiConnected);

	brightness.update();

	vTaskDelay(pdMS_TO_TICKS(30));
}
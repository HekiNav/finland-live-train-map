#pragma once

#include <ESPAsyncWebServer.h>
#include <Esp.h>
#include <ImprovWiFiLibrary.h>
#include <Preferences.h>
#include <WiFi.h>

extern Preferences preferences;

ImprovWiFi improvSerial(&Serial);
AsyncWebServer server(81);

#define MAX_SSID_LEN 32
#define MAX_PASS_LEN 64
#define MAX_WIFI_NETWORKS 16

/**
 * @brief Structure for storing WiFi credentials
 */
struct savedWiFiNetwork {
	char ssid[MAX_SSID_LEN];
	char password[MAX_PASS_LEN];
};

int wifiNetworkIndex = 0;				   // Index of the current WiFi network
unsigned long lastWiFiConnectAttempt = 0;  // Timestamp of the last WiFi connect attempt
uint8_t wifiConnectAttempts = 0;		   // Number of attempts for the current network

savedWiFiNetwork savedWiFi[MAX_WIFI_NETWORKS];

void setUpWebserver(AsyncWebServer &server);

/**
 * @brief Callback for Improv WiFi errors
 * 
 * @param err Error type from Improv WiFi
 */
void onImprovWiFiErrorCb(ImprovTypes::Error err) {
	Serial.printf("Improv WiFi Error: %d\n", err);
	server.end();
	server.begin();
}

/**
 * @brief Save WiFi credentials to NVS
 */
void exportWiFi() {
	preferences.begin("wifi");
	preferences.putBytes("wifi", savedWiFi, sizeof(savedWiFi));
	preferences.end();
}

/**
 * @brief Load WiFi credentials from NVS
 */
void importWiFi() {
	preferences.begin("wifi", true);
	preferences.getBytes("wifi", savedWiFi, sizeof(savedWiFi));
	preferences.end();
}

/**
 * @brief Callback when WiFi is connected via Improv
 * 
 * Saves the new network at the top of the saved networks list
 * and persists it to NVS.
 * 
 * @param ssid Network SSID
 * @param password Network password
 */
void onImprovWiFiConnectedCb(const char *ssid, const char *password) {
	// Move the networks all down one position
	for (int i = MAX_WIFI_NETWORKS - 1; i > 0; i--) {
		strncpy(savedWiFi[i].ssid, savedWiFi[i - 1].ssid, MAX_SSID_LEN);
		strncpy(savedWiFi[i].password, savedWiFi[i - 1].password, MAX_PASS_LEN);
	}

	// Save the new network at the top
	strncpy(savedWiFi[0].ssid, ssid, MAX_SSID_LEN);
	strncpy(savedWiFi[0].password, password, MAX_PASS_LEN);

	// Save the updated WiFi networks to Preferences
	exportWiFi();

	// Restart the web server
	server.end();
	server.begin();
}

/**
 * @brief Initialize WiFi with Improv serial provisioning
 * 
 * Loads saved credentials from NVS and configures Improv WiFi
 * for serial-based provisioning.
 * 
 * @return true if saved WiFi credentials were found, false otherwise
 */
bool WiFiImprovSetup() {
	importWiFi();

#if defined(CONFIG_IDF_TARGET_ESP32S2)
	enum ImprovTypes::ChipFamily chip = ImprovTypes::ChipFamily::CF_ESP32_S2;
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
	enum ImprovTypes::ChipFamily chip = ImprovTypes::ChipFamily::CF_ESP32_C3;
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
	enum ImprovTypes::ChipFamily chip = ImprovTypes::ChipFamily::CF_ESP32_S3;
#else
	error("Unknown chip for Improv WiFi");
#endif

	improvSerial.setDeviceInfo(chip, FIRMWARE, FIRMWARE_VERSION, ARDUINO_BOARD, "http://{LOCAL_IPV4}/");
	improvSerial.onImprovError(onImprovWiFiErrorCb);
	improvSerial.onImprovConnected(onImprovWiFiConnectedCb);
	setUpWebserver(server);

	bool savedWifiFound = false;
	for (int i = 0; i < MAX_WIFI_NETWORKS; i++) {
		if (strlen(savedWiFi[i].ssid) > 0) {
			savedWifiFound = true;
			break;
		}
	}

	return savedWifiFound;
}

const char index_html[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>LED Rails Device</title>
  <style>
    body {
      background: #222;
      color: #fff;
      font-family: -apple-system, system-ui, BlinkMacSystemFont, "Segoe UI", Roboto, Ubuntu, sans-serif;
      margin: 0;
      padding: 0;
      min-height: 100vh;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
    }
    .container {
      background: #222;
      border-radius: 12px;
      box-shadow: 0 2px 12px rgba(0,0,0,0.08);
      padding: 32px 24px;
      max-width: 600px;
      width: 90%;
      text-align: center;
    }
    h1 {
      color: #fff;
      font-family: inherit;
      margin-bottom: 16px;
    }
    h2 {
      color: #fff;
      font-family: inherit;
      font-weight: 400;
      margin-top: 0;
    }
    @media (max-width: 600px) {
      .container { padding: 18px 4px; }
      h1 { font-size: 1.6em; }
      h2 { font-size: 1.1em; }
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>Kea Studios</h1>
    <h2>This is just an empty page for now, in the future settings will be added here...</h2>
  </div>
</body>
</html>

)=====";

/**
 * @brief Configure and start the async web server
 * 
 * @param server AsyncWebServer instance to configure
 */
void setUpWebserver(AsyncWebServer &server) {
	server.on("/favicon.ico", [](AsyncWebServerRequest *request) {
		request->send(404);
	});

	// Serve Basic HTML Page
	server.on("/", HTTP_ANY, [](AsyncWebServerRequest *request) {
		AsyncWebServerResponse *response = request->beginResponse(200, "text/html", index_html);
		response->addHeader(
			"Cache-Control", "public,max-age=31536000");  // save this file to cache for 1 year (unless you refresh)
		request->send(response);
		Serial.println("Served Basic HTML Page");
	});

	server.onNotFound([](AsyncWebServerRequest *request) {
		request->send(404);
	});

	server.begin();
}

/**
 * @brief FreeRTOS task for handling Improv WiFi serial communication
 * 
 * @param param Unused task parameter
 */
void improvSerialTask(void *param) {
	while (true) {
		while (Serial.available() > 0) {
			improvSerial.handleSerial();
		}
		vTaskDelay(pdMS_TO_TICKS(20));
	}
}

/**
 * @brief Manage WiFi connection with failover across saved networks
 * 
 * Attempts to connect to saved WiFi networks, cycling through them
 * on repeated failures.
 */
void manageWiFiConnection() {
	const unsigned long attemptTimeout = 5000;	// 5 seconds for each attempt
	const uint8_t maxAttempts = 3;				// Max attempts per network

	if (millis() - lastWiFiConnectAttempt > attemptTimeout || lastWiFiConnectAttempt == 0) {
		if (wifiConnectAttempts < maxAttempts) {
			wifiConnectAttempts++;
		} else {
			wifiConnectAttempts = 0;

			while (strlen(savedWiFi[wifiNetworkIndex].ssid) == 0 && wifiNetworkIndex < MAX_WIFI_NETWORKS) {
				wifiNetworkIndex++;	 // Skip empty SSIDs
									 // printf("Skipping empty WiFi slot %i\n", wifiNetworkIndex);
			}

			if (wifiNetworkIndex >= MAX_WIFI_NETWORKS) {
				wifiNetworkIndex = 0;
			}
		}

		if (strlen(savedWiFi[wifiNetworkIndex].ssid) != 0) {
			// Attempt to connect to the current network
			Serial.printf("Attempting to connect to saved network %i: %s\n", wifiNetworkIndex, savedWiFi[wifiNetworkIndex].ssid);
			WiFi.disconnect();	// Disconnect from any current network
			WiFi.begin(savedWiFi[wifiNetworkIndex].ssid, savedWiFi[wifiNetworkIndex].password);
			WiFi.setTxPower(WIFI_POWER_15dBm);	// Set WiFi power to avoid interference
		}

		lastWiFiConnectAttempt = millis();
	}
}
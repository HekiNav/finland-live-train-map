#include "ledManager.h"

// Define the LED arrays
// Pins and pixel counts defined in the board file (./boards/)
#if defined(LED_1_PIN)
CRGB leds1[LED_1_PIXELS];
#endif
#if defined(LED_2_PIN)
CRGB leds2[LED_2_PIXELS];
#endif
#if defined(LED_3_PIN)
CRGB leds3[LED_3_PIXELS];
#endif
#if defined(LED_4_PIN)
CRGB leds4[LED_4_PIXELS];
#endif
#if defined(LED_5_PIN)
CRGB leds5[LED_5_PIXELS];
#endif
#if defined(LED_6_PIN)
CRGB leds6[LED_6_PIXELS];
#endif
#if defined(LED_7_PIN)
CRGB leds7[LED_7_PIXELS];
#endif
#if defined(LED_8_PIN)
CRGB leds8[LED_8_PIXELS];
#endif

// Structure to hold strip information for runtime iteration
struct LedStripInfo
{
	CRGB *leds;
	int numPixels;
	int startBlock;
};

std::vector<LedStripInfo> ledStrips;

TaskHandle_t fastLEDDitheringTaskHandle;

void enablePower()
{
#if defined(LVL_Shifter_EN)
	pinMode(LVL_Shifter_EN, OUTPUT);
	digitalWrite(LVL_Shifter_EN, LOW); // Enable LVL Shifter
#endif
#if defined(LED_5V_EN)
	pinMode(LED_5V_EN, OUTPUT);
	digitalWrite(LED_5V_EN, HIGH); // Enable 5V Power
#endif
}

void disablePower()
{
#if defined(LED_5V_EN)
	pinMode(LED_5V_EN, OUTPUT);
	digitalWrite(LED_5V_EN, LOW); // Disable 5V Power
#endif
#if defined(LVL_Shifter_EN)
	pinMode(LVL_Shifter_EN, OUTPUT);
	digitalWrite(LVL_Shifter_EN, HIGH); // Disable LVL Shifter
#endif
}

bool anyLedsOn()
{
	for (const auto &strip : ledStrips)
	{
		for (int j = 0; j < strip.numPixels; j++)
		{
			if (strip.leds[j] != CRGB::Black)
			{
				return true;
			}
		}
	}
	return false;
}

void fastLEDDitheringTask(void *pvParameters)
{
	const TickType_t frameDelay = pdMS_TO_TICKS(20); // 50fps = 20ms interval
	enum class ledState
	{
		OFF,
		TURNING_ON,
		ON,
		TURNING_OFF
	};
	ledState currentState = ledState::OFF;

	while (true)
	{
		switch (currentState)
		{
		case ledState::OFF:
			if (FastLED.getBrightness() > 0)
			{
				currentState = ledState::TURNING_ON;
			}
			vTaskDelay(frameDelay);
			break;

		case ledState::TURNING_ON:
			FastLED.clear(true);
			enablePower();
			currentState = ledState::ON;
			break;

		case ledState::ON:
			FastLED.show();
			vTaskDelay(frameDelay);

			if (FastLED.getBrightness() == 0)
			{
				currentState = ledState::TURNING_OFF;
			}
			break;

		case ledState::TURNING_OFF:
			disablePower();
			currentState = ledState::OFF;
			break;

		default:
			break;
		}
	}
}

void setupLeds()
{
	disablePower();

	ledStrips.clear();

	// Channel 1
#if defined(LED_1_PIN)
	FastLED.addLeds<WS2811, LED_1_PIN, GRB>(leds1, LED_1_PIXELS);
	ledStrips.push_back({leds1, LED_1_PIXELS, LED_1_START});
#endif

	// Channel 2
#if defined(LED_2_PIN)
	FastLED.addLeds<WS2811, LED_2_PIN, GRB>(leds2, LED_2_PIXELS);
	ledStrips.push_back({leds2, LED_2_PIXELS, LED_2_START});
#endif

	// Channel 3
#if defined(LED_3_PIN)
	FastLED.addLeds<WS2811, LED_3_PIN, GRB>(leds3, LED_3_PIXELS);
	ledStrips.push_back({leds3, LED_3_PIXELS, LED_3_START});
#endif

	// Channel 4
#if defined(LED_4_PIN)
	FastLED.addLeds<WS2811, LED_4_PIN, GRB>(leds4, LED_4_PIXELS);
	ledStrips.push_back({leds4, LED_4_PIXELS, LED_4_START});
#endif

	// Channel 5
#if defined(LED_5_PIN)
	FastLED.addLeds<WS2811, LED_5_PIN, GRB>(leds5, LED_5_PIXELS);
	ledStrips.push_back({leds5, LED_5_PIXELS, LED_5_START});
#endif

	// Channel 6
#if defined(LED_6_PIN)
	FastLED.addLeds<WS2811, LED_6_PIN, GRB>(leds6, LED_6_PIXELS);
	ledStrips.push_back({leds6, LED_6_PIXELS, LED_6_START});
#endif

	// Channel 7
#if defined(LED_7_PIN)
	FastLED.addLeds<WS2811, LED_7_PIN, GRB>(leds7, LED_7_PIXELS);
	ledStrips.push_back({leds7, LED_7_PIXELS, LED_7_START});
#endif

	// Channel 8
#if defined(LED_8_PIN)
	FastLED.addLeds<WS2811, LED_8_PIN, GRB>(leds8, LED_8_PIXELS);
	ledStrips.push_back({leds8, LED_8_PIXELS, LED_8_START});
#endif

	FastLED.clear(true);
	FastLED.setDither(BINARY_DITHER);

	enablePower();

	xTaskCreate(fastLEDDitheringTask, "FastLED Dithering", 2048, NULL, 2, &fastLEDDitheringTaskHandle);
}

void setBlockColorRGB(uint16_t block, CRGB color)
{
	// Apply gamma correction (γ = 2.0)
	auto gammaCorrect = [](float value) -> uint8_t
	{
		return static_cast<uint8_t>(pow(value / 255.0f, 2.0) * 255.0f);
	};

	color.r = gammaCorrect(color.r);
	color.g = gammaCorrect(color.g);
	color.b = gammaCorrect(color.b);

	if (block == 0)
		return; // Ignore block 0

	bool found = false;
	for (const auto &strip : ledStrips)
	{
		if (block >= strip.startBlock && block < strip.startBlock + strip.numPixels)
		{
			strip.leds[block - strip.startBlock] = color;
			found = true;
			break;
		}
	}

	if (!found)
	{
		Serial.printf("Block %d is out of range.\n", block);
	}
}

void clearLEDs()
{
	for (const auto &strip : ledStrips)
	{
		fill_solid(strip.leds, strip.numPixels, CRGB::Black);
	}
}

void setAllLedsColor(CRGB color)
{
	for (const auto &strip : ledStrips)
	{
		fill_solid(strip.leds, strip.numPixels, color);
	}
}

void suspendDithering()
{
	vTaskSuspend(fastLEDDitheringTaskHandle);
}

void resumeDithering()
{
	vTaskResume(fastLEDDitheringTaskHandle);
}

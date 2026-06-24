#pragma once

#include <Arduino.h>
#include <FastLED.h>
#include <Preferences.h>

extern Preferences preferences;

/**
 * @brief Manages LED brightness using manual button controls
 * 
 * Provides simple brightness control without an ambient light sensor.
 * Settings are persisted to NVS flash.
 */
class BrightnessManager {
  public:
	BrightnessManager() {}

	/**
	 * @brief Load saved brightness and apply it
	 */
	void begin() {
		load(preferences);
		setBrightness();
	}

	/**
	 * @brief Increase brightness by one step
	 */
	void increase() {
		brightness += BRIGHTNESS_STEP;
		brightness = constrain(brightness, MIN_BRIGHTNESS, MAX_BRIGHTNESS);
		setBrightness();
	}

	/**
	 * @brief Decrease brightness by one step
	 */
	void decrease() {
		brightness -= BRIGHTNESS_STEP;
		brightness = constrain(brightness, MIN_BRIGHTNESS, MAX_BRIGHTNESS);
		setBrightness();
	}

	/**
	 * @brief Toggle power on/off
	 */
	void toggle() {
		powerOn = !powerOn;
		setBrightness();
	}

	/**
	 * @brief Set power state
	 * 
	 * @param on True to turn on, false to turn off
	 */
	void setPower(bool on) {
		powerOn = on;
		setBrightness();
	}

	/**
	 * @brief Save brightness to NVS
	 * 
	 * @param preferences Preferences instance for NVS access
	 */
	void save(Preferences &preferences) {
		preferences.begin("brightness");
		preferences.putInt("brightness", int(brightness));
		preferences.end();
	}

	/**
	 * @brief Load brightness from NVS
	 * 
	 * @param preferences Preferences instance for NVS access
	 */
	void load(Preferences &preferences) {
		preferences.begin("brightness");
		brightness = float(preferences.getInt("brightness", int(brightness)));
		preferences.end();
	}

	/**
	 * @brief No-op update for manual brightness (allows background tasks to run)
	 */
	void update() {
		yield();  // Allow background tasks to run
	}

	/**
	 * @brief Apply the current brightness to the LED strip
	 */
	void setBrightness() {
		// Apply gamma correction for perceived brightness
		float gamma = 2.2f;
		uint8_t gammaBrightness = static_cast<uint8_t>(pow((brightness / 255.0f), gamma) * 255.0f);

		// Update the LEDs
		FastLED.setBrightness(powerOn ? gammaBrightness : 0);

		Serial.printf("Brightness set to %0.0f/255\n", brightness);

		save(preferences);
	}

	/**
	 * @brief Check if LEDs are powered on
	 * 
	 * @return true if powered on
	 */
	bool isOn() {
		return powerOn;
	}


  private:
	float brightness = MIN_BRIGHTNESS + BRIGHTNESS_STEP;  // Current brightness level (0-255)
	bool powerOn = true;								  // Power state
};
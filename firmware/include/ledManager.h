#pragma once

#include <FastLED.h>
#include <vector>

/**
 * @brief Initialize all LED strips and start the dithering task
 */
void setupLeds();

/**
 * @brief Set the color of a specific track block
 * 
 * @param block Block number to set
 * @param color RGB color value
 */
void setBlockColorRGB(uint16_t block, CRGB color);

/**
 * @brief Turn off all LEDs across all strips
 */
void clearLEDs();

/**
 * @brief Set all LEDs to the same color
 * 
 * @param color RGB color value
 */
void setAllLedsColor(CRGB color);

/**
 * @brief Suspend the FastLED dithering task
 */
void suspendDithering();

/**
 * @brief Resume the FastLED dithering task
 */
void resumeDithering();

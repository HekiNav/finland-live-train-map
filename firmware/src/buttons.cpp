#include "buttons.h"

// Define the global queue handle
QueueHandle_t buttonQueue;

void ButtonManager::add(uint8_t pin, ButtonCallback cb, ButtonCallback lpcb, uint16_t longPressDuration) {
	buttons.push_back({ pin, cb, lpcb, longPressDuration, HIGH, 0, 0 });
}

void ButtonManager::setCallback(uint8_t pin, ButtonCallback cb) {
	for (auto& btn : buttons) {
		if (btn.pin == pin) {
			btn.callback = cb;
			return;
		}
	}
	Serial.printf("Button on pin %d not found!\n", pin);
}

void ButtonManager::setLongPressCallback(uint8_t pin, ButtonCallback lpcb) {
	for (auto& btn : buttons) {
		if (btn.pin == pin) {
			btn.longPressCallback = lpcb;
			return;
		}
	}
	Serial.printf("Button on pin %d not found!\n", pin);
}

void ButtonManager::begin() {
	buttonQueue = xQueueCreate(20, sizeof(ButtonEvent));
	if (buttonQueue == NULL) {
		Serial.println("Failed to create button queue!");
		return;
	}

	for (auto& btn : buttons) {
		pinMode(btn.pin, INPUT_PULLUP);
		attachInterruptArg(digitalPinToInterrupt(btn.pin),
						   isrWrapper,
						   &btn,
						   CHANGE  // Trigger on both rising and falling edges
		);
	}

	xTaskCreate(buttonTask, "ButtonTask", 4096, this, 2, NULL);
}

void IRAM_ATTR ButtonManager::isrWrapper(void* arg) {
	Button* button = static_cast<Button*>(arg);
	TickType_t now = xTaskGetTickCountFromISR();

	bool newState = digitalRead(button->pin);

	if (newState != button->state) {
		button->state = newState;

		if (newState == LOW) {
			// Button pressed
			button->fallingTick = now;
			button->pressStartTick = now;
			button->longPressTriggered = false;	 // Reset long press flag
		} else {
			// Button released
			button->risingTick = now;
			// Only send short press if it's debounced and not already handled as long press
			if ((button->risingTick - button->fallingTick) > pdMS_TO_TICKS(DEBOUNCE_MS) && !button->longPressTriggered) {
				ButtonEvent event = { button->pin, false };
				BaseType_t xHigherPriorityTaskWoken = pdFALSE;
				xQueueSendFromISR(buttonQueue, &event, &xHigherPriorityTaskWoken);
				if (xHigherPriorityTaskWoken) {
					portYIELD_FROM_ISR();
				}
			}
		}
	}
}

void ButtonManager::buttonTask(void* pvParameters) {
	ButtonManager* manager = static_cast<ButtonManager*>(pvParameters);
	ButtonEvent event;
	TickType_t lastCheckTime = xTaskGetTickCount();

	while (true) {
		// Check for long press events
		TickType_t currentTime = xTaskGetTickCount();
		TickType_t timeSinceLastCheck = currentTime - lastCheckTime;
		lastCheckTime = currentTime;

		for (auto& btn : manager->buttons) {
			// If button is currently pressed, has long press callback, and long press not yet triggered
			if (btn.state == LOW && btn.longPressCallback != nullptr && !btn.longPressTriggered) {
				// Check if enough time has passed for a long press
				TickType_t pressDuration = currentTime - btn.pressStartTick;
				if (pressDuration >= pdMS_TO_TICKS(btn.longPressDuration)) {
					// Send long press event
					ButtonEvent longPressEvent = { btn.pin, true };
					xQueueSend(buttonQueue, &longPressEvent, 0);
					btn.longPressTriggered = true;	// Mark long press as triggered
				}
			}
		}

		// Process button events from queue
		if (xQueueReceive(buttonQueue, &event, pdMS_TO_TICKS(buttonTaskPollingInterval))) {
			for (auto& btn : manager->buttons) {
				if (btn.pin == event.pin) {
					if (event.isLongPress) {
						if (btn.longPressCallback) {
							btn.longPressCallback();
						}
					} else {
						btn.callback();
					}
					break;
				}
			}
		}
	}
}

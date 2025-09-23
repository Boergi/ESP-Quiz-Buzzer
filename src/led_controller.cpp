#include "led_controller.h"
#include "mqtt_server.h"

// Global instance
LEDController* ledController = nullptr;

LEDController::LEDController(Adafruit_NeoPixel& ledStrip) : strip(ledStrip) {
}

// Basic LED functions
void LEDController::setPixelColor(uint16_t pixel, const Rgb& color) {
  if (pixel < LED_COUNT) {
    strip.setPixelColor(pixel, strip.Color(color.r, color.g, color.b));
  }
}

void LEDController::clearAllLEDs() {
  strip.clear();
  strip.show();
}

void LEDController::showWhiteTest() {
  for (uint16_t i = 0; i < LED_COUNT; i++) {
    setPixelColor(i, COLOR_WHITE);
  }
  strip.show();
}

void LEDController::showLEDs() {
  strip.show();
}

// Server-specific LED functions (18 LEDs)
void LEDController::setActivePlayerLEDs(const Rgb& color) {
  // LEDs 0-7 (positions 1-8) for active player
  for (uint8_t i = 0; i < 8; i++) {
    setPixelColor(i, color);
  }
  strip.show();
}

void LEDController::setQueueLED(uint8_t position, const Rgb& color) {
  // LEDs 8-17 (positions 9-18) for queue display
  if (position < 10) {
    setPixelColor(8 + position, color);
    strip.show();
  }
}

void LEDController::clearQueueLEDs() {
  for (uint8_t i = 8; i < 18; i++) {
    setPixelColor(i, COLOR_BLACK);
  }
  strip.show();
}

void LEDController::updateServerLEDs() {
  // Clear all LEDs first
  clearAllLEDs();
  
  // Show active client on LEDs 0-7 (positions 1-8)
  if (activeClientIndex >= 0 && activeClientIndex < queueLength) {
    String activeClientId = buzzQueue[activeClientIndex];
    for (uint8_t i = 0; i < gameClientCount; i++) {
      if (gameClients[i].id == activeClientId) {
        setActivePlayerLEDs(gameClients[i].color);
        break;
      }
    }
  }
  
  // Show buzz queue on LEDs 8-17 (positions 9-18)
  for (uint8_t i = 0; i < queueLength && i < 10; i++) {
    String queueClientId = buzzQueue[i];
    for (uint8_t j = 0; j < gameClientCount; j++) {
      if (gameClients[j].id == queueClientId) {
        setQueueLED(i, gameClients[j].color);
        break;
      }
    }
  }
}

// Animation functions
void LEDController::testColorCycle() {
  static uint8_t colorIndex = 0;
  static uint32_t lastUpdate = 0;
  
  if (millis() - lastUpdate > 1000) {
    clearAllLEDs();
    
    // Show current color on active player LEDs
    setActivePlayerLEDs(PLAYER_COLORS[colorIndex]);
    
    // Show color info on Serial
    Serial.printf("Testing Color %d: R=%d G=%d B=%d\n", 
                  colorIndex + 1, 
                  PLAYER_COLORS[colorIndex].r,
                  PLAYER_COLORS[colorIndex].g,
                  PLAYER_COLORS[colorIndex].b);
    
    colorIndex = (colorIndex + 1) % MAX_CLIENTS;
    lastUpdate = millis();
  }
}

void LEDController::testQueueDisplay() {
  static uint8_t queuePos = 0;
  static uint32_t lastUpdate = 0;
  
  if (millis() - lastUpdate > 500) {
    clearQueueLEDs();
    
    // Light up queue position with different colors
    for (uint8_t i = 0; i <= queuePos && i < MAX_CLIENTS; i++) {
      setQueueLED(i, PLAYER_COLORS[i]);
    }
    
    Serial.printf("Queue Test: Position %d\n", queuePos + 1);
    
    queuePos = (queuePos + 1) % (MAX_CLIENTS + 2); // +2 for clear cycles
    lastUpdate = millis();
  }
}

void LEDController::showConnectedClients() {
  static uint32_t lastUpdate = 0;
  if (millis() - lastUpdate > 100) {
    clearAllLEDs();
    for (uint8_t i = 0; i < gameClientCount && i < 8; i++) {
      setPixelColor(i, gameClients[i].color);
    }
    strip.show();
    lastUpdate = millis();
  }
}

void LEDController::animateReady() {
  // Show ready state (all queue LEDs blinking white)
  static uint32_t readyBlink = 0;
  if (millis() - readyBlink > 500) {
    static bool blinkState = false;
    if (blinkState) {
      for (uint8_t i = 8; i < 18; i++) {
        setPixelColor(i, COLOR_WHITE);
      }
    } else {
      clearQueueLEDs();
    }
    strip.show();
    blinkState = !blinkState;
    readyBlink = millis();
  }
}

void LEDController::animateOpen() {
  // Show "question open" animation (green breathing)
  static uint32_t openPulse = 0;
  if (millis() - openPulse > 100) {
    static uint8_t brightness = 0;
    static int8_t direction = 1;
    
    brightness += direction * 8;
    if (brightness >= 255 || brightness <= 0) {
      direction = -direction;
    }
    
    Rgb pulseColor(0, brightness, 0); // Green pulse
    setActivePlayerLEDs(pulseColor);
    openPulse = millis();
  }
}

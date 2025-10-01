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

void LEDController::showRGBTest() {
  // Test all 3 basic colors on ALL 18 LEDs to verify RGB functionality
  Serial.println("Testing RED on all 18 LEDs...");
  for (uint16_t i = 0; i < LED_COUNT; i++) {
    setPixelColor(i, Rgb(255, 0, 0)); // RED
  }
  strip.show();
  delay(1000);
  
  Serial.println("Testing GREEN on all 18 LEDs...");
  for (uint16_t i = 0; i < LED_COUNT; i++) {
    setPixelColor(i, Rgb(0, 255, 0)); // GREEN
  }
  strip.show();
  delay(1000);
  
  Serial.println("Testing BLUE on all 18 LEDs...");
  for (uint16_t i = 0; i < LED_COUNT; i++) {
    setPixelColor(i, Rgb(0, 0, 255)); // BLUE
  }
  strip.show();
  delay(1000);
  
  Serial.println("RGB test complete on all 18 LEDs!");
  clearAllLEDs();
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
  // Don't call strip.show() here - let caller decide when to update
}

void LEDController::setQueueLED(uint8_t position, const Rgb& color) {
  // LEDs 8-17 (positions 9-18) for queue display
  if (position < 10) {
    setPixelColor(8 + position, color);
    // Don't call strip.show() here - let caller decide when to update
  }
}

void LEDController::clearQueueLEDs() {
  for (uint8_t i = 8; i < 18; i++) {
    setPixelColor(i, COLOR_BLACK);
  }
  // Don't call strip.show() here - let caller decide when to update
}

void LEDController::updateServerLEDs() {
  // Clear all LEDs first (but don't show yet)
  strip.clear();
  
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
  
  // Now show all changes at once
  showLEDs();
}

// Animation functions

void LEDController::testQueueDisplay() {
  static uint8_t queuePos = 0;
  static uint32_t lastUpdate = 0;
  
  if (millis() - lastUpdate > 500) {
    clearQueueLEDs();
    
    // Light up queue position with different colors
    for (uint8_t i = 0; i <= queuePos && i < MAX_CLIENTS; i++) {
      setQueueLED(i, PLAYER_COLORS[i]);
    }
    showLEDs(); // Show all changes at once
    
    Serial.printf("Queue Test: Position %d\n", queuePos + 1);
    
    queuePos = (queuePos + 1) % (MAX_CLIENTS + 2); // +2 for clear cycles
    lastUpdate = millis();
  }
}

void LEDController::showConnectedClients() {
  static uint32_t lastUpdate = 0;
  if (millis() - lastUpdate > 100) {
    strip.clear(); // Clear but don't show yet
    
    // Show connected clients on LEDs 9-18 (queue area)
    for (uint8_t i = 0; i < gameClientCount && i < MAX_CLIENTS; i++) {
      setQueueLED(i, gameClients[i].color);
    }
    
    // LEDs 1-8 remain off (status area)
    showLEDs(); // Show all changes at once
    lastUpdate = millis();
  }
}

void LEDController::animateReady() {
  // Show ready state - connected clients with beautiful sine-wave pulsing like clients
  static uint32_t lastUpdate = 0;
  if (millis() - lastUpdate > PULSE_SPEED_MS) { // Same timing as client pulse
    
    // Calculate smooth sine-wave breathing effect (same as clients)
    float breath = (sin(millis() * 0.003) + 1.0) * 0.5; // 0.0 to 1.0
    uint8_t brightness = (uint8_t)(breath * 255);
    
    // Clear all LEDs first
    for (uint16_t i = 0; i < LED_COUNT; i++) {
      setPixelColor(i, Rgb(0, 0, 0));
    }
    
    // Show connected clients with beautiful pulsing in their colors on LEDs 9-18
    for (uint8_t i = 0; i < gameClientCount && i < 10; i++) {
      if (gameClients[i].connected) {
        // Calculate smooth pulsed color using sine wave (same as clients)
        Rgb pulseColor(
          (uint8_t)(gameClients[i].color.r * brightness / 255),
          (uint8_t)(gameClients[i].color.g * brightness / 255),
          (uint8_t)(gameClients[i].color.b * brightness / 255)
        );
        
        // Set LED at position 9+i (index 8+i)
        setPixelColor(8 + i, pulseColor);
      }
    }
    
    showLEDs(); // Show all changes at once
    lastUpdate = millis();
  }
}

void LEDController::animateOpen() {
  // Show "question open" animation (green breathing) - ONLY LEDs 1-8
  static uint32_t openPulse = 0;
  if (millis() - openPulse > 100) {
    static uint8_t brightness = 0;
    static int8_t direction = 1;
    
    brightness += direction * 8;
    if (brightness >= 255 || brightness <= 0) {
      direction = -direction;
    }
    
    // Clear ALL LEDs first
    for (uint16_t i = 0; i < LED_COUNT; i++) {
      setPixelColor(i, Rgb(0, 0, 0));
    }
    
    // Only show green pulse on LEDs 1-8 (active player area)
    Rgb pulseColor(0, brightness, 0); // Green pulse
    setActivePlayerLEDs(pulseColor);
    
    showLEDs(); // Show all changes at once
    openPulse = millis();
  }
}

#include "client_led_controller.h"

// Global instance
ClientLEDController* clientLedController = nullptr;

ClientLEDController::ClientLEDController(Adafruit_NeoPixel& ledStrip) : strip(ledStrip) {
}

// Basic LED functions
void ClientLEDController::setPixelColor(uint16_t pixel, const Rgb& color) {
  if (pixel < LED_COUNT) {
    strip.setPixelColor(pixel, strip.Color(color.r, color.g, color.b));
  }
}

void ClientLEDController::clearAllLEDs() {
  strip.clear();
  strip.show();
}

void ClientLEDController::setAllLEDs(const Rgb& color) {
  for (uint16_t i = 0; i < LED_COUNT; i++) {
    setPixelColor(i, color);
  }
  strip.show();
}

void ClientLEDController::showWhiteTest() {
  setAllLEDs(COLOR_WHITE);
}

// Client-specific animations
void ClientLEDController::showSolidColor(const Rgb& color) {
  // Simple solid color display
  setAllLEDs(color);
}

void ClientLEDController::animateIdle(const Rgb& color) {
  // Soft pulse in assigned color
  static uint32_t lastUpdate = 0;
  if (millis() - lastUpdate > PULSE_SPEED_MS) {
    float breath = (sin(millis() * 0.003) + 1.0) * 0.5; // 0.0 to 1.0
    uint8_t brightness = (uint8_t)(breath * 255);
    
    Rgb pulseColor(
      (uint8_t)(color.r * brightness / 255),
      (uint8_t)(color.g * brightness / 255),
      (uint8_t)(color.b * brightness / 255)
    );
    
    setAllLEDs(pulseColor);
    lastUpdate = millis();
  }
}

void ClientLEDController::animateActiveSpin(const Rgb& color) {
  // Fast spinning animation
  static uint32_t lastUpdate = 0;
  static uint16_t animStep = 0;
  
  if (millis() - lastUpdate > SPIN_SPEED_MS) {
    clearAllLEDs();
    
    // Light up current position
    uint16_t position = animStep % LED_COUNT;
    setPixelColor(position, color);
    
    // Add trailing LEDs for better effect
    for (uint8_t i = 1; i <= 2 && i <= position; i++) {
      Rgb trailColor(
        (uint8_t)(color.r / (i + 1)),
        (uint8_t)(color.g / (i + 1)),
        (uint8_t)(color.b / (i + 1))
      );
      setPixelColor(position - i, trailColor);
    }
    
    strip.show();
    animStep++;
    lastUpdate = millis();
  }
}

void ClientLEDController::animateFlash(const Rgb& color) {
  // Quick flash animation
  static uint32_t flashStart = 0;
  static bool flashActive = false;
  
  if (!flashActive) {
    flashStart = millis();
    flashActive = true;
  }
  
  uint32_t elapsed = millis() - flashStart;
  
  if (elapsed < FLASH_DURATION_MS * 4) { // 4 flashes
    if ((elapsed / FLASH_DURATION_MS) % 2 == 0) {
      setAllLEDs(color);
    } else {
      clearAllLEDs();
    }
  } else {
    // Flash finished
    flashActive = false;
    clearAllLEDs();
  }
}

void ClientLEDController::animateCelebration() {
  // Rainbow animation
  static uint32_t lastUpdate = 0;
  static uint16_t animStep = 0;
  
  if (millis() - lastUpdate > 50) {
    for (uint16_t i = 0; i < LED_COUNT; i++) {
      uint8_t hue = (animStep + i * (256 / LED_COUNT)) & 255;
      
      // Simple HSV to RGB conversion for rainbow
      uint8_t sector = hue / 43;
      uint8_t remainder = (hue - (sector * 43)) * 6;
      uint8_t p = 0;
      uint8_t q = 255 - remainder;
      uint8_t t = remainder;
      
      Rgb rainbowColor;
      switch (sector) {
        case 0: rainbowColor = Rgb(255, t, p); break;
        case 1: rainbowColor = Rgb(q, 255, p); break;
        case 2: rainbowColor = Rgb(p, 255, t); break;
        case 3: rainbowColor = Rgb(p, q, 255); break;
        case 4: rainbowColor = Rgb(t, p, 255); break;
        default: rainbowColor = Rgb(255, p, q); break;
      }
      
      setPixelColor(i, rainbowColor);
    }
    strip.show();
    animStep += 5;
    lastUpdate = millis();
  }
}

void ClientLEDController::animateDisconnected() {
  // Slow red pulse to indicate no connection
  static uint32_t lastUpdate = 0;
  if (millis() - lastUpdate > 200) {
    static bool redState = false;
    setAllLEDs(redState ? COLOR_ERROR : COLOR_BLACK);
    redState = !redState;
    lastUpdate = millis();
  }
}

void ClientLEDController::showLocked() {
  // All LEDs white (after buzz)
  setAllLEDs(COLOR_WHITE);
}

// Test functions
void ClientLEDController::runColorTest() {
  Serial.println("Running color test cycle...");
  
  for (uint8_t i = 0; i < MAX_CLIENTS; i++) {
    Serial.printf("Testing color %d: R=%d G=%d B=%d\n", 
                  i + 1, PLAYER_COLORS[i].r, PLAYER_COLORS[i].g, PLAYER_COLORS[i].b);
    setAllLEDs(PLAYER_COLORS[i]);
    delay(1000);
  }
  
  clearAllLEDs();
  Serial.println("Color test complete");
}

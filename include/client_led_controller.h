#pragma once
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"

// Client LED Controller class (8 LEDs)
class ClientLEDController {
private:
  Adafruit_NeoPixel& strip;
  
public:
  ClientLEDController(Adafruit_NeoPixel& ledStrip);
  
  // Basic LED functions
  void setPixelColor(uint16_t pixel, const Rgb& color);
  void clearAllLEDs();
  void setAllLEDs(const Rgb& color);
  void showRGBTest();
  
  // Client-specific animations
  void showSolidColor(const Rgb& color);        // Solid color (not OPEN)
  void animateIdle(const Rgb& color);           // Soft pulse in assigned color (OPEN state)
  void animateActiveSpin(const Rgb& color);     // Fast spinning animation
  void animateFlash(const Rgb& color);          // Quick flash animation
  void animateCelebration();                    // Rainbow animation
  void animateDisconnected();                   // Red pulse (no connection)
  void showLocked();                            // All white (buzzed)
  
  // Test functions
  void runColorTest();
};

// Global LED controller instance
extern ClientLEDController* clientLedController;

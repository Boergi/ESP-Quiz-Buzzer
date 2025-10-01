#pragma once
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"

// LED Controller class
class LEDController {
private:
  Adafruit_NeoPixel& strip;
  
public:
  LEDController(Adafruit_NeoPixel& ledStrip);
  
  // Basic LED functions
  void setPixelColor(uint16_t pixel, const Rgb& color);
  void clearAllLEDs();
  void showRGBTest();
  void showLEDs(); // Call strip.show()
  
  // Server-specific LED functions (18 LEDs)
  void setActivePlayerLEDs(const Rgb& color);
  void setQueueLED(uint8_t position, const Rgb& color);
  void clearQueueLEDs();
  void updateServerLEDs();
  
  // Animation functions
  void testQueueDisplay();
  void showConnectedClients();
  void animateReady();
  void animateOpen();
};

// Global LED controller instance
extern LEDController* ledController;

#pragma once
#include <Arduino.h>
#include <Bounce2.h>
#include "config.h"
#include "protocol.h"

// Button press detection
class ButtonHandler {
private:
  Bounce& button;
  uint32_t lastButtonPress;
  bool buttonHeld;
  
public:
  ButtonHandler(Bounce& btn);
  ButtonPress checkButtonPress();
};

// Game phase management
class GameManager {
private:
  uint32_t celebrationStart = 0;
  
public:
  void handleButtonPress(ButtonPress press);
  void handlePhase();
  void resetGame();
  void startQuestion();
  void nextClient();
  void correctAnswer();
  void resetToReady(); // Helper function
};

// Global instances
extern ButtonHandler* buttonHandler;
extern GameManager* gameManager;

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
  uint32_t lastPingTime = 0;
  
public:
  void handleButtonPress(ButtonPress press);
  void handlePhase();
  void resetGame();
  void startQuestion();
  void nextClient();
  void correctAnswer();
  void resetToReady(); // Helper function
  void handleClientJoin(const String& payload);
  void handleClientBuzz(const String& payload);
  void handleClientPing(const String& payload);
  void checkClientTimeouts();
  void publishAnnounce();
  void publishGameState();
  void publishBuzzQueue();
  void sendClientAssignment(const String& clientId, uint8_t slot, const Rgb& color);
  void sendPingToAllClients(); // New ping function
};

// Global instances
extern ButtonHandler* buttonHandler;
extern GameManager* gameManager;

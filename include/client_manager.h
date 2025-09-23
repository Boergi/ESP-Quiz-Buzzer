#pragma once
#include <Arduino.h>
#include <Bounce2.h>
#include "config.h"
#include "protocol.h"

// Client state management
class ClientManager {
public:
  struct ClientData {
    String id;
    uint8_t slot;
    Rgb assignedColor;
    ClientState currentState;
    bool hasBuzzed;
    bool isActive;
  };
  
private:
  ClientData data;
  
public:
  ClientManager();
  
  // State management
  void setState(ClientState newState);
  ClientState getState() const;
  void handleStateAnimations();
  
  // Assignment handling
  void setAssignment(uint8_t slot, const Rgb& color);
  bool isAssigned() const;
  
  // Buzz handling
  void buzz();
  bool canBuzz() const;
  void resetBuzzState();
  
  // Getters
  const ClientData& getData() const;
  const String& getClientId() const;
  const Rgb& getAssignedColor() const;
};

// Button handling for client
class ClientButtonHandler {
private:
  Bounce& button;
  uint32_t lastButtonPress;
  bool buttonPressed;
  
public:
  ClientButtonHandler(Bounce& btn);
  void update();
  bool wasPressed();
};

// Global instances
extern ClientManager* clientManager;
extern ClientButtonHandler* clientButtonHandler;

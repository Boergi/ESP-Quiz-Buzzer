#include "client_manager.h"
#include "client_led_controller.h"
#include "client_mqtt.h"

// Global instances
ClientManager* clientManager = nullptr;
ClientButtonHandler* clientButtonHandler = nullptr;

// ClientManager Implementation
ClientManager::ClientManager() {
  // Initialize with defaults
  data.currentState = ClientState::DISCONNECTED;
  data.slot = 0;
  data.assignedColor = Rgb(255, 0, 0); // Default red
  data.hasBuzzed = false;
  data.isActive = false;
  
  if (clientMqtt) {
    data.id = clientMqtt->getClientId();
  }
}

void ClientManager::setState(ClientState newState) {
  if (data.currentState != newState) {
    Serial.printf("State change: %d -> %d\n", (int)data.currentState, (int)newState);
    data.currentState = newState;
  }
}

ClientState ClientManager::getState() const {
  return data.currentState;
}

void ClientManager::handleStateAnimations() {
  if (!clientLedController) return;
  
  // Check if game is OPEN for different idle behavior
  extern bool gameIsOpen; // We'll need to track this
  
  switch (data.currentState) {
    case ClientState::DISCONNECTED:
      clientLedController->animateDisconnected();
      break;
      
    case ClientState::ASSIGNED:
      // Show assigned color briefly, then go to idle
      clientLedController->setAllLEDs(data.assignedColor);
      // Auto-transition after showing assignment
      static uint32_t assignmentStart = millis();
      if (millis() - assignmentStart > 2000) {
        setState(ClientState::IDLE);
        assignmentStart = millis() + 60000; // Don't repeat for a while
      }
      break;
      
    case ClientState::IDLE:
      if (isAssigned()) {
        // Different behavior based on game state
        if (gameIsOpen) {
          clientLedController->animateIdle(data.assignedColor); // Pulse when ready to buzz
        } else {
          clientLedController->showSolidColor(data.assignedColor); // Solid when waiting
        }
      } else {
        clientLedController->animateDisconnected();
      }
      break;
      
    case ClientState::LOCKED_AFTER_BUZZ:
      clientLedController->showLocked();
      break;
      
    case ClientState::ACTIVE_TURN:
      if (isAssigned()) {
        clientLedController->animateActiveSpin(data.assignedColor);
      }
      break;
      
    case ClientState::CELEBRATE:
      clientLedController->animateCelebration();
      // Auto-return to idle after celebration
      static uint32_t celebrationStart = 0;
      if (celebrationStart == 0) {
        celebrationStart = millis();
      }
      if (millis() - celebrationStart > CELEBRATION_DURATION_MS) {
        setState(ClientState::IDLE);
        celebrationStart = 0; // Reset for next time
      }
      break;
      
    case ClientState::WRONG_FLASH:
      clientLedController->animateFlash(COLOR_ERROR);
      // Auto-return to idle after flash
      static uint32_t flashStart = 0;
      if (flashStart == 0) {
        flashStart = millis();
      }
      if (millis() - flashStart > FLASH_DURATION_MS * 4) {
        // Reset buzz state so client can buzz again
        resetBuzzState();
        setState(ClientState::IDLE);
        flashStart = 0; // Reset for next time
        Serial.println("WRONG_FLASH complete - can buzz again");
      }
      break;
      
    default:
      clientLedController->clearAllLEDs();
      break;
  }
}

void ClientManager::setAssignment(uint8_t slot, const Rgb& color) {
  data.slot = slot;
  data.assignedColor = color;
  setState(ClientState::ASSIGNED);
  
  Serial.printf("Assigned slot %d, color R:%d G:%d B:%d\n", 
                slot, color.r, color.g, color.b);
}

bool ClientManager::isAssigned() const {
  return data.slot > 0;
}

void ClientManager::buzz() {
  if (!canBuzz()) return;
  
  data.hasBuzzed = true;
  setState(ClientState::LOCKED_AFTER_BUZZ);
  
  // Send buzz via MQTT
  if (clientMqtt && clientMqtt->isConnected()) {
    clientMqtt->sendBuzz();
  }
  
  Serial.println("BUZZED!");
}

bool ClientManager::canBuzz() const {
  return isAssigned() && 
         !data.hasBuzzed && 
         data.currentState == ClientState::IDLE &&
         clientMqtt && 
         clientMqtt->isConnected();
}

void ClientManager::resetBuzzState() {
  data.hasBuzzed = false;
  data.isActive = false;
}

const ClientManager::ClientData& ClientManager::getData() const {
  return data;
}

const String& ClientManager::getClientId() const {
  return data.id;
}

const Rgb& ClientManager::getAssignedColor() const {
  return data.assignedColor;
}

// ClientButtonHandler Implementation
ClientButtonHandler::ClientButtonHandler(Bounce& btn) : button(btn), lastButtonPress(0), buttonPressed(false) {
}

void ClientButtonHandler::update() {
  button.update();
  
  if (button.fell() && !buttonPressed) {
    buttonPressed = true;
    lastButtonPress = millis();
  }
}

bool ClientButtonHandler::wasPressed() {
  if (buttonPressed) {
    buttonPressed = false; // Reset flag
    return true;
  }
  return false;
}

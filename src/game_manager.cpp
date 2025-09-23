#include "game_manager.h"
#include "mqtt_server.h"
#include "led_controller.h"
#include <ArduinoJson.h>

// Global instances
ButtonHandler* buttonHandler = nullptr;
GameManager* gameManager = nullptr;

// ButtonHandler Implementation
ButtonHandler::ButtonHandler(Bounce& btn) : button(btn), lastButtonPress(0), buttonHeld(false) {
}

ButtonPress ButtonHandler::checkButtonPress() {
  button.update();
  
  // Button pressed
  if (button.fell()) {
    lastButtonPress = millis();
    buttonHeld = false;
    return ButtonPress::NONE;
  }
  
  // Button held - check for long press
  if (button.read() == LOW && lastButtonPress && !buttonHeld) {
    uint32_t holdTime = millis() - lastButtonPress;
    
    if (holdTime >= VERY_LONG_PRESS_MS) {
      buttonHeld = true;
      return ButtonPress::VERY_LONG;
    } else if (holdTime >= LONG_PRESS_MS) {
      buttonHeld = true;
      return ButtonPress::LONG;
    }
  }
  
  // Button released
  if (button.rose()) {
    if (!buttonHeld && lastButtonPress) {
      uint32_t pressTime = millis() - lastButtonPress;
      lastButtonPress = 0;
      buttonHeld = false;
      
      if (pressTime < SHORT_PRESS_MAX_MS) {
        return ButtonPress::SHORT;
      }
    }
    lastButtonPress = 0;
    buttonHeld = false;
  }
  
  return ButtonPress::NONE;
}

// GameManager Implementation
void GameManager::handleButtonPress(ButtonPress press) {
  switch(press) {
    case ButtonPress::SHORT:
      Serial.println("SHORT press detected");
      if (currentPhase == Phase::LOBBY && gameClientCount >= MIN_CLIENTS_TO_START) {
        currentPhase = Phase::READY;
        gameLocked = true;
        Serial.println("=== PHASE: READY ===");
        publishGameState();
      } else if (currentPhase == Phase::READY) {
        startQuestion();
      } else if (currentPhase == Phase::ANSWER) {
        nextClient();
      }
      break;
      
    case ButtonPress::LONG:
      Serial.println("LONG press detected");
      if (currentPhase == Phase::ANSWER) {
        correctAnswer();
      } else {
        resetGame();
      }
      break;
      
    case ButtonPress::VERY_LONG:
      Serial.println("VERY LONG press detected - UNLOCK GAME");
      gameLocked = false;
      publishGameState();
      break;
      
    default:
      break;
  }
}

void GameManager::handlePhase() {
  if (!ledController) return;
  
  switch(currentPhase) {
    case Phase::BOOT:
      // Initialize and test LEDs
      ledController->testColorCycle();
      
      // Auto-transition to LOBBY after 15 seconds
      static uint32_t bootStart = millis();
      if (millis() - bootStart > 15000) {
        currentPhase = Phase::LOBBY;
        ledController->clearAllLEDs();
        Serial.println("=== PHASE: LOBBY ===");
        publishGameState();
      }
      break;
      
    case Phase::LOBBY:
      // Show connected clients or test queue display
      if (gameClientCount > 0) {
        ledController->showConnectedClients();
      } else {
        ledController->testQueueDisplay();
      }
      break;
      
    case Phase::READY:
      ledController->animateReady();
      break;
      
    case Phase::OPEN:
      ledController->animateOpen();
      break;
      
    case Phase::ANSWER:
      // Show active client and queue
      ledController->updateServerLEDs();
      break;
      
    case Phase::RESET:
      // Celebration phase - show rainbow animation on server too
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
          
          ledController->setPixelColor(i, rainbowColor);
        }
        ledController->showLEDs();
        animStep += 5;
        lastUpdate = millis();
      }
      
      // Wait for animation to complete
      if (celebrationStart > 0 && millis() - celebrationStart > CELEBRATION_DURATION_MS) {
        celebrationStart = 0;
        resetToReady();
      }
      break;
      
    default:
      ledController->clearAllLEDs();
      break;
  }
}

void GameManager::resetGame() {
  currentPhase = Phase::LOBBY;
  gameLocked = false;
  
  // Clear buzz queue
  memset(buzzQueue, 0, sizeof(buzzQueue));
  queueLength = 0;
  activeClientIndex = -1;
  
  // Reset client buzz states
  for (uint8_t i = 0; i < gameClientCount; i++) {
    gameClients[i].buzzed = false;
  }
  
  if (ledController) {
    ledController->clearAllLEDs();
  }
  
  Serial.println("=== RESET to LOBBY ===");
  publishGameState();
}

void GameManager::startQuestion() {
  currentPhase = Phase::OPEN;
  Serial.println("=== PHASE: OPEN ===");
  publishGameState();
}

void GameManager::nextClient() {
  // Next client in queue
  if (activeClientIndex + 1 < queueLength) {
    activeClientIndex++;
    if (ledController) {
      ledController->updateServerLEDs();
    }
    Serial.printf("Next client active: %s\n", buzzQueue[activeClientIndex].c_str());
  }
}

void GameManager::correctAnswer() {
  // Send celebration command to active client
  if (activeClientIndex >= 0 && activeClientIndex < queueLength) {
    String activeClientId = buzzQueue[activeClientIndex];
    
    // Send celebrate command via MQTT
    StaticJsonDocument<200> doc;
    doc[JsonKey::CMD] = Command::CELEBRATE;
    doc[JsonKey::TARGET] = activeClientId;
    
    String message;
    serializeJson(doc, message);
    
    mqttBroker.publish(Topic::CMD, message.c_str());
    Serial.printf("Sent celebrate command to %s\n", activeClientId.c_str());
    
    // Set celebration phase with delay
    currentPhase = Phase::RESET;
    celebrationStart = millis();
    Serial.println("=== CELEBRATION - waiting for animation ===");
    return; // Don't reset immediately
  }
  
  // If no active client, reset immediately
  resetToReady();
}

void GameManager::resetToReady() {
  // Correct answer - reset for next question
  memset(buzzQueue, 0, sizeof(buzzQueue));
  queueLength = 0;
  activeClientIndex = -1;
  
  // Reset client buzz states
  for (uint8_t i = 0; i < gameClientCount; i++) {
    gameClients[i].buzzed = false;
  }
  
  currentPhase = Phase::READY;
  if (ledController) {
    ledController->clearAllLEDs();
  }
  Serial.println("=== CORRECT ANSWER - READY for next question ===");
  publishGameState();
}

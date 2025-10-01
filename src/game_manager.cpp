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
  // Wrong answer - remove current client from queue and reset them
  if (activeClientIndex >= 0 && activeClientIndex < queueLength) {
    String wrongClientId = buzzQueue[activeClientIndex];
    
    // Send WRONG_FLASH command to current client
    StaticJsonDocument<200> doc;
    doc[JsonKey::CMD] = Command::WRONG_FLASH;
    doc[JsonKey::TARGET] = wrongClientId;
    
    String message;
    serializeJson(doc, message);
    mqttBroker.publish(Topic::CMD, message.c_str());
    Serial.printf("Sent WRONG_FLASH to %s\n", wrongClientId.c_str());
    
    // Also send RESET command after a short delay to ensure client can buzz again
    delay(100); // Small delay to ensure WRONG_FLASH is processed first
    StaticJsonDocument<200> resetDoc;
    resetDoc[JsonKey::CMD] = Command::RESET;
    resetDoc[JsonKey::TARGET] = wrongClientId;
    
    String resetMessage;
    serializeJson(resetDoc, resetMessage);
    mqttBroker.publish(Topic::CMD, resetMessage.c_str());
    Serial.printf("Sent RESET to %s\n", wrongClientId.c_str());
    
    // Reset client's buzzed state (allow them to buzz again)
    for (uint8_t i = 0; i < gameClientCount; i++) {
      if (gameClients[i].id == wrongClientId) {
        gameClients[i].buzzed = false;
        Serial.printf("Reset %s - can buzz again\n", wrongClientId.c_str());
        break;
      }
    }
    
    // Remove client from queue (shift all following clients forward)
    for (uint8_t i = activeClientIndex; i < queueLength - 1; i++) {
      buzzQueue[i] = buzzQueue[i + 1];
    }
    queueLength--;
    
    // activeClientIndex stays the same (next client is now at same index)
    // Check if there's still a client at current index
    if (activeClientIndex < queueLength) {
      String nextClientId = buzzQueue[activeClientIndex];
      
      // Send ANIM_ACTIVE command to next client
      StaticJsonDocument<200> nextDoc;
      nextDoc[JsonKey::CMD] = Command::ANIM_ACTIVE;
      nextDoc[JsonKey::TARGET] = nextClientId;
      
      String nextMessage;
      serializeJson(nextDoc, nextMessage);
      mqttBroker.publish(Topic::CMD, nextMessage.c_str());
      Serial.printf("Sent ANIM_ACTIVE to next client: %s\n", nextClientId.c_str());
      
      // Update server LEDs
      if (ledController) {
        ledController->updateServerLEDs();
      }
      
      // Publish updated queue
      publishBuzzQueue();
    } else {
      // No more clients in queue - back to OPEN phase
      activeClientIndex = -1;
      currentPhase = Phase::OPEN;
      Serial.println("=== No more clients in queue - back to OPEN ===");
      publishGameState();
      if (ledController) {
        ledController->clearAllLEDs();
      }
    }
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

void GameManager::sendPingToAllClients() {
  if (millis() - lastPingTime < PING_INTERVAL_MS) {
    return; // Not time for ping yet
  }
  
  lastPingTime = millis();
  
  // Send ping request to all connected clients
  StaticJsonDocument<100> doc;
  doc[JsonKey::CMD] = "PING_REQUEST";
  doc[JsonKey::TIMESTAMP] = millis();
  
  String message;
  serializeJson(doc, message);
  
  mqttBroker.publish(Topic::CMD, message.c_str());
  Serial.printf("Sent ping to all clients (count: %d)\n", gameClientCount);
}

void GameManager::publishGameState() {
  StaticJsonDocument<200> doc;
  doc[JsonKey::PHASE] = phaseToString(currentPhase);
  doc[JsonKey::LOCKED] = gameLocked;
  doc["gameClientCount"] = gameClientCount;
  
  String message;
  serializeJson(doc, message);
  
  mqttBroker.publish(Topic::STATE, message.c_str(), true); // retained
  Serial.printf("Published game state: %s\n", message.c_str());
}

void GameManager::publishBuzzQueue() {
  StaticJsonDocument<300> doc;
  JsonArray order = doc.createNestedArray(JsonKey::ORDER);
  
  for (uint8_t i = 0; i < queueLength; i++) {
    order.add(buzzQueue[i]);
  }
  
  if (activeClientIndex >= 0 && activeClientIndex < queueLength) {
    doc[JsonKey::ACTIVE] = buzzQueue[activeClientIndex];
  }
  
  String message;
  serializeJson(doc, message);
  
  mqttBroker.publish(Topic::QUEUE, message.c_str());
  Serial.printf("Published buzz queue: %s\n", message.c_str());
}

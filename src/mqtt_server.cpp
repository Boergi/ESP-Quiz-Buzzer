#include "mqtt_server.h"
#include "led_controller.h"
#include "game_manager.h"
#include <ArduinoJson.h>

// Global variables
QuizMQTTBroker mqttBroker;
ClientInfo gameClients[MAX_CLIENTS];
uint8_t gameClientCount = 0;
String buzzQueue[MAX_CLIENTS];
uint8_t queueLength = 0;

// Forward declaration
extern GameManager* gameManager;
int8_t activeClientIndex = -1;
Phase currentPhase = Phase::BOOT;
bool gameLocked = false;

// MQTT Broker Implementation
void QuizMQTTBroker::on_connected(const char * client_id) {
  Serial.printf("MQTT Client connected: %s\n", client_id);
}

void QuizMQTTBroker::on_disconnected(const char * client_id) {
  Serial.printf("MQTT Client disconnected: %s\n", client_id);
  // Note: Game client cleanup handled in main loop via timeouts
}

// MQTT Message Handlers
void handleClientJoin(const String& payload) {
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, payload);
  
  if (error) {
    Serial.printf("JSON parse error: %s\n", error.c_str());
    return;
  }
  
  String clientId = doc[JsonKey::ID];
  uint8_t capability = doc[JsonKey::CAPABILITY] | 8;  // default 8 if not provided
  String firmware = doc[JsonKey::FIRMWARE] | "1.0";  // default if not provided
  
  Serial.printf("Client join request: %s (cap: %d, fw: %s) - Phase: %s\n", 
                clientId.c_str(), capability, firmware.c_str(), phaseToString(currentPhase));
  
  // ====== RECONNECT LOGIC - ALWAYS ALLOW KNOWN CLIENTS ======
  // Check if client already exists (reconnect scenario)
  for (uint8_t i = 0; i < gameClientCount; i++) {
    if (gameClients[i].id == clientId) {
      gameClients[i].connected = true;
      gameClients[i].lastSeen = millis();
      Serial.printf("✓ Client %s RECONNECTED (slot %d)\n", clientId.c_str(), gameClients[i].slot);
      
      // Send assignment to restore client state
      sendClientAssignment(clientId, gameClients[i].slot, gameClients[i].color);
      
      // Restore client state based on current game phase
      StaticJsonDocument<200> stateDoc;
      
      // If client was in buzz queue, restore their state
      bool wasInQueue = false;
      for (uint8_t q = 0; q < queueLength; q++) {
        if (buzzQueue[q] == clientId) {
          wasInQueue = true;
          Serial.printf("  → Client was in buzz queue at position %d\n", q);
          
          // If client is active, send ANIM_ACTIVE
          if (activeClientIndex == q) {
            stateDoc[JsonKey::CMD] = Command::ANIM_ACTIVE;
            stateDoc[JsonKey::TARGET] = clientId;
            String message;
            serializeJson(stateDoc, message);
            mqttBroker.publish(Topic::CMD, message.c_str());
            Serial.printf("  → Restored ACTIVE state for %s\n", clientId.c_str());
          } else {
            // Client is waiting in queue, show white light
            stateDoc[JsonKey::CMD] = Command::LIGHT_WHITE;
            stateDoc[JsonKey::TARGET] = clientId;
            String message;
            serializeJson(stateDoc, message);
            mqttBroker.publish(Topic::CMD, message.c_str());
            Serial.printf("  → Restored LOCKED state for %s (waiting in queue)\n", clientId.c_str());
          }
          break;
        }
      }
      
      // If not in queue, restore IDLE state (if in READY/OPEN phase)
      if (!wasInQueue && (currentPhase == Phase::READY || currentPhase == Phase::OPEN)) {
        stateDoc[JsonKey::CMD] = Command::IDLE_COLOR;
        stateDoc[JsonKey::TARGET] = clientId;
        String message;
        serializeJson(stateDoc, message);
        mqttBroker.publish(Topic::CMD, message.c_str());
        Serial.printf("  → Restored IDLE state for %s\n", clientId.c_str());
      }
      
      return; // Reconnect handled, exit function
    }
  }
  
  // ====== NEW CLIENT LOGIC - CHECK PHASE ======
  // Only allow new clients to join in LOBBY or READY phase
  if (currentPhase != Phase::LOBBY && currentPhase != Phase::READY) {
    Serial.printf("✗ New client %s rejected - game in phase %s (only LOBBY/READY allowed)\n", 
                  clientId.c_str(), phaseToString(currentPhase));
    return;
  }
  
  // Check if game is locked (for new clients)
  if (gameLocked && currentPhase == Phase::READY) {
    Serial.printf("✗ New client %s rejected - game locked in READY phase\n", clientId.c_str());
    return;
  }
  
  // Add new client
  if (gameClientCount < MAX_CLIENTS) {
    gameClients[gameClientCount].id = clientId;
    gameClients[gameClientCount].slot = gameClientCount + 1;
    gameClients[gameClientCount].color = PLAYER_COLORS[gameClientCount];
    gameClients[gameClientCount].connected = true;
    gameClients[gameClientCount].buzzed = false;
    gameClients[gameClientCount].lastSeen = millis();
    
    Serial.printf("✓ New client added: %s (slot %d, color R:%d G:%d B:%d)\n",
                  clientId.c_str(), gameClients[gameClientCount].slot,
                  gameClients[gameClientCount].color.r, gameClients[gameClientCount].color.g, gameClients[gameClientCount].color.b);
    
    sendClientAssignment(clientId, gameClients[gameClientCount].slot, gameClients[gameClientCount].color);
    gameClientCount++;
    
    gameManager->publishGameState();
  } else {
    Serial.printf("✗ Max clients reached, rejecting %s\n", clientId.c_str());
  }
}

void handleClientBuzz(const String& payload) {
  if (currentPhase != Phase::OPEN && currentPhase != Phase::ANSWER) {
    Serial.printf("Buzz ignored - game phase is %s (need OPEN or ANSWER)\n", phaseToString(currentPhase));
    return;
  }
  
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, payload);
  
  if (error) {
    Serial.printf("Buzz JSON parse error: %s\n", error.c_str());
    return;
  }
  
  String clientId = doc[JsonKey::ID];
  uint32_t timestamp = doc[JsonKey::TIMESTAMP] | millis(); // use current time if not provided
  
  // Check if client already buzzed
  for (uint8_t i = 0; i < gameClientCount; i++) {
    if (gameClients[i].id == clientId && gameClients[i].buzzed) {
      Serial.printf("Client %s already buzzed, ignoring\n", clientId.c_str());
      return;
    }
  }
  
  // Add to buzz queue
  if (queueLength < MAX_CLIENTS) {
    buzzQueue[queueLength] = clientId;
    queueLength++;
    
    // Mark client as buzzed
    for (uint8_t i = 0; i < gameClientCount; i++) {
      if (gameClients[i].id == clientId) {
        gameClients[i].buzzed = true;
        break;
      }
    }
    
    Serial.printf("BUZZ from %s (timestamp: %u), queue position: %d/%d\n", 
                  clientId.c_str(), timestamp, queueLength, MAX_CLIENTS);
    
    // First buzz? Switch to ANSWER phase and send ANIM_ACTIVE
    if (queueLength == 1) {
      currentPhase = Phase::ANSWER;
      activeClientIndex = 0;
      Serial.printf("=== FIRST BUZZ! %s is now ACTIVE (index %d) ===\n", clientId.c_str(), activeClientIndex);
      
      // Send ANIM_ACTIVE command to first client
      StaticJsonDocument<200> activeDoc;
      activeDoc[JsonKey::CMD] = Command::ANIM_ACTIVE;
      activeDoc[JsonKey::TARGET] = clientId;
      
      String activeMessage;
      serializeJson(activeDoc, activeMessage);
      mqttBroker.publish(Topic::CMD, activeMessage.c_str());
      Serial.printf("Sent ANIM_ACTIVE to first client: %s\n", clientId.c_str());
      
      gameManager->publishGameState();
    } else {
      Serial.printf("Additional buzz: %s added to queue (position %d)\n", clientId.c_str(), queueLength);
    }
    
    // Debug: Print current queue
    Serial.print("Current buzz queue: ");
    for (uint8_t i = 0; i < queueLength; i++) {
      Serial.printf("[%d]%s ", i, buzzQueue[i].c_str());
    }
    Serial.println();
    
    gameManager->publishBuzzQueue();
    if (ledController) {
      ledController->updateServerLEDs();
    }
  }
}

void handleClientPing(const String& payload) {
  StaticJsonDocument<100> doc;
  DeserializationError error = deserializeJson(doc, payload);
  
  if (error) return;
  
  String clientId = doc[JsonKey::ID];
  
  // Update last seen timestamp
  for (uint8_t i = 0; i < gameClientCount; i++) {
    if (gameClients[i].id == clientId) {
      gameClients[i].lastSeen = millis();
      break;
    }
  }
}

// MQTT Publishers
void sendClientAssignment(const String& clientId, uint8_t slot, const Rgb& color) {
  StaticJsonDocument<200> doc;
  doc[JsonKey::SLOT] = slot;
  char colorHex[8];
  sprintf(colorHex, "#%02X%02X%02X", color.r, color.g, color.b);
  doc[JsonKey::COLOR] = colorHex;
  
  String message;
  serializeJson(doc, message);
  
  String topic = String(Topic::ASSIGN) + clientId;
  mqttBroker.publish(topic.c_str(), message.c_str(), true); // retained
  
  Serial.printf("Sent assignment to %s: slot %d, color %s\n", 
                clientId.c_str(), slot, colorHex);
}

// Functions moved to GameManager class

void publishAnnounce() {
  StaticJsonDocument<200> doc;
  doc[JsonKey::VERSION] = PROTOCOL_VERSION;
  doc[JsonKey::MAX_CLIENTS] = MAX_CLIENTS;
  doc[JsonKey::LOCKED] = gameLocked;
  
  String message;
  serializeJson(doc, message);
  
  mqttBroker.publish(Topic::ANNOUNCE, message.c_str(), true); // retained
  Serial.printf("Published announce: %s\n", message.c_str());
}

void checkClientTimeouts() {
  uint32_t now = millis();
  for (uint8_t i = 0; i < gameClientCount; i++) {
    if (gameClients[i].connected && (now - gameClients[i].lastSeen > CLIENT_TIMEOUT_MS)) {
      gameClients[i].connected = false;
      Serial.printf("⚠ Client %s timed out (no ping for %d ms)\n", 
                    gameClients[i].id.c_str(), CLIENT_TIMEOUT_MS);
      
      // Note: Client stays in gameClients array and can reconnect at any time
      // Their slot and color are preserved for seamless reconnection
    }
  }
}

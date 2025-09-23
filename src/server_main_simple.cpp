#define SERVER 1
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Adafruit_NeoPixel.h>
#include <Bounce2.h>
#include <ArduinoJson.h>
#include "config.h"
#include "protocol.h"

// Hardware Objects
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
Bounce button = Bounce();

// UDP Communication
WiFiUDP udp;
constexpr uint16_t UDP_PORT = 12345;

// Game State
struct ClientInfo {
  String id;
  uint8_t slot;
  Rgb color;
  bool connected;
  uint32_t lastSeen;
};

ClientInfo clients[MAX_CLIENTS];
uint8_t clientCount = 0;
Phase currentPhase = Phase::BOOT;
bool gameLocked = false;
uint32_t lastButtonPress = 0;
bool buttonHeld = false;

// LED Helper Functions
void setPixelColor(uint16_t pixel, const Rgb& color) {
  if (pixel < LED_COUNT) {
    strip.setPixelColor(pixel, strip.Color(color.r, color.g, color.b));
  }
}

void clearAllLEDs() {
  strip.clear();
  strip.show();
}

void setActivePlayerLEDs(const Rgb& color) {
  for (uint8_t i = 0; i < 8; i++) {
    setPixelColor(i, color);
  }
  strip.show();
}

void setQueueLED(uint8_t position, const Rgb& color) {
  if (position < 10) {
    setPixelColor(8 + position, color);
    strip.show();
  }
}

// UDP Communication
void handleUdpMessage() {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    char buffer[512];
    int len = udp.read(buffer, 511);
    buffer[len] = 0;
    
    IPAddress remoteIP = udp.remoteIP();
    uint16_t remotePort = udp.remotePort();
    
    Serial.printf("UDP received from %s:%d - %s\n", remoteIP.toString().c_str(), remotePort, buffer);
    
    // Parse JSON message
    StaticJsonDocument<300> doc;
    DeserializationError error = deserializeJson(doc, buffer);
    
    if (error) {
      Serial.printf("JSON parse error: %s\n", error.c_str());
      return;
    }
    
    String msgType = doc["type"];
    String clientId = doc["id"];
    
    if (msgType == "join") {
      handleClientJoin(clientId, remoteIP, remotePort);
    } else if (msgType == "buzz") {
      handleClientBuzz(clientId);
    } else if (msgType == "ping") {
      handleClientPing(clientId);
    }
  }
}

void handleClientJoin(const String& clientId, IPAddress ip, uint16_t port) {
  Serial.printf("Client join request: %s from %s:%d\n", clientId.c_str(), ip.toString().c_str(), port);
  
  if (gameLocked) {
    Serial.println("Game locked, rejecting client");
    sendResponse(ip, port, "join_rejected", "Game is locked");
    return;
  }
  
  // Check if client already exists
  for (uint8_t i = 0; i < clientCount; i++) {
    if (clients[i].id == clientId) {
      clients[i].connected = true;
      clients[i].lastSeen = millis();
      sendClientAssignment(ip, port, clientId, clients[i].slot, clients[i].color);
      return;
    }
  }
  
  // Add new client
  if (clientCount < MAX_CLIENTS) {
    clients[clientCount].id = clientId;
    clients[clientCount].slot = clientCount + 1;
    clients[clientCount].color = PLAYER_COLORS[clientCount];
    clients[clientCount].connected = true;
    clients[clientCount].lastSeen = millis();
    
    Serial.printf("New client added: %s (slot %d, color R:%d G:%d B:%d)\n",
                  clientId.c_str(), clients[clientCount].slot,
                  clients[clientCount].color.r, clients[clientCount].color.g, clients[clientCount].color.b);
    
    sendClientAssignment(ip, port, clientId, clients[clientCount].slot, clients[clientCount].color);
    clientCount++;
    
    broadcastGameState();
  } else {
    Serial.println("Max clients reached");
    sendResponse(ip, port, "join_rejected", "Server full");
  }
}

void sendClientAssignment(IPAddress ip, uint16_t port, const String& clientId, uint8_t slot, const Rgb& color) {
  StaticJsonDocument<200> doc;
  doc["type"] = "assignment";
  doc["slot"] = slot;
  char colorHex[8];
  sprintf(colorHex, "#%02X%02X%02X", color.r, color.g, color.b);
  doc["color"] = colorHex;
  
  String message;
  serializeJson(doc, message);
  
  sendUdpMessage(ip, port, message);
  Serial.printf("Sent assignment to %s: slot %d, color %s\n", clientId.c_str(), slot, colorHex);
}

void handleClientBuzz(const String& clientId) {
  if (currentPhase != Phase::OPEN) {
    Serial.println("Buzz ignored - game not open");
    return;
  }
  
  Serial.printf("BUZZ from client: %s\n", clientId.c_str());
  
  // Find client color and show on LEDs
  for (uint8_t i = 0; i < clientCount; i++) {
    if (clients[i].id == clientId) {
      setActivePlayerLEDs(clients[i].color);
      currentPhase = Phase::ANSWER;
      Serial.println("=== PHASE: ANSWER ===");
      broadcastGameState();
      break;
    }
  }
}

void handleClientPing(const String& clientId) {
  // Update last seen timestamp
  for (uint8_t i = 0; i < clientCount; i++) {
    if (clients[i].id == clientId) {
      clients[i].lastSeen = millis();
      break;
    }
  }
}

void sendResponse(IPAddress ip, uint16_t port, const String& type, const String& message) {
  StaticJsonDocument<200> doc;
  doc["type"] = type;
  doc["message"] = message;
  
  String response;
  serializeJson(doc, response);
  
  sendUdpMessage(ip, port, response);
}

void sendUdpMessage(IPAddress ip, uint16_t port, const String& message) {
  udp.beginPacket(ip, port);
  udp.print(message);
  udp.endPacket();
}

void broadcastGameState() {
  StaticJsonDocument<200> doc;
  doc["type"] = "state";
  doc["phase"] = phaseToString(currentPhase);
  doc["locked"] = gameLocked;
  doc["clients"] = clientCount;
  
  String message;
  serializeJson(doc, message);
  
  // Broadcast to all connected clients (would need client IP storage for real implementation)
  Serial.printf("Broadcast state: %s\n", message.c_str());
}

// Button Handling
ButtonPress checkButtonPress() {
  button.update();
  
  if (button.fell()) {
    lastButtonPress = millis();
    buttonHeld = false;
    return ButtonPress::NONE;
  }
  
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

void handleButtonPress(ButtonPress press) {
  switch(press) {
    case ButtonPress::SHORT:
      Serial.println("SHORT press detected");
      if (currentPhase == Phase::LOBBY && clientCount >= MIN_CLIENTS_TO_START) {
        currentPhase = Phase::READY;
        gameLocked = true;
        Serial.println("=== PHASE: READY ===");
        broadcastGameState();
      } else if (currentPhase == Phase::READY) {
        currentPhase = Phase::OPEN;
        Serial.println("=== PHASE: OPEN ===");
        broadcastGameState();
      } else if (currentPhase == Phase::ANSWER) {
        currentPhase = Phase::READY;
        clearAllLEDs();
        Serial.println("=== NEXT QUESTION ===");
        broadcastGameState();
      }
      break;
      
    case ButtonPress::LONG:
      Serial.println("LONG press detected");
      currentPhase = Phase::LOBBY;
      gameLocked = false;
      clearAllLEDs();
      Serial.println("=== RESET to LOBBY ===");
      broadcastGameState();
      break;
      
    case ButtonPress::VERY_LONG:
      Serial.println("VERY LONG press detected - UNLOCK GAME");
      gameLocked = false;
      broadcastGameState();
      break;
      
    default:
      break;
  }
}

// Demo Animations (simplified)
void handlePhase() {
  static uint32_t lastUpdate = 0;
  
  switch(currentPhase) {
    case Phase::BOOT:
      if (millis() - lastUpdate > 1000) {
        static uint8_t colorIndex = 0;
        clearAllLEDs();
        setActivePlayerLEDs(PLAYER_COLORS[colorIndex]);
        colorIndex = (colorIndex + 1) % MAX_CLIENTS;
        lastUpdate = millis();
        
        // Auto-transition to LOBBY after 10 seconds
        static uint32_t bootStart = millis();
        if (millis() - bootStart > 10000) {
          currentPhase = Phase::LOBBY;
          clearAllLEDs();
          Serial.println("=== PHASE: LOBBY ===");
          broadcastGameState();
        }
      }
      break;
      
    case Phase::LOBBY:
      // Show client count on first LEDs
      if (millis() - lastUpdate > 500) {
        clearAllLEDs();
        for (uint8_t i = 0; i < clientCount && i < 8; i++) {
          setPixelColor(i, clients[i].color);
        }
        strip.show();
        lastUpdate = millis();
      }
      break;
      
    case Phase::READY:
      // Breathing white
      if (millis() - lastUpdate > 50) {
        float breath = (sin(millis() * 0.005) + 1.0) * 0.5;
        uint8_t brightness = (uint8_t)(breath * 255);
        Rgb breathColor(brightness, brightness, brightness);
        
        for (uint8_t i = 8; i < 18; i++) {
          setPixelColor(i, breathColor);
        }
        strip.show();
        lastUpdate = millis();
      }
      break;
      
    case Phase::OPEN:
      // Green breathing
      if (millis() - lastUpdate > 50) {
        float breath = (sin(millis() * 0.008) + 1.0) * 0.5;
        uint8_t brightness = (uint8_t)(breath * 255);
        Rgb breathColor(0, brightness, 0);
        
        setActivePlayerLEDs(breathColor);
        lastUpdate = millis();
      }
      break;
      
    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Quiz-Buzzer Server Starting...");
  Serial.printf("Hardware Config - LED Pin: %d, Button Pin: %d, LED Count: %d\n", 
                LED_PIN, BUTTON_PIN, LED_COUNT);
  
  // Initialize LEDs
  strip.begin();
  strip.setBrightness(LED_BRIGHTNESS);
  strip.clear();
  strip.show();
  
  // Initialize Button
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  button.attach(BUTTON_PIN);
  button.interval(DEBOUNCE_MS);
  
  // Initialize WiFi Access Point
  Serial.println("Setting up WiFi Access Point...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_SSID, WIFI_PSK);
  WiFi.softAPConfig(IPAddress(AP_IP_ADDR), IPAddress(AP_GATEWAY_ADDR), IPAddress(AP_SUBNET_ADDR));
  
  Serial.printf("AP SSID: %s\n", WIFI_SSID);
  Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
  
  // Initialize UDP
  udp.begin(UDP_PORT);
  Serial.printf("UDP Server started on port %d\n", UDP_PORT);
  
  // Test sequence
  Serial.println("Starting LED test sequence...");
  for (uint16_t i = 0; i < LED_COUNT; i++) {
    setPixelColor(i, COLOR_WHITE);
  }
  strip.show();
  delay(1000);
  clearAllLEDs();
  
  Serial.println("=== PHASE: BOOT ===");
  Serial.println("Server ready for client connections!");
}

void loop() {
  // Handle UDP messages
  handleUdpMessage();
  
  // Handle button presses
  ButtonPress press = checkButtonPress();
  if (press != ButtonPress::NONE) {
    handleButtonPress(press);
  }
  
  // Handle game phases
  handlePhase();
  
  delay(10);
}

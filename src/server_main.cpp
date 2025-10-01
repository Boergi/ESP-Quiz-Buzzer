#define SERVER 1
#include <Arduino.h>
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
#include <Bounce2.h>
#include "config.h"
#include "protocol.h"
#include "mqtt_server.h"
#include "led_controller.h"
#include "game_manager.h"

// Hardware Objects
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
Bounce button = Bounce();

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
  
  // Initialize LED Controller
  ledController = new LEDController(strip);
  
  // Initialize Button
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  button.attach(BUTTON_PIN);
  button.interval(DEBOUNCE_MS);
  
  // Initialize Button Handler
  buttonHandler = new ButtonHandler(button);
  
  // Initialize Game Manager
  gameManager = new GameManager();
  
  // Initialize WiFi Access Point
  Serial.println("Setting up WiFi Access Point...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_SSID, WIFI_PSK, 1, 0, MAX_CLIENTS); // channel=1, hidden=false, max_clients=10
  WiFi.softAPConfig(IPAddress(AP_IP_ADDR), IPAddress(AP_GATEWAY_ADDR), IPAddress(AP_SUBNET_ADDR));
  
  Serial.printf("AP SSID: %s\n", WIFI_SSID);
  Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
  
  // Initialize MQTT Broker
  Serial.println("Starting PicoMQTT Broker...");
  
  // Setup MQTT message handlers
  mqttBroker.subscribe(Topic::JOIN, [](const char * payload) {
    handleClientJoin(String(payload));
  });
  
  mqttBroker.subscribe(Topic::BUZZ, [](const char * payload) {
    handleClientBuzz(String(payload));
  });
  
  mqttBroker.subscribe(Topic::PING, [](const char * payload) {
    handleClientPing(String(payload));
  });
  
  mqttBroker.begin();
  
  Serial.printf("MQTT Broker running on port %d\n", MQTT_PORT);
  
  // Publish initial announcements
  publishAnnounce();
  gameManager->publishGameState();
  
  // LED test sequence
  Serial.println("Starting LED test sequence...");
  ledController->showWhiteTest();
  delay(1000);
  ledController->clearAllLEDs();
  
  Serial.println("=== PHASE: BOOT ===");
  Serial.println("Phase Controls:");
  Serial.println("- SHORT press: LOBBY -> READY -> OPEN -> NEXT");
  Serial.println("- LONG press: Correct Answer / Reset");
  Serial.println("- VERY LONG press: Unlock game");
  Serial.println("Server ready for client connections!");
}

void loop() {
  // Handle MQTT broker
  mqttBroker.loop();
  
  // Handle button presses
  if (buttonHandler) {
    ButtonPress press = buttonHandler->checkButtonPress();
    if (press != ButtonPress::NONE && gameManager) {
      gameManager->handleButtonPress(press);
    }
  }
  
  // Handle game phases
  if (gameManager) {
    gameManager->handlePhase();
    gameManager->sendPingToAllClients(); // Background ping system
  }
  
  // Check for client timeouts (every 5 seconds)
  static uint32_t lastClientCheck = 0;
  if (millis() - lastClientCheck > 5000) {
    checkClientTimeouts();
    lastClientCheck = millis();
  }
  
  delay(10); // Small delay for stability
}
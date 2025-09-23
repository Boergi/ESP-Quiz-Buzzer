#define CLIENT 1
#include <Arduino.h>
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
#include <Bounce2.h>
#include "config.h"
#include "protocol.h"
#include "client_led_controller.h"
#include "client_mqtt.h"
#include "client_manager.h"

// Hardware Objects
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
Bounce button = Bounce();

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Quiz-Buzzer Client Starting...");
  Serial.printf("Hardware Config - LED Pin: %d, Button Pin: %d, LED Count: %d\n", 
                LED_PIN, BUTTON_PIN, LED_COUNT);
  
  // Initialize LEDs
  strip.begin();
  strip.setBrightness(LED_BRIGHTNESS);
  strip.clear();
  strip.show();
  
  // Initialize LED Controller
  clientLedController = new ClientLEDController(strip);
  
  // Initialize Button
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  button.attach(BUTTON_PIN);
  button.interval(DEBOUNCE_MS);
  
  // Initialize Button Handler
  clientButtonHandler = new ClientButtonHandler(button);
  
  // Initialize MQTT Client
  clientMqtt = new ClientMQTT();
  
  // Initialize Client Manager
  clientManager = new ClientManager();
  
  // LED test sequence
  Serial.println("Starting LED test...");
  clientLedController->showWhiteTest();
  delay(500);
  clientLedController->clearAllLEDs();
  
  // Color test cycle
  clientLedController->runColorTest();
  
  // Start MQTT connection
  clientMqtt->begin();
  
  Serial.println("Client ready!");
  Serial.println("Button Controls:");
  Serial.println("- Press: Buzz (when connected and game is open)");
  Serial.println("- System automatically handles connection and state changes");
}

void loop() {
  // Handle MQTT communication
  if (clientMqtt) {
    clientMqtt->loop();
    
    // Update client manager state based on connection
    if (clientManager) {
      if (clientMqtt->isConnected()) {
        // Connected - handle normal states
        if (clientManager->getState() == ClientState::DISCONNECTED) {
          if (clientManager->isAssigned()) {
            clientManager->setState(ClientState::IDLE);
          } else {
            clientManager->setState(ClientState::CONNECTING);
          }
        }
      } else {
        // Disconnected
        if (clientManager->getState() != ClientState::DISCONNECTED) {
          clientManager->setState(ClientState::DISCONNECTED);
        }
      }
    }
  }
  
  // Handle button presses
  if (clientButtonHandler) {
    clientButtonHandler->update();
    
    if (clientButtonHandler->wasPressed()) {
      Serial.println("Button pressed!");
      
      if (clientManager) {
        if (clientManager->canBuzz()) {
          clientManager->buzz();
        } else {
          Serial.println("Cannot buzz right now");
          
          // Debug info
          Serial.printf("State: %d, Assigned: %s, Buzzed: %s, Connected: %s\n",
                        (int)clientManager->getState(),
                        clientManager->isAssigned() ? "yes" : "no",
                        clientManager->getData().hasBuzzed ? "yes" : "no",
                        (clientMqtt && clientMqtt->isConnected()) ? "yes" : "no");
        }
      }
    }
  }
  
  // Handle state animations
  if (clientManager) {
    clientManager->handleStateAnimations();
  }
  
  delay(10); // Small delay for stability
}
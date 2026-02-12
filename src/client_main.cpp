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

namespace {
struct BatteryPoint {
  float voltage;
  uint8_t percent;
};

constexpr uint8_t BATTERY_ADC_PIN = 34;
constexpr float BATTERY_DIVIDER_R1_OHM = 100000.0f; // BAT+ -> ADC
constexpr float BATTERY_DIVIDER_R2_OHM = 100000.0f; // ADC -> GND
constexpr uint8_t BATTERY_SAMPLE_COUNT = 16;
constexpr uint16_t BATTERY_DISPLAY_MS = 3000;

constexpr BatteryPoint BATTERY_TABLE[] = {
  {4.20f, 100}, {4.10f, 90}, {4.00f, 80}, {3.92f, 70}, {3.85f, 60},
  {3.79f, 50},  {3.73f, 40}, {3.68f, 30}, {3.60f, 20}, {3.45f, 10}, {3.30f, 0}
};

bool batteryDisplayActive = false;
uint32_t batteryDisplayUntil = 0;

float readBatteryVoltage() {
  uint32_t sumMilliVolts = 0;
  for (uint8_t i = 0; i < BATTERY_SAMPLE_COUNT; i++) {
    sumMilliVolts += analogReadMilliVolts(BATTERY_ADC_PIN);
    delay(2);
  }

  const float adcVolts = (sumMilliVolts / static_cast<float>(BATTERY_SAMPLE_COUNT)) / 1000.0f;
  return adcVolts * ((BATTERY_DIVIDER_R1_OHM + BATTERY_DIVIDER_R2_OHM) / BATTERY_DIVIDER_R2_OHM);
}

uint8_t batteryPercentFromVoltage(float vbat) {
  if (vbat >= BATTERY_TABLE[0].voltage) return 100;
  if (vbat <= BATTERY_TABLE[10].voltage) return 0;

  for (uint8_t i = 0; i < 10; i++) {
    const BatteryPoint high = BATTERY_TABLE[i];
    const BatteryPoint low = BATTERY_TABLE[i + 1];
    if (vbat <= high.voltage && vbat >= low.voltage) {
      const float t = (vbat - low.voltage) / (high.voltage - low.voltage);
      return static_cast<uint8_t>(low.percent + t * (high.percent - low.percent));
    }
  }
  return 0;
}

Rgb batteryColor(uint8_t percent) {
  if (percent < 20) return Rgb(255, 0, 0);
  if (percent < 50) return Rgb(255, 180, 0);
  return Rgb(0, 255, 0);
}

void renderBatteryRing(uint8_t percent) {
  const Rgb base = batteryColor(percent);
  const float ledUnits = (percent / 100.0f) * LED_COUNT;
  const uint8_t fullLeds = static_cast<uint8_t>(ledUnits);
  const float fraction = ledUnits - fullLeds;

  for (uint8_t i = 0; i < LED_COUNT; i++) {
    if (i < fullLeds) {
      clientLedController->setPixelColor(i, base);
    } else if (i == fullLeds && fraction > 0.0f) {
      clientLedController->setPixelColor(i, Rgb(
        static_cast<uint8_t>(base.r * fraction),
        static_cast<uint8_t>(base.g * fraction),
        static_cast<uint8_t>(base.b * fraction)
      ));
    } else {
      clientLedController->setPixelColor(i, COLOR_BLACK);
    }
  }
}
} // namespace

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
  analogReadResolution(12);
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);
  
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
  
  // RGB LED test sequence
  Serial.println("Starting RGB LED test...");
  clientLedController->showRGBTest();
  
  // Start MQTT connection
  clientMqtt->begin();
  
  Serial.println("Client ready!");
  Serial.println("Button Controls:");
  Serial.println("- SHORT press: Buzz (when connected and game is open)");
  Serial.println("- VERY LONG press: Battery check (when not connected or in LOBBY)");
  Serial.println("- System automatically handles connection and state changes");
}

void loop() {
  // Keep battery indication stable by pausing reconnect attempts.
  if (!batteryDisplayActive && clientMqtt) {
    // Handle MQTT communication
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
    ButtonPress press = clientButtonHandler->getButtonPress();
    if (press != ButtonPress::NONE && clientManager) {
      if (press == ButtonPress::SHORT) {
        Serial.println("Button SHORT press");
        if (clientManager->canBuzz()) {
          clientManager->buzz();
        } else {
          Serial.println("Cannot buzz right now");
          Serial.printf("State: %d, Assigned: %s, Buzzed: %s, Connected: %s\n",
                        (int)clientManager->getState(),
                        clientManager->isAssigned() ? "yes" : "no",
                        clientManager->getData().hasBuzzed ? "yes" : "no",
                        (clientMqtt && clientMqtt->isConnected()) ? "yes" : "no");
        }
      } else if (press == ButtonPress::VERY_LONG) {
        const bool notConnected = !(clientMqtt && clientMqtt->isConnected());
        const bool inLobby = (currentGamePhase == Phase::LOBBY);
        const bool batteryCheckAllowed = notConnected || inLobby;

        if (batteryCheckAllowed) {
          const float vbat = readBatteryVoltage();
          const uint8_t percent = batteryPercentFromVoltage(vbat);
          Serial.printf("Battery check: %.2fV -> %u%%\n", vbat, percent);
          renderBatteryRing(percent);
          strip.show();
          batteryDisplayActive = true;
          batteryDisplayUntil = millis() + BATTERY_DISPLAY_MS;
        } else {
          Serial.println("Battery check ignored (only allowed when disconnected or LOBBY)");
        }
      }
    }
  }
  
  // Handle state animations
  if (clientManager) {
    if (batteryDisplayActive) {
      if (millis() >= batteryDisplayUntil) {
        batteryDisplayActive = false;
      }
    } else {
      clientManager->handleStateAnimations();
    }
  }
  
  delay(10); // Small delay for stability
}

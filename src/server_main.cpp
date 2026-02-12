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
ButtonPress deferredPress = ButtonPress::NONE;

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

void showBatteryOnRing(uint8_t percent) {
  if (!ledController) return;

  const Rgb base = batteryColor(percent);
  const float ledUnits = (percent / 100.0f) * 8.0f;
  const uint8_t fullLeds = static_cast<uint8_t>(ledUnits);
  const float fraction = ledUnits - fullLeds;

  for (uint8_t i = 0; i < LED_COUNT; i++) {
    ledController->setPixelColor(i, COLOR_BLACK);
  }

  for (uint8_t i = 0; i < 8; i++) {
    if (i < fullLeds) {
      ledController->setPixelColor(i, base);
    } else if (i == fullLeds && fraction > 0.0f) {
      ledController->setPixelColor(i, Rgb(
        static_cast<uint8_t>(base.r * fraction),
        static_cast<uint8_t>(base.g * fraction),
        static_cast<uint8_t>(base.b * fraction)
      ));
    }
  }
  ledController->showLEDs();
}
} // namespace

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
  analogReadResolution(12);
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);
  
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
  
  // RGB LED test sequence on all 18 LEDs
  Serial.println("Starting RGB LED test on all 18 LEDs...");
  ledController->showRGBTest();
  
  Serial.println("=== PHASE: BOOT ===");
  Serial.println("Phase Controls:");
  Serial.println("- SHORT press: LOBBY -> READY -> OPEN -> NEXT");
  Serial.println("- LONG press: Correct Answer / Reset");
  Serial.println("- VERY LONG press: Show battery 3s, then unlock game");
  Serial.println("Server ready for client connections!");
}

void loop() {
  // Handle MQTT broker
  mqttBroker.loop();
  
  // Handle button presses
  if (buttonHandler) {
    ButtonPress press = buttonHandler->checkButtonPress();
    if (press != ButtonPress::NONE && gameManager) {
      if (press == ButtonPress::VERY_LONG && currentPhase == Phase::LOBBY) {
        const float vbat = readBatteryVoltage();
        const uint8_t percent = batteryPercentFromVoltage(vbat);
        Serial.printf("Battery check before VERY_LONG action: %.2fV -> %u%%\n", vbat, percent);
        showBatteryOnRing(percent);
        batteryDisplayActive = true;
        batteryDisplayUntil = millis() + BATTERY_DISPLAY_MS;
        deferredPress = ButtonPress::VERY_LONG;
      } else {
        gameManager->handleButtonPress(press);
      }
    }
  }
  
  // Handle game phases
  if (gameManager) {
    if (batteryDisplayActive) {
      if (millis() >= batteryDisplayUntil) {
        batteryDisplayActive = false;
        if (deferredPress != ButtonPress::NONE) {
          gameManager->handleButtonPress(deferredPress);
          deferredPress = ButtonPress::NONE;
        }
      }
    } else {
      gameManager->handlePhase();
    }
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

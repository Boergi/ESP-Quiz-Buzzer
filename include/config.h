#pragma once
#include <Arduino.h>

// Hardware Pin Configuration (unified for Server & Client)
constexpr uint8_t LED_PIN = 5;      // WS2812B Data Pin (both Server & Client)
constexpr uint8_t BUTTON_PIN = 18;  // Main Button Pin with INPUT_PULLUP (both Server & Client)

// Additional Server Buttons (for Quiz Master control)
#ifdef SERVER
  constexpr uint8_t NEXT_BUTTON_PIN = 19;     // "Nächster Client" / "Falsche Antwort"
  constexpr uint8_t CORRECT_BUTTON_PIN = 21;  // "Richtige Antwort" / "Weiter"
#endif

// LED Count (differs between Server & Client)
#ifdef SERVER
  constexpr uint16_t LED_COUNT = 18;  // Server: 1-8 active player, 9-18 queue
#else
  constexpr uint16_t LED_COUNT = 8;   // Client: 8 LED ring
#endif

// LED Configuration
constexpr uint8_t LED_BRIGHTNESS = 64; // 0..255 (64 = 25% to prevent brownouts)

// Button Timing Configuration (ms)
constexpr uint16_t DEBOUNCE_MS = 20;
constexpr uint16_t SHORT_PRESS_MAX_MS = 600;
constexpr uint16_t LONG_PRESS_MS = 1200;
constexpr uint16_t VERY_LONG_PRESS_MS = 4000;

// WiFi/Network Configuration
constexpr char WIFI_SSID[] = "QUIZ-HUB";
constexpr char WIFI_PSK[] = "quiz12345"; // TODO: Change in production

// Network addresses (use IPAddress constructor in code)
#define AP_IP_ADDR 192, 168, 4, 1
#define AP_GATEWAY_ADDR 192, 168, 4, 1  
#define AP_SUBNET_ADDR 255, 255, 255, 0

// MQTT Configuration
constexpr char MQTT_HOST[] = "192.168.4.1";
constexpr uint16_t MQTT_PORT = 1883;
constexpr uint16_t MQTT_KEEPALIVE_INTERVAL = 60;

// Game Configuration
constexpr uint8_t MAX_CLIENTS = 10;
constexpr uint8_t MIN_CLIENTS_TO_START = 1;

// Ping Configuration
constexpr uint16_t PING_INTERVAL_MS = 5000;     // Ping every 5 seconds
constexpr uint16_t CLIENT_TIMEOUT_MS = 10000;   // Consider client dead after 10 seconds

// RGB Color Structure
struct Rgb {
  uint8_t r, g, b;
  constexpr Rgb(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0) : r(red), g(green), b(blue) {}
};

// Player Colors (10 unique colors, excluding white)
constexpr Rgb PLAYER_COLORS[MAX_CLIENTS] = {
  Rgb(255, 59, 48),   // 1. Rot
  Rgb(0, 122, 255),   // 2. Blau  
  Rgb(52, 199, 89),   // 3. Grün
  Rgb(255, 204, 0),   // 4. Gelb
  Rgb(255, 45, 85),   // 5. Magenta
  Rgb(50, 173, 230),  // 6. Cyan
  Rgb(255, 149, 0),   // 7. Orange
  Rgb(175, 82, 222),  // 8. Lila
  Rgb(26, 188, 156),  // 9. Türkis
  Rgb(88, 86, 214)    // 10. Indigo
};

// Special Colors
constexpr Rgb COLOR_WHITE(255, 255, 255);
constexpr Rgb COLOR_BLACK(0, 0, 0);
constexpr Rgb COLOR_ERROR(255, 0, 0);

// Animation Timings
constexpr uint16_t PULSE_SPEED_MS = 50;
constexpr uint16_t SPIN_SPEED_MS = 60;
constexpr uint16_t CELEBRATION_DURATION_MS = 5000;
constexpr uint16_t FLASH_DURATION_MS = 200;

# ESP32 Quiz-Buzzer System

A quiz buzzer system with one ESP32 server and multiple ESP32 clients, connected via WLAN and MQTT.

## 🎯 Project Overview

- **Server**: ESP32 with 18 WS2812B LEDs and button (Quiz Master)
- **Clients**: 2-10 ESP32s with 8 WS2812B LEDs and button each (Buzzers)
- **Connection**: WLAN Access Point (Server) + MQTT Protocol
- **Features**: Color-based player identification, buzz order, LED animations

## 🔧 Hardware Requirements

### Detailed Component List

#### Core Components (Server & Clients)

| Component | Description | Link |
|-----------|-------------|------|
| **ESP32 Board** | ESP32-CH340C-TYPEC Development Board | [AliExpress](https://de.aliexpress.com/item/1005004337178335.html) |
| **LED Ring (8 LEDs)** | WS2812 5050 RGB LED Ring, 8 Bit, 32mm outer diameter | [AZ-Delivery](https://www.az-delivery.de/products/led-ring-ws2812-5050-rgb) |
| **Button** | 12x12x7mm Tactile Push Button Switch, 4-Pin | [Amazon DE](https://www.amazon.de/dp/B0CP2DMNMQ) |
| **Button Spring** | Spring for button mechanism | [Amazon DE](https://www.amazon.de/dp/B0DHCPVJNS) |
| **Charging Module** | Lithium Battery Charging Module (TP4056 or similar) | [Amazon DE](https://www.amazon.de/dp/B0DBHMR5KJ) |

#### Server-Specific Components

| Component | Specification | Link |
|-----------|---------------|------|
| **LED Strip** | WS2812B LED Strip, 60 LEDs/m (for additional 10 status LEDs) | [Amazon DE](https://www.amazon.de/SEZO-Individuell-Adressierbar-Nicht-Wasserdicht-Inneneinrichtung/dp/B09N99DJRG) |
| **Battery** | Lithium Polymer Battery 3.7V 8000mAh (high capacity for server) | [Amazon DE](https://www.amazon.de/dp/B0F4WW11PX) |

#### Client-Specific Components

| Component | Specification | Link |
|-----------|---------------|------|
| **Battery** | Lithium Polymer Battery 3.7V 1800mAh (compact for clients) | [Amazon DE](https://www.amazon.de/dp/B095YC5PW8) |

#### Additional Components (both Server & Clients)

- **Series Resistor:** 330-470Ω for LED data line protection
- **Capacitor:** 1000µF electrolytic capacitor (5V ↔ GND) for power stabilization
- **Wiring:** Suitable gauge wire for connections
- **Case/Housing:** Custom 3D-printed or purchased enclosure (optional)

### Build Specifications

#### Server (Quiz Master Station)
- 1x ESP32-CH340C-TYPEC Board
- 1x 8-LED WS2812 Ring (player status display)
- 10x WS2812B LEDs from strip (queue/waiting list display) = **18 LEDs total**
- 1x Tactile Button (main control)
- 1x Button Spring
- 1x Lithium Battery Charging Module
- 1x 3.7V 8000mAh LiPo Battery
- 1x 470Ω Resistor
- 1x 1000µF Capacitor

#### Client (Player Buzzer) - Build 10x
- 1x ESP32-CH340C-TYPEC Board
- 1x 8-LED WS2812 Ring
- 1x Tactile Button
- 1x Button Spring
- 1x Lithium Battery Charging Module
- 1x 3.7V 1800mAh LiPo Battery
- 1x 470Ω Resistor
- 1x 1000µF Capacitor

## 📋 Pin Configuration (unified)

| Component | Server & Client |
|-----------|-----------------|
| LED Data  | **GPIO 5**      |
| Button    | **GPIO 18**     |
| LED Count | 18 (Server) / 8 (Client) |

> **Unified**: Both use identical pins → simpler hardware!

## 🚀 Setup & Compilation

### PlatformIO Installation
```bash
# Install PlatformIO CLI (if not already installed)
pip install platformio
```

### Compile Project
```bash
# Compile server
pio run --environment server

# Compile client
pio run --environment client

# Compile both
pio run
```

### Upload to Hardware
```bash
# Flash server (ESP32 connected via USB)
pio run --environment server --target upload

# Flash client (ESP32 connected via USB)
pio run --environment client --target upload
```

## 🎮 Functionality (Phase 1)

### Server
- **WiFi Access Point**: SSID "QUIZ-HUB", Password "quiz12345"
- **LED Test**: Automatic color cycle of all 10 player colors
- **Button Control**:
  - Short (< 600ms): Phase change LOBBY → READY → OPEN → LOBBY
  - Long (≥ 1.2s): Reset to LOBBY
  - Very Long (≥ 4s): Special Mode
- **LED Layout**:
  - LED 1-8: Active player (breathing animation)
  - LED 9-18: Queue display (buzz order)

### Client  
- **WiFi Station**: Connects to server AP
- **Unique ID**: Based on ESP32 MAC address (C-XXXXXX)
- **LED Animations**:
  - IDLE: Gentle pulsing in player color
  - BUZZ: All LEDs white (locked)
  - ACTIVE: Fast running light spin
  - CELEBRATE: Rainbow animation
  - WRONG: Red flash animation
- **Button Test**: Cycle through all states

## 🌈 Color Scheme

| Slot | Color   | RGB         | HEX     |
|------|---------|-------------|---------|
| 1    | Red     | 255,59,48   | #FF3B30 |
| 2    | Blue    | 0,122,255   | #007AFF |
| 3    | Green   | 52,199,89   | #34C759 |
| 4    | Yellow  | 255,204,0   | #FFCC00 |
| 5    | Magenta | 255,45,85   | #FF2D55 |
| 6    | Cyan    | 50,173,230  | #32ADE6 |
| 7    | Orange  | 255,149,0   | #FF9500 |
| 8    | Purple  | 175,82,222  | #AF52DE |
| 9    | Turquoise| 26,188,156  | #1ABC9C |
| 10   | Indigo  | 88,86,214   | #5856D6 |

## 📡 Network Configuration

- **Server IP**: 192.168.4.1
- **DHCP Range**: 192.168.4.2-192.168.4.254
- **MQTT Broker**: Port 1883 (on server)
- **Topic Namespace**: `quiz/*`

## 🔍 Serial Monitor

Both devices log detailed information:
```bash
# Server Monitor  
pio device monitor --environment server

# Client Monitor
pio device monitor --environment client
```

## 📦 Dependencies

- **Adafruit NeoPixel**: LED control
- **Bounce2**: Button debouncing  
- **ArduinoJson**: JSON parsing
- **PubSubClient**: MQTT client (for clients)
- **WiFi**: ESP32 WiFi functionality

## 🚧 Current Status: Phase 1 ✅

**Completed:**
- ✅ PlatformIO project setup
- ✅ Hardware configuration (config.h)  
- ✅ Protocol definitions (protocol.h)
- ✅ Server basic functions (LED tests, button handling, WiFi AP)
- ✅ Client basic functions (LED animations, button tests)
- ✅ Compilation for both environments

**Next Steps (Phase 2):**
- Integrate MQTT broker on server
- Client WiFi connection to server
- Basic MQTT communication

---

## 🔧 Technical Specifications

### Hardware Configuration

#### Server (Quiz Master Station)
- **ESP32 Dev Board**
- **18 WS2812B LEDs:** LEDs 1-8 (game status), LEDs 9-18 (player display)
- **1 Main Button:** GPIO 18 (game control)
- **2 Admin Buttons:** GPIO 19 & 21 (for future features)
- **LED Data Pin:** GPIO 5
- **Battery Powered:** Integrated battery

#### Clients (Player Buzzers)
- **ESP32 Dev Board**
- **8 WS2812B LEDs:** LED ring for status and animations
- **1 Buzzer Button:** GPIO 18
- **LED Data Pin:** GPIO 5
- **Battery Powered:** Integrated battery

### Network Configuration
- **SSID:** QUIZ-HUB
- **Password:** quiz12345
- **IP Range:** 192.168.4.x
- **Server IP:** 192.168.4.1
- **Max. Clients:** 10
- **MQTT Port:** 1883

### Timing Parameters
- **Short Button Press:** < 600ms
- **Long Button Press:** ≥ 1200ms
- **Very Long Button Press:** ≥ 4000ms
- **Celebration Duration:** 5 seconds
- **Client Timeout:** 10 seconds
- **Ping Interval:** 5 seconds

### Power Consumption (Battery Operation)
- **Server:** ~2A at 5V (18 LEDs + ESP32)
- **Client:** ~1A at 5V (8 LEDs + ESP32)
- **Total:** ~12A for complete system (1 server + 10 clients)
- **Battery Life:** Depends on battery capacity and LED brightness

### Player Colors (RGB Values)
| No. | Color | RGB | No. | Color | RGB |
|-----|-------|-----|-----|-------|-----|
| 1 | 🔴 Red | (255,0,0) | 6 | 🔵 Cyan | (0,255,255) |
| 2 | 🔵 Blue | (0,0,255) | 7 | 🟠 Orange | (255,128,0) |
| 3 | 🟢 Green | (0,255,0) | 8 | 🟣 Violet | (128,0,255) |
| 4 | 🟡 Yellow | (255,255,0) | 9 | 🩷 Pink | (255,192,203) |
| 5 | 🟣 Magenta | (255,0,255) | 10 | ⚪ White | (255,255,255) |

### Libraries
- **Adafruit NeoPixel:** LED control
- **Bounce2:** Button debouncing
- **ArduinoJson:** MQTT messages
- **PubSubClient:** MQTT client (clients)
- **PicoMQTT:** MQTT broker (server)

---

## 📚 Additional Documentation

- **User Manual:** `USER_MANUAL.md` - For Quiz Masters
- **Operating Instructions (German):** `BEDIENUNGSANLEITUNG.md` - Für Quiz-Master
- **Complete Specification:** `esp_32_quiz_buzzer_server_clients_umsetzungsleitfaden_fur_platform_io.md`


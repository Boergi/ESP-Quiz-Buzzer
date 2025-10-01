# ESP32 Quiz-Buzzer System

Ein Quiz-Buzzer-System mit einem ESP32-Server und mehreren ESP32-Clients, verbunden über WLAN und MQTT.

## 🎯 Projektübersicht

- **Server**: ESP32 mit 18 WS2812B LEDs und Taster (Quiz-Master)
- **Clients**: 2-10 ESP32s mit je 8 WS2812B LEDs und Taster (Buzzer)
- **Verbindung**: WLAN Access Point (Server) + MQTT-Protokoll
- **Features**: Farb-basierte Spieleridentifikation, Buzz-Reihenfolge, LED-Animationen

## 🔧 Hardware-Requirements

### Server (Quiz-Master)
- ESP32 DevKitC
- 18x WS2812B LED-Ring
- Taster (momentary, NO)
- Netzteil 5V ≥ 2A
- Serienwiderstand (330-470Ω) für LED-Data
- Elko 1000µF (5V ↔ GND)

### Client (Buzzer)
- ESP32 DevKitC  
- 8x WS2812B LED-Ring
- Taster (momentary, NO)
- Netzteil 5V ≥ 1A
- Serienwiderstand (330-470Ω) für LED-Data
- Elko 1000µF (5V ↔ GND)

## 📋 Pin-Konfiguration (einheitlich)

| Komponente | Server & Client |
|------------|-----------------|
| LED Data   | **GPIO 5**      |
| Button     | **GPIO 18**     |
| LED Count  | 18 (Server) / 8 (Client) |

> **Vereinheitlicht**: Beide verwenden identische Pins → einfachere Hardware!

## 🚀 Setup & Kompilierung

### PlatformIO Installation
```bash
# PlatformIO CLI installieren (falls nicht vorhanden)
pip install platformio
```

### Projekt kompilieren
```bash
# Server kompilieren
pio run --environment server

# Client kompilieren  
pio run --environment client

# Beide kompilieren
pio run
```

### Upload auf Hardware
```bash
# Server flashen (ESP32 am USB angeschlossen)
pio run --environment server --target upload

# Client flashen (ESP32 am USB angeschlossen)
pio run --environment client --target upload
```

## 🎮 Funktionalität (Phase 1)

### Server
- **WiFi Access Point**: SSID "QUIZ-HUB", Password "quiz12345"
- **LED-Test**: Automatischer Farbzyklus aller 10 Spielerfarben
- **Button-Control**:
  - Kurz (< 600ms): Phasenwechsel LOBBY → READY → OPEN → LOBBY
  - Lang (≥ 1.2s): Reset zu LOBBY
  - Sehr lang (≥ 4s): Special Mode
- **LED-Layout**:
  - LED 1-8: Aktiver Spieler (breathing animation)
  - LED 9-18: Queue-Anzeige (Buzz-Reihenfolge)

### Client  
- **WiFi Station**: Verbindet sich zum Server-AP
- **Eindeutige ID**: Basiert auf ESP32 MAC-Adresse (C-XXXXXX)
- **LED-Animationen**:
  - IDLE: Sanftes Pulsieren in Spielerfarbe
  - BUZZ: Alle LEDs weiß (gesperrt)
  - ACTIVE: Schneller Lauflicht-Spin
  - CELEBRATE: Rainbow-Animation
  - WRONG: Rote Blitz-Animation
- **Button-Test**: Zyklus durch alle Zustände

## 🌈 Farbschema

| Slot | Farbe   | RGB         | HEX     |
|------|---------|-------------|---------|
| 1    | Rot     | 255,59,48   | #FF3B30 |
| 2    | Blau    | 0,122,255   | #007AFF |
| 3    | Grün    | 52,199,89   | #34C759 |
| 4    | Gelb    | 255,204,0   | #FFCC00 |
| 5    | Magenta | 255,45,85   | #FF2D55 |
| 6    | Cyan    | 50,173,230  | #32ADE6 |
| 7    | Orange  | 255,149,0   | #FF9500 |
| 8    | Lila    | 175,82,222  | #AF52DE |
| 9    | Türkis  | 26,188,156  | #1ABC9C |
| 10   | Indigo  | 88,86,214   | #5856D6 |

## 📡 Netzwerk-Konfiguration

- **Server IP**: 192.168.4.1
- **DHCP Range**: 192.168.4.2-192.168.4.254
- **MQTT Broker**: Port 1883 (auf Server)
- **Topic Namespace**: `quiz/*`

## 🔍 Serial Monitor

Beide Devices loggen detaillierte Informationen:
```bash
# Server Monitor  
pio device monitor --environment server

# Client Monitor
pio device monitor --environment client
```

## 📦 Dependencies

- **Adafruit NeoPixel**: LED-Steuerung
- **Bounce2**: Button-Entprellung  
- **ArduinoJson**: JSON-Parsing
- **PubSubClient**: MQTT-Client (für Clients)
- **WiFi**: ESP32 WiFi-Funktionalität

## 🚧 Aktueller Status: Phase 1 ✅

**Abgeschlossen:**
- ✅ PlatformIO-Projekt Setup
- ✅ Hardware-Konfiguration (config.h)  
- ✅ Protocol-Definitionen (protocol.h)
- ✅ Server-Grundfunktionen (LED-Tests, Button-Handling, WiFi-AP)
- ✅ Client-Grundfunktionen (LED-Animationen, Button-Tests)
- ✅ Kompilierung für beide Environments

**Nächste Schritte (Phase 2):**
- MQTT-Broker auf Server integrieren
- Client WiFi-Verbindung zum Server
- Grundlegende MQTT-Kommunikation

---

## 🔧 Technische Spezifikationen

### Hardware-Konfiguration

#### Server (Quiz-Master Station)
- **ESP32 Dev Board**
- **18 WS2812B LEDs:** LEDs 1-8 (Spielstatus), LEDs 9-18 (Spieler-Anzeige)
- **1 Haupt-Button:** GPIO 18 (Spielsteuerung)
- **2 Admin-Buttons:** GPIO 19 & 21 (für zukünftige Features)
- **LED-Data Pin:** GPIO 5
- **Akkubetrieb:** Integrierter Akku

#### Clients (Spieler-Buzzer)
- **ESP32 Dev Board**
- **8 WS2812B LEDs:** LED-Ring für Status und Animationen
- **1 Buzzer-Button:** GPIO 18
- **LED-Data Pin:** GPIO 5
- **Akkubetrieb:** Integrierter Akku

### Netzwerk-Konfiguration
- **SSID:** QUIZ-HUB
- **Passwort:** quiz12345
- **IP-Bereich:** 192.168.4.x
- **Server IP:** 192.168.4.1
- **Max. Clients:** 10
- **MQTT Port:** 1883

### Timing-Parameter
- **Kurzer Button-Druck:** < 600ms
- **Langer Button-Druck:** ≥ 1200ms
- **Sehr langer Button-Druck:** ≥ 4000ms
- **Celebration-Dauer:** 5 Sekunden
- **Client-Timeout:** 10 Sekunden
- **Ping-Intervall:** 5 Sekunden

### Stromverbrauch (Akkubetrieb)
- **Server:** ~2A bei 5V (18 LEDs + ESP32)
- **Client:** ~1A bei 5V (8 LEDs + ESP32)
- **Gesamt:** ~12A für komplettes System (1 Server + 10 Clients)
- **Akkulaufzeit:** Abhängig von Akku-Kapazität und LED-Helligkeit

### Player-Farben (RGB-Werte)
| Nr. | Farbe | RGB | Nr. | Farbe | RGB |
|-----|-------|-----|-----|-------|-----|
| 1 | 🔴 Rot | (255,0,0) | 6 | 🔵 Cyan | (0,255,255) |
| 2 | 🔵 Blau | (0,0,255) | 7 | 🟠 Orange | (255,128,0) |
| 3 | 🟢 Grün | (0,255,0) | 8 | 🟣 Violett | (128,0,255) |
| 4 | 🟡 Gelb | (255,255,0) | 9 | 🩷 Rosa | (255,192,203) |
| 5 | 🟣 Magenta | (255,0,255) | 10 | ⚪ Weiß | (255,255,255) |

### Bibliotheken
- **Adafruit NeoPixel:** LED-Steuerung
- **Bounce2:** Button-Entprellung
- **ArduinoJson:** MQTT-Nachrichten
- **PubSubClient:** MQTT-Client (Clients)
- **PicoMQTT:** MQTT-Broker (Server)

---

## 📚 Weitere Dokumentation

- **Bedienungsanleitung:** `BEDIENUNGSANLEITUNG_QUIZ_DJ.md` - Für Quiz-Master
- **Vollständige Spezifikation:** `esp_32_quiz_buzzer_server_clients_umsetzungsleitfaden_fur_platform_io.md`


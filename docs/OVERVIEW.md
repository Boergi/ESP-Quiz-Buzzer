# ESP32 Quiz-Buzzer System Documentation

Welcome to the comprehensive documentation for the ESP32 Quiz-Buzzer System!

## 📚 Documentation Structure

This documentation is organized into several sections to help you build, configure, and operate your quiz buzzer system:

### For Users & Operators

- **[User Manual (English)](USER_MANUAL.md)** - Complete guide for Quiz Masters and DJs
- **[Bedienungsanleitung (Deutsch)](BEDIENUNGSANLEITUNG.md)** - Deutsche Anleitung für Quiz-Master

### For Builders & Developers

- **[Wiring Guide](WIRING_GUIDE.md)** - Complete wiring instructions with diagrams for both Server and Client devices
- **[Technical Specification](TECHNICAL_SPECIFICATION.md)** - Detailed technical documentation including:
  - System architecture
  - MQTT protocol specification
  - State machine logic
  - Code structure and implementation details
  - Edge case handling

### Hardware Reference

- **Wiring Diagrams:**
  - [Server Wiring Diagram](images/server-sheet.png)
  - [Client Wiring Diagram](images/client-sheet.png)

## 🎯 Quick Start

**New to the project?** Start here:

1. Read the main [README.md](../README.md) for project overview and hardware requirements
2. Follow the [Wiring Guide](WIRING_GUIDE.md) to assemble your devices
3. Compile and upload the code using PlatformIO (see README.md)
4. Familiarize yourself with the [User Manual](USER_MANUAL.md) for operation

**Developing or customizing?** Check out:

1. [Technical Specification](TECHNICAL_SPECIFICATION.md) for system architecture
2. Review `include/config.h` and `include/protocol.h` for configuration
3. Examine the source code in `src/` directory

## 🔧 System Overview

The ESP32 Quiz-Buzzer System consists of:

- **1 Server (Quiz Master Station):** ESP32 with 18 WS2812B LEDs, acts as WiFi Access Point and MQTT broker
- **2-10 Clients (Player Buzzers):** ESP32 with 8 WS2812B LEDs each, connects to server via WiFi
- **MQTT-based communication:** Real-time game state synchronization
- **Color-coded players:** Each buzzer gets a unique color for easy identification
- **Battery-powered:** Completely wireless operation

### Key Features

✅ Automatic player registration and color assignment  
✅ Buzz order tracking with visual queue display  
✅ LED animations for different game states  
✅ Automatic reconnection with state restoration  
✅ Simple button-based control for Quiz Master  
✅ Scalable: 2-10 players supported  

## 🌈 Player Colors

The system assigns 10 unique, easily distinguishable colors:

| Slot | Color | RGB |
|------|-------|-----|
| 1 | 🔴 Red | (255,0,0) |
| 2 | 🔵 Blue | (0,0,255) |
| 3 | 🟢 Green | (0,255,0) |
| 4 | 🟡 Yellow | (255,255,0) |
| 5 | 🟣 Magenta | (255,0,255) |
| 6 | 🔵 Cyan | (0,255,255) |
| 7 | 🟠 Orange | (255,128,0) |
| 8 | 🟣 Violet | (128,0,255) |
| 9 | 🩷 Pink | (255,192,203) |
| 10 | ⚪ White | (255,255,255) |

## 📡 Network Configuration

- **Server SSID:** QUIZ-HUB
- **Password:** quiz12345
- **Server IP:** 192.168.4.1
- **MQTT Port:** 1883
- **Max Clients:** 10

## 🎮 Game Flow (Quick Reference)

1. **LOBBY:** Clients connect and get assigned colors
2. **READY:** Quiz Master prepares to ask question (press button)
3. **OPEN:** Question is active, buzzers pulse (players can buzz!)
4. **ANSWER:** First buzzer activates, table gives answer
5. **CELEBRATION:** Correct answer triggers rainbow animation
6. **RESET:** Return to READY for next question

For detailed operation, see the [User Manual](USER_MANUAL.md).

## 🛠️ Technical Highlights

### Unified Hardware Design
Both Server and Client use the same pin configuration:
- **LED Data Pin:** GPIO 5
- **Button Pin:** GPIO 18
- **LED Count:** 18 (Server) / 8 (Client)

This simplifies assembly and reduces errors!

### Power Management
- **Server Battery:** 3.7V 8000mAh LiPo (long runtime for 18 LEDs)
- **Client Battery:** 3.7V 1800mAh LiPo (compact for 8 LEDs)
- **Brightness Limiting:** 64/255 (25%) to prevent brownouts
- **TP4056 Charging Module:** Built-in USB charging

### Robust Communication
- **MQTT QoS 1** for critical buzz messages (guaranteed delivery)
- **Automatic reconnection** with state preservation
- **Ping/keepalive** mechanism (5s interval, 10s timeout)
- **Time-based tiebreaking** for simultaneous buzzes

## 📂 Project Structure

```
PR-ESP-QB/
├── include/           # Header files
│   ├── config.h       # Pin configuration, colors, timings
│   └── protocol.h     # MQTT topics, game states
├── src/               # Source code
│   ├── server_main.cpp
│   └── client_main.cpp
├── docs/              # Documentation (you are here!)
│   ├── OVERVIEW.md
│   ├── TECHNICAL_SPECIFICATION.md
│   ├── WIRING_GUIDE.md
│   ├── USER_MANUAL.md
│   ├── BEDIENUNGSANLEITUNG.md
│   └── images/
│       ├── server-sheet.png
│       └── client-sheet.png
└── platformio.ini     # Build configuration
```

## 🚀 Getting Started with Development

### Prerequisites
- PlatformIO IDE or CLI
- ESP32 development boards
- WS2812B LED rings/strips
- Basic soldering skills

### Compilation
```bash
# Compile server
pio run --environment server

# Compile client
pio run --environment client

# Upload to device
pio run --environment server --target upload
```

### Debugging
```bash
# Serial monitor for server
pio device monitor --environment server

# Serial monitor for client
pio device monitor --environment client
```

## 🤝 Contributing

When modifying the system:

1. **Update config.h** for hardware changes
2. **Update protocol.h** for MQTT message changes
3. **Test thoroughly** with multiple clients
4. **Update documentation** to reflect your changes
5. **Maintain pin compatibility** (unified design = easier support!)

## 📞 Support & Questions

For technical questions or issues:

1. Check the [Technical Specification](TECHNICAL_SPECIFICATION.md) for implementation details
2. Review the [Wiring Guide](WIRING_GUIDE.md) for connection problems
3. Consult the [User Manual](USER_MANUAL.md) for operational issues
4. Examine serial monitor output for debugging

## 📝 License & Credits

This project is open source and designed for wedding entertainment and quiz events. Feel free to build, modify, and improve!

**Special thanks to all components:**
- Adafruit NeoPixel library
- Bounce2 button library
- ArduinoJson
- PubSubClient (MQTT)
- PicoMQTT (embedded broker)

---

**Happy Buzzing! 🎮✨**

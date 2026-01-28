# Technical Specification - ESP32 Quiz-Buzzer System

Complete technical documentation for developers and advanced users.

---

## Table of Contents

1. [System Architecture](#system-architecture)
2. [Hardware Configuration](#hardware-configuration)
3. [Network & MQTT Protocol](#network--mqtt-protocol)
4. [State Machine](#state-machine)
5. [LED System](#led-system)
6. [Button Logic](#button-logic)
7. [Edge Cases & Error Handling](#edge-cases--error-handling)
8. [Code Structure](#code-structure)
9. [Performance & Optimization](#performance--optimization)
10. [Testing & Validation](#testing--validation)

---

## System Architecture

### Overview

The ESP32 Quiz-Buzzer System uses a **Server-Client architecture** with one ESP32 acting as both WiFi Access Point and MQTT broker, while 2-10 client ESP32s connect as stations.

```
┌─────────────────────────────────────────────────────┐
│                    SERVER ESP32                     │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────┐ │
│  │  WiFi AP     │  │ MQTT Broker  │  │   Game   │ │
│  │  (SoftAP)    │  │  (PicoMQTT)  │  │  Manager │ │
│  └──────────────┘  └──────────────┘  └──────────┘ │
│         │                  │                │       │
│         │                  │                │       │
│  ┌──────▼──────────────────▼────────────────▼────┐ │
│  │        LED Controller (18 WS2812B)            │ │
│  │   [1-8: Active Player] [9-18: Queue Display]  │ │
│  └───────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────┘
                         │
                    WiFi Network
                  (QUIZ-HUB AP)
                         │
          ┌──────────────┼──────────────┐
          │              │              │
┌─────────▼──────┐ ┌────▼──────┐ ┌────▼──────┐
│  CLIENT ESP32  │ │  CLIENT   │ │  CLIENT   │
│  ┌──────────┐  │ │           │ │   ...     │
│  │WiFi STA  │  │ │           │ │           │
│  └────┬─────┘  │ │           │ │           │
│  ┌────▼─────┐  │ │           │ │ (2-10     │
│  │MQTT      │  │ │           │ │  Clients) │
│  │Client    │  │ │           │ │           │
│  └────┬─────┘  │ │           │ │           │
│  ┌────▼─────┐  │ │           │ │           │
│  │LED Ctrl  │  │ │           │ │           │
│  │(8 LEDs)  │  │ │           │ │           │
│  └──────────┘  │ │           │ │           │
└────────────────┘ └───────────┘ └───────────┘
```

### Communication Flow

1. **Client Boot:**
   - Connects to server's WiFi AP ("QUIZ-HUB")
   - Connects to MQTT broker (192.168.4.1:1883)
   - Sends JOIN message with unique ID (MAC-based)

2. **Server Response:**
   - Assigns unique color from available slots (1-10)
   - Publishes ASSIGN message to specific client
   - Updates game state (LOBBY → READY when ≥ min clients)

3. **Game Loop:**
   - Server publishes STATE changes to all clients
   - Clients react with LED animations
   - Clients send BUZZ messages when button pressed
   - Server tracks buzz order and manages active player

4. **Resilience:**
   - Ping/keepalive every 5 seconds
   - Auto-reconnect with state restoration
   - Timeout detection (10s without ping = disconnect)

---

## Hardware Configuration

### Pin Mapping (Unified!)

Both Server and Client use **identical GPIO pins** for simplified assembly:

```cpp
// From include/config.h
constexpr uint8_t LED_PIN = 5;       // WS2812B Data
constexpr uint8_t BUTTON_PIN = 18;   // Main button (INPUT_PULLUP)

#ifdef SERVER
  constexpr uint16_t LED_COUNT = 18;  // 8 ring + 10 strip
#else
  constexpr uint16_t LED_COUNT = 8;   // 8 ring only
#endif
```

**Optional server buttons** (not currently used in Phase 1):
- GPIO 19: Next button (wrong answer shortcut)
- GPIO 21: Correct button (correct answer shortcut)

### Component Specifications

#### Server
- **ESP32-CH340C-TYPEC** development board
- **WS2812B LEDs:** 18 total (8-LED ring + 10 from strip)
- **Battery:** 3.7V 8000mAh LiPo
- **Charging:** TP4056 USB-C module
- **DC-DC Step-Up Converter:** Optional but recommended (boosts 3.7V battery to stable 5V)
- **Power switch:** In positive line (bottom hole mount)
- **Protection:** 470Ω resistor (data line), 1000µF capacitor (power)

#### Client
- **ESP32-CH340C-TYPEC** development board
- **WS2812B LEDs:** 8-LED ring
- **Battery:** 3.7V 1800mAh LiPo
- **Charging:** TP4056 USB-C module
- **Power switch:** In positive line (bottom hole mount)
- **Protection:** 470Ω resistor (data line), 1000µF capacitor (power)

### Power Budget

| Component | Current Draw | Notes |
|-----------|--------------|-------|
| ESP32 (idle) | ~80-100mA | WiFi active |
| ESP32 (peak) | ~240mA | WiFi TX/RX |
| WS2812B LED | ~60mA max | Per LED at full white |
| **Server total** | ~1200mA | 18 LEDs @ 64 brightness |
| **Client total** | ~600mA | 8 LEDs @ 64 brightness |

**Brightness limiting** (64/255 = 25%) is critical to:
- Prevent brownouts (voltage sag under load)
- Extend battery life
- Reduce heat generation

### LED Layout

#### Server LED Mapping

```
Physical Layout:
┌─────────────────────────────────────┐
│  [LED Ring - 8 LEDs]                │
│       LED 1-8                       │
│   Active Player Display             │
│                                     │
│  [LED Strip - 10 LEDs]              │
│   LED 9 10 11 12 13 14 15 16 17 18  │
│   Queue Display (buzz order)        │
└─────────────────────────────────────┘

Data Flow: GPIO 5 → Ring (1-8) → Strip (9-18)
```

#### Client LED Mapping

```
Physical Layout:
    LED 1
LED 8   LED 2
LED 7   LED 3
    LED 6
LED 6   LED 4
    LED 5

8-LED Ring (numbered clockwise)
```

---

## Network & MQTT Protocol

### WiFi Configuration

```cpp
// From include/config.h
constexpr char WIFI_SSID[] = "QUIZ-HUB";
constexpr char WIFI_PSK[] = "quiz12345";

// Server IP configuration
AP_IP: 192.168.4.1
Gateway: 192.168.4.1
Subnet: 255.255.255.0
DHCP Range: 192.168.4.2 - 192.168.4.254
```

**Server:** ESP32 SoftAP mode (Access Point)  
**Clients:** ESP32 STA mode (Station)

### MQTT Broker

- **Implementation:** PicoMQTT (embedded on server ESP32)
- **Port:** 1883
- **Keepalive:** 60 seconds
- **Max Clients:** 10 concurrent connections

### MQTT Topic Structure

All topics use the `quiz/` namespace:

```cpp
// From include/protocol.h
quiz/announce      // Server → All (retained)
quiz/join          // Client → Server
quiz/assign/{id}   // Server → Specific Client
quiz/state         // Server → All
quiz/buzz          // Client → Server (QoS 1!)
quiz/queue         // Server → All
quiz/cmd           // Server → All or specific Client
quiz/ping          // Bidirectional keepalive
```

### Message Formats (JSON)

#### 1. Server Announce (Retained)

Published by server on startup, retained for new clients.

```json
{
  "version": "1.0",
  "maxClients": 10,
  "locked": false
}
```

**Fields:**
- `version`: Protocol version
- `maxClients`: Maximum supported clients (10)
- `locked`: Join allowed (false) or locked (true)

---

#### 2. Client Join

Sent by client after connecting to WiFi and MQTT broker.

```json
{
  "id": "C-A1B2C3D4",
  "cap": 8,
  "fw": "1.0.0"
}
```

**Fields:**
- `id`: Unique client ID (C- + last 4 bytes of MAC in hex)
- `cap`: LED capability (always 8 for clients)
- `fw`: Firmware version string

---

#### 3. Server Assignment

Published to `quiz/assign/{clientId}` for specific client.

```json
{
  "color": "#FF0000",
  "slot": 1
}
```

**Fields:**
- `color`: Hex color code (e.g., "#FF0000" for red)
- `slot`: Player slot number (1-10)

**Color assignments** from `config.h`:
```cpp
Slot 1:  Red     (255,0,0)
Slot 2:  Blue    (0,0,255)
Slot 3:  Green   (0,255,0)
Slot 4:  Yellow  (255,255,0)
Slot 5:  Magenta (255,0,255)
Slot 6:  Cyan    (0,255,255)
Slot 7:  Orange  (255,128,0)
Slot 8:  Violet  (128,0,255)
Slot 9:  Pink    (255,192,203)
Slot 10: White   (255,255,255)
```

---

#### 4. Game State

Published by server on every phase change.

```json
{
  "phase": "OPEN",
  "locked": true
}
```

**Fields:**
- `phase`: Current game phase (see State Machine section)
- `locked`: Join locked (true during active game)

**Valid phases:** `BOOT`, `LOBBY`, `READY`, `OPEN`, `ANSWER`, `RESET`

---

#### 5. Client Buzz (QoS 1!)

Sent by client when button pressed during OPEN phase.

```json
{
  "id": "C-A1B2C3D4",
  "t": 12345678
}
```

**Fields:**
- `id`: Client ID (for verification)
- `t`: Client-side timestamp (millis()) for tiebreaking

**Important:** This message uses **QoS 1** (guaranteed delivery) to ensure no buzzes are lost!

---

#### 6. Queue Update

Published by server when buzz queue changes.

```json
{
  "order": ["C-A1B2C3D4", "C-E5F6G7H8", "C-I9J0K1L2"],
  "active": "C-A1B2C3D4"
}
```

**Fields:**
- `order`: Array of client IDs in buzz order (first = fastest)
- `active`: Currently active client ID (answering now)

**Maximum 10 clients** in queue (MAX_CLIENTS).

---

#### 7. Command Messages

Published by server to trigger client animations.

```json
{
  "cmd": "CELEBRATE",
  "target": "C-A1B2C3D4"
}
```

**Fields:**
- `cmd`: Command string (see below)
- `target`: (Optional) Specific client ID, or omit for broadcast

**Valid commands:**
- `LIGHT_WHITE`: Client shows white (locked after buzz)
- `ANIM_ACTIVE`: Client shows active animation (spinning)
- `IDLE_COLOR`: Client returns to idle (pulsing player color)
- `CELEBRATE`: Rainbow celebration (correct answer)
- `WRONG_FLASH`: Red flash (incorrect answer)
- `RESET`: Reset to LOBBY state

---

#### 8. Ping/Keepalive

Bidirectional ping to detect disconnections.

```json
{
  "id": "C-A1B2C3D4",
  "t": 12345678
}
```

**Configuration:**
```cpp
constexpr uint16_t PING_INTERVAL_MS = 5000;   // Ping every 5s
constexpr uint16_t CLIENT_TIMEOUT_MS = 10000; // Timeout after 10s
```

Server tracks last ping time per client. If no ping received within timeout, client is considered disconnected and removed from game.

---

## State Machine

### Server State Machine

```
┌─────────┐
│  BOOT   │  Power on, initialize WiFi AP & MQTT broker
└────┬────┘
     │
     ▼
┌─────────┐
│  LOBBY  │  Wait for clients to join (locked=false)
└────┬────┘  LED: Cyan running light animation
     │  
     │ (≥ MIN_CLIENTS && Button SHORT)
     ▼
┌─────────┐
│  READY  │  Ready for question (locked=true, no new joins)
└────┬────┘  LED: Green-Orange ping-pong animation
     │
     │ (Button SHORT: Quiz Master ready)
     ▼
┌─────────┐
│  OPEN   │  Question active, buzz allowed
└────┬────┘  LED: Green pulsing
     │
     │ (First BUZZ received)
     ▼
┌─────────┐
│ ANSWER  │  Client answering
└────┬────┘  LED: Show active client color + queue
     │
     ├─ (Button SHORT: Wrong answer)
     │  └─→ Next client in queue → Stay in ANSWER
     │
     └─ (Button LONG: Correct answer)
        └─→ RESET → Celebration → READY
```

### Server State Transitions

| Current State | Event | Next State | Actions |
|--------------|-------|------------|---------|
| BOOT | Setup complete | LOBBY | Start WiFi AP, MQTT broker, LED test |
| LOBBY | ≥ MIN_CLIENTS + Button SHORT | READY | Lock joins, stop accepting new clients |
| READY | Button SHORT | OPEN | Allow buzzing, green pulsing |
| OPEN | First BUZZ | ANSWER | Set active client, show queue |
| ANSWER | Button SHORT | ANSWER (next) | Next client in queue becomes active |
| ANSWER | Button LONG | RESET | Celebration, clear queue |
| RESET | Celebration done | READY | Ready for next question |
| Any | Button VERY_LONG (4s) | LOBBY | Emergency reset, unlock joins |

### Client State Machine

```
┌──────────────┐
│ DISCONNECTED │  Initial state / connection lost
└──────┬───────┘
       │
       ▼
┌──────────────┐
│  CONNECTING  │  Connecting to WiFi & MQTT
└──────┬───────┘
       │
       ▼
┌──────────────┐
│   ASSIGNED   │  Received color assignment
└──────┬───────┘
       │
       ▼
┌──────────────┐
│     IDLE     │  Waiting for question
└──────┬───────┘  LED: Pulsing in player color
       │
       │ (Phase=OPEN && Button pressed)
       ▼
┌──────────────┐
│ LOCKED_AFTER │  Buzzed, waiting in queue
│    _BUZZ     │  LED: Solid white
└──────┬───────┘
       │
       │ (CMD: ANIM_ACTIVE)
       ▼
┌──────────────┐
│ ACTIVE_TURN  │  Currently answering
└──────┬───────┘  LED: Fast spinning in player color
       │
       ├─ (CMD: CELEBRATE)
       │  └─→ CELEBRATE → Rainbow animation → IDLE
       │
       └─ (CMD: WRONG_FLASH)
          └─→ WRONG_FLASH → Red flash → IDLE (or LOCKED if still in queue)
```

### Client State Transitions

| Current State | Event | Next State | LED Animation |
|--------------|-------|------------|---------------|
| DISCONNECTED | Connect WiFi+MQTT | CONNECTING | Red pulsing |
| CONNECTING | Receive ASSIGN | ASSIGNED | Show player color |
| ASSIGNED | Phase=READY | IDLE | Pulse player color |
| IDLE | Button press + Phase=OPEN | LOCKED_AFTER_BUZZ | Solid white |
| LOCKED_AFTER_BUZZ | CMD: ANIM_ACTIVE | ACTIVE_TURN | Fast spin player color |
| ACTIVE_TURN | CMD: CELEBRATE | CELEBRATE | Rainbow 5s |
| ACTIVE_TURN | CMD: WRONG_FLASH | WRONG_FLASH | Red flash |
| CELEBRATE/WRONG_FLASH | Animation done | IDLE | Pulse player color |

---

## LED System

### LED Controller Architecture

Each device has a dedicated LED controller managing animations:

```cpp
class LEDController {
  private:
    Adafruit_NeoPixel strip;
    AnimationType currentAnimation;
    Rgb currentColor;
    uint16_t animationStep;
    
  public:
    void update();  // Called in loop()
    void setAnimation(AnimationType type, Rgb color);
    void setSolid(Rgb color);
    void clear();
};
```

### Animation Types

Defined in `protocol.h`:

```cpp
enum class AnimationType : uint8_t {
  SOLID = 0,   // All LEDs one color
  PULSE,       // Breathing effect
  SPIN,        // Running light around ring
  FLASH,       // On/off blinking
  RAINBOW,     // Rainbow cycle
  OFF          // All LEDs off
};
```

### Server LED Patterns

| Phase | LEDs 1-8 (Ring) | LEDs 9-18 (Strip) |
|-------|-----------------|-------------------|
| BOOT | Test pattern (cycle all colors) | Test pattern |
| LOBBY | Cyan running light | Off |
| READY | Green-orange ping-pong | Off |
| OPEN | Green pulsing | Off |
| ANSWER | Active client color (breathing) | Queue order (solid colors) |
| RESET | Rainbow celebration | Rainbow |

#### Queue Display Logic (LEDs 9-18)

```cpp
// Pseudocode for server queue display
for (int i = 0; i < 10; i++) {
  int ledIndex = 8 + i;  // LEDs 9-18
  
  if (i < queueLength) {
    // Show client color at queue position
    Rgb color = getClientColor(queue[i]);
    strip.setPixelColor(ledIndex, color);
  } else {
    // No client in this position
    strip.setPixelColor(ledIndex, BLACK);
  }
}
```

**Example:** If 3 clients buzzed (Red, Blue, Green):
- LED 9: Red (1st buzzer)
- LED 10: Blue (2nd buzzer)
- LED 11: Green (3rd buzzer)
- LED 12-18: Off (no more buzzers)

### Client LED Patterns

| State | Animation | Color | Speed |
|-------|-----------|-------|-------|
| DISCONNECTED | Pulse | Red | Slow |
| CONNECTING | Pulse | Red | Fast |
| ASSIGNED | Solid | Player color | - |
| IDLE | Pulse | Player color | Slow (50ms/step) |
| LOCKED_AFTER_BUZZ | Solid | White | - |
| ACTIVE_TURN | Spin | Player color | Fast (60ms/step) |
| CELEBRATE | Rainbow | - | 5s total |
| WRONG_FLASH | Flash | Red | 200ms on/off x3 |

### Animation Implementation

#### Pulse (Breathing)

```cpp
// Simplified pseudocode
void updatePulseAnimation() {
  static uint8_t brightness = 0;
  static int8_t direction = 1;
  
  brightness += direction * 5;
  
  if (brightness >= 255 || brightness <= 0) {
    direction *= -1;  // Reverse direction
  }
  
  for (int i = 0; i < LED_COUNT; i++) {
    Rgb scaledColor = scaleColor(playerColor, brightness);
    strip.setPixelColor(i, scaledColor);
  }
  
  delay(PULSE_SPEED_MS);  // 50ms
}
```

#### Spin (Running Light)

```cpp
void updateSpinAnimation() {
  static uint16_t position = 0;
  
  // Clear all
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, BLACK);
  }
  
  // Light up current position
  strip.setPixelColor(position % LED_COUNT, playerColor);
  
  position++;
  delay(SPIN_SPEED_MS);  // 60ms
}
```

#### Rainbow

```cpp
void updateRainbowAnimation() {
  static uint16_t hue = 0;
  
  for (int i = 0; i < LED_COUNT; i++) {
    // Offset hue per LED for rainbow effect
    uint16_t pixelHue = hue + (i * 65536L / LED_COUNT);
    strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
  }
  
  hue += 256;  // Advance rainbow
  delay(20);
}
```

### Brightness Management

**Global brightness** is critical for battery operation:

```cpp
constexpr uint8_t LED_BRIGHTNESS = 64; // 25% of maximum

// Set at initialization
strip.setBrightness(LED_BRIGHTNESS);
```

**Why 64/255?**
- Prevents voltage sag (brownouts) under load
- Extends battery life significantly
- Reduces heat generation
- Still plenty bright in indoor lighting

---

## Button Logic

### Button Timing

Defined in `config.h`:

```cpp
constexpr uint16_t DEBOUNCE_MS = 20;
constexpr uint16_t SHORT_PRESS_MAX_MS = 600;
constexpr uint16_t LONG_PRESS_MS = 1200;
constexpr uint16_t VERY_LONG_PRESS_MS = 4000;
```

### Press Detection Algorithm

Using the **Bounce2** library for debouncing:

```cpp
Bounce button = Bounce();
button.attach(BUTTON_PIN);
button.interval(DEBOUNCE_MS);

void loop() {
  button.update();
  
  static uint32_t pressStartTime = 0;
  static bool longPressHandled = false;
  
  // Button pressed (falling edge)
  if (button.fell()) {
    pressStartTime = millis();
    longPressHandled = false;
  }
  
  // Button held (check for long/very long press)
  if (button.read() == LOW && pressStartTime > 0) {
    uint32_t pressDuration = millis() - pressStartTime;
    
    // Very long press (4+ seconds) - only once
    if (!longPressHandled && pressDuration >= VERY_LONG_PRESS_MS) {
      handleVeryLongPress();
      longPressHandled = true;
    }
    // Long press (1.2+ seconds) - only once
    else if (!longPressHandled && pressDuration >= LONG_PRESS_MS) {
      handleLongPress();
      longPressHandled = true;
    }
  }
  
  // Button released (rising edge)
  if (button.rose()) {
    if (!longPressHandled) {
      // Short press (< 600ms)
      handleShortPress();
    }
    pressStartTime = 0;
    longPressHandled = false;
  }
}
```

### Server Button Actions

| Press Type | Phase | Action |
|------------|-------|--------|
| SHORT | LOBBY | → READY (if ≥ MIN_CLIENTS) |
| SHORT | READY | → OPEN (start question) |
| SHORT | OPEN | (No action, wait for buzz) |
| SHORT | ANSWER | Next client in queue |
| LONG | ANSWER | Correct answer → RESET → READY |
| VERY_LONG | Any | Emergency reset → LOBBY (unlock joins) |

### Client Button Actions

| Press Type | Phase | Action |
|------------|-------|--------|
| ANY | OPEN | Send BUZZ message (if not already buzzed) |
| ANY | Other | (No action, button disabled) |

**Important:** Client button only functional during OPEN phase. This prevents accidental buzzes.

---

## Edge Cases & Error Handling

### 1. Simultaneous Buzzes

**Problem:** Multiple clients buzz at nearly the same time.

**Solution:** Server uses a multi-level tiebreaker:

```cpp
// Pseudocode for buzz ordering
struct BuzzEvent {
  String clientId;
  uint32_t serverReceiveTime;  // Server's millis()
  uint32_t clientTimestamp;    // Client's millis() from message
};

void insertBuzzInOrder(BuzzEvent buzz) {
  // Primary: Server receive time (most accurate)
  // Secondary: Client timestamp (for sub-millisecond tie)
  // Tertiary: Client ID (alphabetical, deterministic)
  
  for (int i = 0; i < queueSize; i++) {
    if (buzz.serverReceiveTime < queue[i].serverReceiveTime) {
      // Insert here
    }
    else if (buzz.serverReceiveTime == queue[i].serverReceiveTime) {
      // Same server time, check client timestamp
      if (buzz.clientTimestamp < queue[i].clientTimestamp) {
        // Insert here
      }
      else if (buzz.clientTimestamp == queue[i].clientTimestamp) {
        // Still tied, use alphabetical client ID
        if (buzz.clientId < queue[i].clientId) {
          // Insert here
        }
      }
    }
  }
}
```

### 2. Duplicate Buzzes (Spam Prevention)

**Problem:** Client spams button, sends multiple BUZZ messages.

**Solution:** Server tracks which clients have already buzzed:

```cpp
struct ClientInfo {
  String id;
  uint8_t slot;
  Rgb color;
  bool hasBuzzed;  // Per-question flag
  uint32_t lastPingTime;
};

void onBuzzMessage(String clientId, uint32_t timestamp) {
  // Find client
  ClientInfo* client = findClient(clientId);
  
  // Already buzzed this question?
  if (client->hasBuzzed) {
    return;  // Ignore duplicate
  }
  
  // Mark as buzzed
  client->hasBuzzed = true;
  
  // Add to queue
  addToQueue(clientId, timestamp);
}

void resetForNewQuestion() {
  // Clear all hasBuzzed flags
  for (auto& client : clients) {
    client.hasBuzzed = false;
  }
  queue.clear();
}
```

### 3. Client Disconnection

**Problem:** Client loses power or WiFi connection during game.

**Solution:** Multiple layers of detection:

#### Server-Side Detection

```cpp
constexpr uint16_t CLIENT_TIMEOUT_MS = 10000;

void checkClientTimeouts() {
  uint32_t now = millis();
  
  for (auto& client : clients) {
    if (now - client.lastPingTime > CLIENT_TIMEOUT_MS) {
      // Client timed out
      handleClientDisconnect(client.id);
    }
  }
}

void handleClientDisconnect(String clientId) {
  // Remove from queue if present
  removeFromQueue(clientId);
  
  // Free up color slot
  freeSlot(clientId);
  
  // If was active client, advance to next
  if (activeClient == clientId) {
    nextActiveClient();
  }
  
  // Mark as disconnected
  client.state = DISCONNECTED;
  
  // Publish updated queue (without this client)
  publishQueue();
}
```

#### Client-Side Reconnection

```cpp
void loop() {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    reconnectWiFi();
    return;
  }
  
  // Check MQTT connection
  if (!mqtt.connected()) {
    reconnectMQTT();
    return;
  }
  
  mqtt.loop();
}

void reconnectMQTT() {
  // Try to reconnect
  if (mqtt.connect(clientId.c_str())) {
    // Reconnected! Re-subscribe to topics
    mqtt.subscribe("quiz/state");
    mqtt.subscribe("quiz/cmd");
    mqtt.subscribe(("quiz/assign/" + clientId).c_str());
    
    // Re-send JOIN message (server will reassign same color if available)
    publishJoin();
  }
}
```

### 4. Late Joiner (During Active Game)

**Problem:** Client tries to join while game is in progress (phase ≠ LOBBY).

**Solution:** Server locks joins when leaving LOBBY:

```cpp
void onJoinMessage(String clientId) {
  // Check if joins are locked
  if (currentPhase != Phase::LOBBY) {
    // Send rejection or simply ignore
    // Client will stay in CONNECTING state with red pulsing
    return;
  }
  
  // Find free slot
  int slot = findFreeSlot();
  if (slot == -1) {
    // No slots available (10 clients already)
    return;
  }
  
  // Assign color
  assignClientToSlot(clientId, slot);
}
```

**User experience:** Late joiner sees red pulsing (not connected). Must wait for next game reset or emergency VERY_LONG press to unlock.

### 5. Queue Exhaustion

**Problem:** All clients in queue give wrong answers.

**Solution:** Server continues cycling through queue:

```cpp
void nextActiveClient() {
  activeIndex++;
  
  if (activeIndex >= queueSize) {
    // Wrapped around - all clients had a turn
    // Options:
    // 1. Reset question (no one got it right)
    // 2. Loop back to first client (give second chance)
    // 3. Wait for Quiz Master to manually reset
    
    // Current implementation: Stay at last client, wait for LONG press
    activeIndex = queueSize - 1;
  }
  
  publishActiveClient(queue[activeIndex]);
}
```

### 6. Server Crash/Reboot

**Problem:** Server ESP32 crashes or is power-cycled during game.

**Current behavior:**
- All clients lose connection (will show red pulsing)
- Server reboots to LOBBY state
- Clients auto-reconnect and get reassigned (possibly different slots)
- Game state is lost

**Future improvement:** Could add state persistence to NVS (ESP32 non-volatile storage):

```cpp
// Save to NVS on state change
void saveGameState() {
  Preferences prefs;
  prefs.begin("quiz", false);
  prefs.putUInt("phase", (uint8_t)currentPhase);
  prefs.putUInt("clientCount", clientCount);
  // ... save client assignments, queue, etc.
  prefs.end();
}

// Restore on boot
void restoreGameState() {
  Preferences prefs;
  prefs.begin("quiz", true);  // read-only
  if (prefs.isKey("phase")) {
    currentPhase = (Phase)prefs.getUInt("phase");
    clientCount = prefs.getUInt("clientCount");
    // ... restore full state
  }
  prefs.end();
}
```

---

## Code Structure

### Project Organization

```
PR-ESP-QB/
├── include/
│   ├── config.h              # Hardware & timing config
│   ├── protocol.h            # MQTT topics, states, enums
│   ├── mqtt_server.h         # Server MQTT broker wrapper
│   ├── game_manager.h        # Server game logic
│   ├── led_controller.h      # Server LED control
│   ├── client_mqtt.h         # Client MQTT wrapper
│   ├── client_manager.h      # Client state management
│   └── client_led_controller.h  # Client LED control
├── src/
│   ├── server_main.cpp       # Server entry point
│   ├── mqtt_server.cpp       # MQTT broker implementation
│   ├── game_manager.cpp      # Game state machine
│   ├── led_controller.cpp    # Server LED animations
│   ├── client_main.cpp       # Client entry point
│   ├── client_mqtt.cpp       # MQTT client implementation
│   ├── client_manager.cpp    # Client state machine
│   └── client_led_controller.cpp  # Client LED animations
└── platformio.ini            # Build configuration
```

### Key Classes

#### Server: GameManager

Manages game state and logic:

```cpp
class GameManager {
  public:
    void begin();
    void update();
    void handleButtonPress(ButtonPress type);
    void onClientJoin(const String& clientId);
    void onClientBuzz(const String& clientId, uint32_t timestamp);
    void onClientPing(const String& clientId);
    
  private:
    Phase currentPhase;
    ClientInfo clients[MAX_CLIENTS];
    String queue[MAX_CLIENTS];
    int8_t activeIndex;
    
    void transitionTo(Phase newPhase);
    void publishState();
    void publishQueue();
    void assignClientSlot(const String& clientId);
    void removeFromQueue(const String& clientId);
};
```

#### Server: LEDController

Manages server LED animations:

```cpp
class LEDController {
  public:
    void begin();
    void update();
    void setPhaseAnimation(Phase phase);
    void setActiveClient(Rgb color);
    void showQueue(const ClientInfo* clients, const String* queue, uint8_t queueSize);
    
  private:
    Adafruit_NeoPixel strip;
    AnimationType currentAnimation;
    void updateAnimation();
};
```

#### Client: ClientManager

Manages client state:

```cpp
class ClientManager {
  public:
    void begin();
    void update();
    void onAssignment(Rgb color, uint8_t slot);
    void onStateChange(Phase serverPhase);
    void onCommand(const String& cmd);
    void handleButtonPress();
    
  private:
    ClientState currentState;
    Rgb myColor;
    uint8_t mySlot;
    bool hasBuzzed;
    
    void transitionTo(ClientState newState);
    void sendBuzz();
};
```

#### Client: ClientLEDController

Manages client LED animations:

```cpp
class ClientLEDController {
  public:
    void begin();
    void update();
    void setState(ClientState state, Rgb color);
    
  private:
    Adafruit_NeoPixel strip;
    AnimationType currentAnimation;
    Rgb currentColor;
    void updateAnimation();
};
```

### Compilation Flow

```bash
# PlatformIO uses build flags to differentiate:

[env:server]
build_src_filter = +<server_main.cpp> +<mqtt_server.cpp> +<game_manager.cpp> +<led_controller.cpp>
build_flags = -DSERVER

[env:client]
build_src_filter = +<client_main.cpp> +<client_mqtt.cpp> +<client_manager.cpp> +<client_led_controller.cpp>
```

**Conditional compilation** in `config.h`:

```cpp
#ifdef SERVER
  constexpr uint16_t LED_COUNT = 18;
  constexpr uint8_t NEXT_BUTTON_PIN = 19;
  constexpr uint8_t CORRECT_BUTTON_PIN = 21;
#else
  constexpr uint16_t LED_COUNT = 8;
#endif
```

---

## Performance & Optimization

### Memory Usage

ESP32 has plenty of RAM for this application:

| Component | Server | Client |
|-----------|--------|--------|
| **ESP32 RAM** | 320 KB | 320 KB |
| **Program (Flash)** | ~200 KB | ~150 KB |
| **Global Variables** | ~5 KB | ~2 KB |
| **Stack** | ~8 KB | ~8 KB |
| **Heap (free)** | ~250 KB | ~250 KB |

**No dynamic allocation needed** - all arrays are fixed size (MAX_CLIENTS = 10).

### CPU Usage

**Server:**
- Idle: ~10% (WiFi AP, MQTT broker)
- Active game: ~30% (LED updates, MQTT handling)

**Client:**
- Idle: ~5% (WiFi STA, MQTT client)
- Active animation: ~15% (LED updates)

**LED update rate:** ~20-60ms per frame (depending on animation)

### Network Bandwidth

**Typical message sizes:**
- JOIN: ~50 bytes
- ASSIGN: ~40 bytes
- STATE: ~30 bytes
- BUZZ: ~35 bytes
- QUEUE: ~150 bytes (10 clients)
- PING: ~30 bytes

**Peak bandwidth scenario:**
- 10 clients buzzing simultaneously
- 10 × 35 bytes = 350 bytes in <100ms
- ~3.5 KB/s burst (well within WiFi capacity)

**Steady-state bandwidth:**
- Ping every 5s × 10 clients = 60 bytes/s
- Negligible

### Battery Life Estimates

**Server (8000mAh @ 1200mA average):**
- ~6.7 hours continuous operation
- Typically used 2-3 hours at event (plenty of margin)

**Client (1800mAh @ 600mA average):**
- ~3 hours continuous operation
- Event usage: 2-3 hours (charge beforehand)

**Tips to extend battery:**
- Reduce LED_BRIGHTNESS (trade visibility for runtime)
- Increase PING_INTERVAL (trade responsiveness for power)
- Dim or off LEDs during long idle periods

---

## Testing & Validation

### Unit Testing

**Test each component independently:**

1. **LED Test:** Upload firmware, verify all LEDs cycle through colors
2. **Button Test:** Press button, check serial monitor for press types
3. **WiFi Test:** Verify server creates AP, client connects
4. **MQTT Test:** Check broker accepts connections, messages published/received

### Integration Testing

**Test full system with 2-3 clients:**

1. **Join Test:**
   - Power on server → LOBBY
   - Power on clients → Receive colors
   - Verify each client shows unique color

2. **Buzz Test:**
   - Server: READY → OPEN
   - Clients: Buzz in known order (A, B, C)
   - Verify server queue shows correct order
   - Verify server LEDs 9-11 show A, B, C colors

3. **Wrong Answer Test:**
   - From ANSWER phase
   - Server button SHORT
   - Verify next client becomes active
   - Verify previous client returns to LOCKED

4. **Correct Answer Test:**
   - From ANSWER phase
   - Server button LONG
   - Verify celebration animations
   - Verify return to READY phase
   - Verify queue cleared

5. **Reconnect Test:**
   - During LOBBY: Power off client → power on → verify reconnects with same color
   - During game: Power off client → verify removed from queue → power on → verify stays disconnected until reset

### Stress Testing

**Test with maximum clients (10):**

1. **Simultaneous Buzz:**
   - All 10 clients buzz at same time
   - Verify queue shows all 10 in order
   - Cycle through all 10 answers

2. **Rapid Phase Changes:**
   - Quickly cycle LOBBY → READY → OPEN → ANSWER → RESET
   - Verify clients track correctly

3. **Marathon Test:**
   - Run continuously for 2+ hours
   - Monitor for memory leaks (check free heap)
   - Monitor for connection drops

4. **Range Test:**
   - Move clients progressively farther from server
   - Note effective range (typically 10-20m indoors)

### Validation Checklist

**Before deployment:**

- [ ] All server LEDs functional (18 LEDs)
- [ ] All client LEDs functional (8 LEDs each)
- [ ] Server creates WiFi AP "QUIZ-HUB"
- [ ] Clients connect and receive colors
- [ ] Button presses detected (all devices)
- [ ] Buzz order accurate with 3+ clients
- [ ] Queue display accurate on server
- [ ] Correct answer triggers celebration
- [ ] Wrong answer advances queue
- [ ] Reconnection works (power cycle client)
- [ ] Battery life sufficient (charge all devices)
- [ ] Enclosures secure (all screws tight)
- [ ] Power switches accessible

---

## Debugging Tips

### Serial Monitor

**Essential for debugging!** Connect via USB:

```bash
pio device monitor --environment server  # 115200 baud
pio device monitor --environment client
```

**What to look for:**

**Server:**
```
[BOOT] Quiz Server v1.0
[WIFI] AP started: QUIZ-HUB
[MQTT] Broker started on port 1883
[GAME] Phase: LOBBY
[CLIENT] Join: C-A1B2C3D4
[GAME] Assigned slot 1 (Red) to C-A1B2C3D4
[BUZZ] C-A1B2C3D4 buzzed at 12345ms
[QUEUE] Order: C-A1B2C3D4, C-E5F6G7H8
```

**Client:**
```
[BOOT] Quiz Client v1.0
[WIFI] Connecting to QUIZ-HUB...
[WIFI] Connected! IP: 192.168.4.2
[MQTT] Connecting to 192.168.4.1:1883...
[MQTT] Connected!
[ASSIGN] Slot 1, Color: #FF0000
[STATE] Server phase: OPEN
[BUZZ] Sent buzz at 12345ms
```

### Common Issues

**"Brownout detector was triggered"**
- Power supply insufficient
- Reduce LED_BRIGHTNESS
- Check battery voltage (should be >3.7V)
- Verify capacitor installed

**"WiFi: No AP found"**
- Server not powered on
- Check SSID spelling
- Server WiFi may have crashed → restart server

**"MQTT: Connection refused"**
- Server broker not running
- Check server serial monitor for broker status
- Verify server IP (should be 192.168.4.1)

**"LEDs flickering/wrong colors"**
- Check data line connection
- Verify 470Ω resistor installed
- Check 1000µF capacitor polarity
- Reduce brightness

**"Button not responding"**
- Check GPIO 18 connection
- Verify INPUT_PULLUP enabled
- Test with multimeter (should read 3.3V when not pressed, 0V when pressed)

---

## Future Enhancements

### Phase 2 Features (Potential)

1. **Web Interface:**
   - Server hosts web UI (192.168.4.1)
   - Monitor game state on phone/tablet
   - Remote control (start/stop, reset)

2. **Score Tracking:**
   - Keep points per client/team
   - Display rankings on server LEDs
   - JSON export for external scoring

3. **Sound Effects:**
   - Add buzzer/speaker to clients
   - Buzz sound on button press
   - Celebration sound on correct answer

4. **Advanced Animations:**
   - More LED patterns (chase, sparkle, etc.)
   - Customizable colors per event
   - Team-based color schemes

5. **Statistics:**
   - Track buzz times (reaction speed)
   - Log question history
   - SD card logging

6. **Power Optimizations:**
   - Sleep modes when idle
   - Automatic brightness adjustment
   - Low battery warnings

---

## Conclusion

This technical specification provides a complete reference for understanding, building, and extending the ESP32 Quiz-Buzzer System.

**Key Takeaways:**

✅ **Simple architecture:** Server-client with MQTT  
✅ **Robust protocol:** Handles disconnections, ties, spam  
✅ **Unified hardware:** Same pins for server & client  
✅ **Rich feedback:** LED animations for every state  
✅ **Event-ready:** Tested with wedding quiz buffet games  

For additional details, see:
- [Wiring Guide](WIRING_GUIDE.md) - Hardware assembly
- [User Manual](USER_MANUAL.md) - Operating instructions
- [Code Comments](../src/) - Implementation details

**Happy hacking! 🎮🔧✨**

# ESP32 Quiz‑Buzzer (Server & Clients)

**Ziel:** Ein ESP32 als *Quiz‑Master (Server)* mit 18 WS2812B‑LEDs und Taster; mehrere *Clients* (2–10) mit je 8 WS2812B‑LEDs und Taster. Verbindung via WLAN direkt zum Server. Der Server weist jedem Client eine eindeutige Farbe zu, steuert den Spielablauf (Buzz‑Reihenfolge, Freigabe/Nachsperren) und zeigt den Status mit LEDs an. Pro Frage dürfen keine neuen Clients joinen; nach einem Reset geht es mit der bestehenden Runde weiter.

---

## 1) Hardware & Aufbau

### 1.1 Stückliste (pro Gerät)
- **ESP32 DevKitC** (oder gleichwertig)
- **WS2812B LED‑Ring**
  - Server: 18 LEDs (Position 1–8 = aktiver Spieler‑Block, 9–18 = Reihenfolge‑Anzeige)
  - Client: 8 LEDs als Ring
- **Taster** (momentary, NO) + 10 kΩ Widerstand (falls externer Pull‑up/‑down)
- **Netzteil 5 V ≥ 2 A** (Server), **5 V ≥ 1 A** (Client)
- **Kleinkram:** 330–470 Ω **Serienwiderstand** in der Datenleitung, **1000 µF Elko** (5 V ↔ GND) nahe der LED‑Versorgung, saubere Masseführung
- Optional: **Level‑Shifter** 3,3 V → 5 V für WS2812B‑Data (oft geht es direkt, Shifter erhöht Robustheit)

### 1.2 Verdrahtung (Empfehlung)
- **LED_DATA** → Server: GPIO **5** | Client: GPIO **14**
- **BUTTON** → Server: GPIO **18** | Client: GPIO **27**
- **GND** von **ESP <-> LEDs <-> Netzteil** sternförmig verbinden
- **5 V** der LEDs direkt vom Netzteil; **ESP32** ebenfalls aus 5 V versorgen (on‑board Regler), **nicht** über USB im Produktivbetrieb
- **Schutz:** Serienwiderstand in Data, Elko an 5 V, Ein‑/Ausschaltstrom beachten

> Interne Pull‑ups verwenden (INPUT_PULLUP) und Taster gegen **GND** schalten. Entprellen in Software (siehe Bibliothek).

---

## 2) LED‑Farben & Layout

### 2.1 Eindeutige Spielerfarben (10 Slots)
Weiß wird **nicht** vergeben (reserviert für „bereits gedrückt“). Helligkeit global dimmen (z. B. 64/255).

| Slot | Name        | HEX      | RGB       |
|-----:|-------------|----------|-----------|
| 1    | Rot         | #FF3B30  | 255,59,48 |
| 2    | Blau        | #007AFF  | 0,122,255 |
| 3    | Grün        | #34C759  | 52,199,89 |
| 4    | Gelb        | #FFCC00  | 255,204,0 |
| 5    | Magenta     | #FF2D55  | 255,45,85 |
| 6    | Cyan        | #32ADE6  | 50,173,230|
| 7    | Orange      | #FF9500  | 255,149,0 |
| 8    | Lila        | #AF52DE  | 175,82,222|
| 9    | Türkis      | #1ABC9C  | 26,188,156|
| 10   | Indigo      | #5856D6  | 88,86,214 |

### 2.2 LED‑Zuordnung Server
- **LED 1–8:** Farbe des aktuell antwortenden Clients (Blockanzeige, z. B. „breathing“)
- **LED 9–18:** Buzz‑Reihenfolge der laufenden Frage
  - LED 9 = Schnellster, LED 10 = Zweitschnellster, …, LED 18 = 10. Platz
  - Farbe = Spielerfarbe; freie Plätze = dunkel

### 2.3 LED‑Ring Client (8 LEDs)
- Normalzustand: Spielerfarbe (ruhig, leichtes Pulsieren)
- Nach Buzz: **weiß** (gesperrt)
- Wenn „an der Reihe“: schnelle Kreis‑Animation in Spielerfarbe

---

## 3) Netzwerk & MQTT

### 3.1 Architektur
- **Server = WLAN‑Access‑Point & MQTT‑Broker** (SoftAP)
  - SSID: `QUIZ-HUB` | Passwort: `quiz12345` (anpassen)
  - Statische IP: `192.168.4.1`
  - MQTT‑Broker Port: `1883` (embedded Broker z. B. *uMQTTBroker*)
- **Clients = WLAN‑Stations** verbinden sich zum Server‑AP und als MQTT‑Clients zum Broker

> Falls Embedded‑Broker Probleme macht: Fallback ist möglich (UDP‑Protokoll oder externer Broker). Für dieses Projekt planen wir **MQTT on‑device**.

### 3.2 MQTT‑Topic‑Design (Namespace `quiz/`)
- `quiz/announce` (Server → alle, Retained): `{ "version":"1.0", "maxClients":10, "locked":false }`
- `quiz/join` (Client → Server): `{ "id":"C-<chip>", "cap":8, "fw":"x.y" }`
- `quiz/assign/<clientId>` (Server → Client): `{ "color":"#RRGGBB", "slot":1..10 }`
- `quiz/state` (Server → alle): `{ "phase":"LOBBY|READY|OPEN|ANSWER|REVIEW|RESET", "locked":bool }`
- `quiz/buzz` (Client → Server, **QoS 1**): `{ "id":"C-<chip>", "t":<millis> }`
- `quiz/queue` (Server → alle): `{ "order":["C-abc","C-def",...], "active":"C-abc" }`
- `quiz/cmd` (Server → alle): `{ "cmd":"LIGHT_WHITE|ANIM_ACTIVE|IDLE_COLOR|CELEBRATE|WRONG_FLASH" }`
- `quiz/ping` (Client ↔ Server): Healthcheck

**QoS:** Für `buzz` **QoS 1**, Retained = **false**; für Status/Anzeigen **QoS 0**, Retained je nach Bedarf (z. B. `announce`).

### 3.3 Verbindungsablauf
1. Client verbindet WLAN → sendet `quiz/join`
2. Server weist **Farbe/Slot** zu → `quiz/assign/<clientId>`; Client speichert und zeigt **Spielerfarbe**
3. Sobald **≥2 Clients** verbunden, kann Server‑Taster **kurz** gedrückt werden → `phase=OPEN`; Join wird **gesperrt** (`locked=true`)
4. Clients dürfen jetzt buzzern (`quiz/buzz`)
5. Server bildet **Queue** (erste 10) nach Empfangszeit; bei Gleichzeitigkeit tiebreak per `t` oder `clientId`
6. Server setzt `active = first(queue)` → zeigt **LED 1–8** in aktiver Spielerfarbe; **LED 9–18** zeigen Reihenfolge
7. **Falsche Antwort**: Server‑Taster **kurz** → `active = next(queue)`; Visuals aktualisieren
8. **Richtige Antwort**: Server‑Taster **lang** → `phase=RESET`, kurze Celebrations; danach `phase=READY|OPEN` für nächste Frage (Join weiterhin gesperrt)

---

## 4) Zustandsmaschine

### 4.1 Server‑States
- **BOOT** → **LOBBY** (AP+Broker aktiv, wartet auf ≥2 Clients)
- **LOBBY** (locked=false) → Button kurz: **READY** (locked=true)
- **READY** → Button kurz: **OPEN** (Frage offen; Buzz erlaubt)
- **OPEN** → bei erstem Buzz: **ANSWER** (aktiv gesetzt)
- **ANSWER** →
  - Button **kurz**: nächster in Queue → weiter **ANSWER**
  - Button **lang**: **REVIEW/RESET** → Queue löschen → zurück **READY** (oder **OPEN**, je nach Wunsch)

### 4.2 Client‑States
- **BOOT** → WLAN + MQTT → **ASSIGNED** (Farbe erhalten)
- **IDLE** (eigene Farbe, soft pulse)
- **LOCKED_AFTER_BUZZ** (weiß)
- **ACTIVE_TURN** (schneller Spin in Spielerfarbe)
- **CELEBRATE** (korrekt)
- **WRONG_FLASH** (kurzes Rot/Weiß)

---

## 5) Animationen (Vorschläge)
- **IDLE_PULSE:** sanftes Atmen in Spielerfarbe (Clients)
- **ACTIVE_SPIN:** schneller Lauflicht‑Spin (Clients)
- **LOCKED_WHITE:** alle LEDs weiß (Clients, nach Buzz)
- **WRONG_FLASH:** 2× kurzes Weiß‑/Aus‑Blinken (Client aktiv)
- **CELEBRATE:** 2 s Rainbow‑Lauf (Client aktiv) + Server LED 1–8 Rainbow
- **SERVER_ACTIVE_BREATH:** LED 1–8 „breathing“ in aktiver Farbe
- **SERVER_QUEUE_SOLID:** LED 9–18 in Queue‑Farben, statisch

---

## 6) Taster‑Logik (Server)
- **Kurz** (< 600 ms):
  - In **LOBBY**: Start (→ READY, locked=true)
  - In **READY**: Frage öffnen (→ OPEN)
  - In **ANSWER**: Nächster Kandidat
- **Lang** (≥ 1.2 s): In **ANSWER**: Richtige Antwort → Reset/Next Question
- **Sehr lang** (≥ 4 s): Admin: Lobby wieder **entsperren** (locked=false), neue Clients dürfen joinen (optional)

Entprellung: 10–30 ms; „press‑and‑hold“ mit Zeitstempel messen.

---

## 7) Edge‑Cases
- **Doppelte Buzzes / Spam:** Pro Frage nur der **erste** Buzz pro Client wird akzeptiert; danach Client → **weiß**
- **Zeitgleichheit:** Server‑Empfangszeit zuerst; bei Gleichstand `t` vergleichen; Fallback: alphabetisch nach `clientId`
- **Disconnect:** Entferne Client aus Queue; Color‑Slot wieder freigeben, aber in aktueller Runde **nicht** neu vergeben
- **Max 10 Clients:** Bei 11. Join → `locked=true` oder „Room full“ reply

---

## 8) PlatformIO – Projektstruktur

```
quiz-buzzer/
├─ platformio.ini
├─ include/
│  ├─ config.h            // Pins, Farben, Timings
│  └─ protocol.h          // Topics, JSON‑Keys, States
├─ src/
│  ├─ server_main.cpp     // Quiz‑Hub (AP+Broker)
│  └─ client_main.cpp     // Buzzer‑Client
└─ lib/
   └─ (optional Animations)
```

### 8.1 `platformio.ini` (Beispiel)
```ini
[platformio]
default_envs = server

[env]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
lib_deps =
  adafruit/Adafruit NeoPixel @ ^1.12.0
  thomasfredericks/Bounce2 @ ^2.72
  bblanchon/ArduinoJson @ ^6.21.4
  # MQTT Client (für Clients):
  knolleary/PubSubClient @ ^2.8
  # MQTT Broker (für Server, bitte passende Bibliothek hinzufügen, z. B.):
  # WalterH/uMQTTBroker (ESP32)

[env:server]
build_src_filter = +<server_main.cpp>

[env:client]
build_src_filter = +<client_main.cpp>
```

> Hinweis: Für den **Broker** bitte eine kompatible MQTT‑Broker‑Lib für ESP32 einbinden (z. B. *uMQTTBroker*). Alternativ: Externer Broker oder UDP‑Fallback.

---

## 9) Gemeinsame Header

### 9.1 `include/config.h`
```cpp
#pragma once
#include <Arduino.h>

// Pins
#ifdef SERVER
  constexpr uint8_t LED_PIN = 5;
  constexpr uint8_t BUTTON_PIN = 18;
  constexpr uint16_t LED_COUNT = 18;
#else
  constexpr uint8_t LED_PIN = 14;
  constexpr uint8_t BUTTON_PIN = 27;
  constexpr uint16_t LED_COUNT = 8;
#endif

// Brightness
constexpr uint8_t LED_BRIGHTNESS = 64; // 0..255

// Button timings (ms)
constexpr uint16_t DEBOUNCE_MS = 20;
constexpr uint16_t LONG_PRESS_MS = 1200;
constexpr uint16_t VERY_LONG_PRESS_MS = 4000;

// WiFi/AP
constexpr char WIFI_SSID[] = "QUIZ-HUB";
constexpr char WIFI_PSK[]  = "quiz12345"; // ändern
constexpr IPAddress AP_IP(192,168,4,1);

// MQTT
constexpr char MQTT_HOST[] = "192.168.4.1";
constexpr uint16_t MQTT_PORT = 1883;

// Farben (RGB)
struct Rgb { uint8_t r,g,b; };
constexpr Rgb PLAYER_COLORS[10] = {
  {255,59,48}, {0,122,255}, {52,199,89}, {255,204,0}, {255,45,85},
  {50,173,230}, {255,149,0}, {175,82,222}, {26,188,156}, {88,86,214}
};
```

### 9.2 `include/protocol.h`
```cpp
#pragma once
namespace Topic {
  constexpr auto ANNOUNCE = "quiz/announce";
  constexpr auto JOIN     = "quiz/join";
  constexpr auto STATE    = "quiz/state";
  constexpr auto BUZZ     = "quiz/buzz";
  constexpr auto QUEUE    = "quiz/queue";
  constexpr auto CMD      = "quiz/cmd";
}

enum class Phase { BOOT, LOBBY, READY, OPEN, ANSWER, RESET };
```

---

## 10) Server – Code‑Skelett (`src/server_main.cpp`)

> Zeigt die Struktur (Pseudo‑/Beispielcode). MQTT‑Broker‑API je nach Bibliothek anpassen.

```cpp
#define SERVER 1
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Bounce2.h>
#include <ArduinoJson.h>
#include "config.h"
#include "protocol.h"

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
Bounce btn = Bounce();

// ---- Spielzustand ----
struct ClientInfo { String id; uint8_t slot; Rgb color; bool buzzed=false; };
ClientInfo clients[10];
uint8_t clientCount=0;
String queueOrder[10];
int8_t activeIndex=-1; // Index in queueOrder
Phase phase = Phase::LOBBY;
bool locked = false;

// TODO: MQTT Broker init/start
void startBroker() { /* broker.begin(MQTT_PORT) */ }

void setup() {
  Serial.begin(115200);
  strip.begin(); strip.setBrightness(LED_BRIGHTNESS); strip.show();
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  btn.attach(BUTTON_PIN); btn.interval(DEBOUNCE_MS);

  // WiFi SoftAP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_SSID, WIFI_PSK);
  WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255,255,255,0));

  startBroker();
  // broker.publishRetained(Topic::ANNOUNCE, ...)
  phase = Phase::LOBBY; locked=false;
}

void setServerLedsActiveColor(const Rgb &c) {
  for (uint8_t i=0;i<8;i++) strip.setPixelColor(i, strip.Color(c.r,c.g,c.b));
  strip.show();
}

void paintQueueLeds() {
  for (uint8_t i=0;i<10;i++) {
    uint16_t led = 8 + i; // 9..18
    if (i < clientCount && queueOrder[i].length()) {
      // lookup color by client id
      for (uint8_t k=0;k<clientCount;k++) if (clients[k].id==queueOrder[i]) {
        auto c = clients[k].color; strip.setPixelColor(led, strip.Color(c.r,c.g,c.b));
      }
    } else {
      strip.setPixelColor(led, 0);
    }
  }
  strip.show();
}

void nextActive() {
  if (activeIndex+1 < 10 && queueOrder[activeIndex+1].length()) {
    activeIndex++;
    // Farbe setzen
    for (uint8_t k=0;k<clientCount;k++) if (clients[k].id==queueOrder[activeIndex]) {
      setServerLedsActiveColor(clients[k].color);
      break;
    }
  }
}

void onBuzzMessage(const String &clientId, uint32_t t) {
  if (phase != Phase::OPEN) return;
  // Schon in Queue? überspringen
  for (auto &q : queueOrder) if (q==clientId) return;
  // Einfügen
  for (uint8_t i=0;i<10;i++) if (!queueOrder[i].length()) { queueOrder[i]=clientId; break; }
  paintQueueLeds();
  if (activeIndex==-1) { // erster Buzz
    activeIndex= -1; phase = Phase::ANSWER; nextActive();
  }
}

void shortPress() {
  if (phase==Phase::LOBBY && clientCount>=2) { phase=Phase::READY; locked=true; /* publish state */ }
  else if (phase==Phase::READY) { phase=Phase::OPEN; /* publish state */ }
  else if (phase==Phase::ANSWER) { nextActive(); /* publish queue */ }
}

void longPress() {
  if (phase==Phase::ANSWER) {
    // richtige Antwort → Celebration + Reset der Frage
    // TODO: CMD CELEBRATE an active Client
    memset(queueOrder, 0, sizeof(queueOrder));
    activeIndex=-1; paintQueueLeds();
    phase=Phase::READY; /* publish state */
  }
}

void loop() {
  btn.update();
  static uint32_t pressedAt=0; static bool held=false;
  if (btn.fell()) { pressedAt=millis(); held=false; }
  if (btn.read()==LOW && pressedAt && !held && millis()-pressedAt>LONG_PRESS_MS) { held=true; longPress(); }
  if (btn.rose()) { if (!held) shortPress(); pressedAt=0; held=false; }

  // TODO: handle MQTT messages (join, buzz, etc.)
}
```

---

## 11) Client – Code‑Skelett (`src/client_main.cpp`)

```cpp
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Bounce2.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"
#include "protocol.h"

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
Bounce btn = Bounce();
WiFiClient wifi;
PubSubClient mqtt(wifi);

String clientId;
Rgb myColor{0,0,0};
bool assigned=false, hasBuzzed=false, isActive=false;

void ledsSolid(Rgb c){ for(uint16_t i=0;i<LED_COUNT;i++) strip.setPixelColor(i, strip.Color(c.r,c.g,c.b)); strip.show(); }
void ledsWhite(){ for(uint16_t i=0;i<LED_COUNT;i++) strip.setPixelColor(i, strip.Color(255,255,255)); strip.show(); }

void spinActive(){ static uint16_t pos=0; for(uint16_t i=0;i<LED_COUNT;i++) strip.setPixelColor(i,0); strip.setPixelColor(pos%LED_COUNT, strip.Color(myColor.r,myColor.g,myColor.b)); strip.show(); pos++; }

void onMqtt(char* topic, byte* payload, unsigned int len){
  // parse assign, state, cmd; set flags/colors
}

void connectWiFi(){ WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID, WIFI_PSK); while(WiFi.status()!=WL_CONNECTED) delay(100); }
void connectMQTT(){ mqtt.setServer(MQTT_HOST, MQTT_PORT); mqtt.setCallback(onMqtt); while(!mqtt.connected()){ mqtt.connect(clientId.c_str()); delay(250);} }

void setup(){
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP); btn.attach(BUTTON_PIN); btn.interval(DEBOUNCE_MS);
  strip.begin(); strip.setBrightness(LED_BRIGHTNESS); strip.show();
  clientId = String("C-") + String((uint32_t)ESP.getEfuseMac(), HEX);
  connectWiFi(); connectMQTT();
  // publish join
}

void loop(){
  mqtt.loop(); btn.update();
  if (btn.fell() && !hasBuzzed && assigned) {
    // buzz
    hasBuzzed=true; ledsWhite();
    // publish buzz with millis()
  }
  if (isActive) { spinActive(); delay(60); } else { delay(10); }
}
```

---

## 12) Testplan
1. **AP + Broker** startet, `announce` retained
2. 2× Clients verbinden, **Farben** korrekt zugewiesen
3. **Start** (Server kurz) → `OPEN` → Buzz‑Queue entsteht
4. **Anzeige** Server LED 1–8 = aktiv; 9–18 = Reihenfolge
5. **Kurz** → nächster Kandidat; **Lang** → Celebration + Reset
6. **Rejoin gesperrt** während Runde; Entsperren per sehr langem Druck (optional)

---

## 13) Tipps & Robustheit
- Globales **Brightness‑Limit** (64/255) gegen Brownouts
- **Watchdog/Auto‑Reconnect** bei WLAN/MQTT‑Abbrüchen
- **Persistenz** (NVS) optional für Slot/Farbe, falls Client neu startet
- **Serial‑Logs** mit JSON‑Kurzform für Debug

---

## 14) Nächste Schritte
- Bibliothek für **MQTT‑Broker** final auswählen und in `server_main.cpp` integrieren
- JSON‑Payloads konkret implementieren (Parser/Builder)
- Animations‑Funktionen ausbauen (separate `Animations.h/.cpp`)
- Optional: Web‑UI am Server (Status/Lock/Reset)

> Wenn du möchtest, erstelle ich dir daraus **ein lauffähiges PlatformIO‑Starterprojekt** (Server & Client, kompilierbar) inkl. minimaler MQTT‑Broker‑Integration und Beispiel‑Animationen.


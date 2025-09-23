#pragma once
#include <Arduino.h>
#include <PicoMQTT.h>
#include "config.h"
#include "protocol.h"

// Game Client Structure
struct ClientInfo {
  String id;
  uint8_t slot;
  Rgb color;
  bool connected;
  bool buzzed;
  uint32_t lastSeen;
};

// Custom MQTT Broker class
class QuizMQTTBroker : public PicoMQTT::Server {
public:
  void on_connected(const char * client_id) override;
  void on_disconnected(const char * client_id) override;
};

// MQTT Message Handlers
void handleClientJoin(const String& payload);
void handleClientBuzz(const String& payload);
void handleClientPing(const String& payload);

// MQTT Publishers
void sendClientAssignment(const String& clientId, uint8_t slot, const Rgb& color);
void publishGameState();
void publishBuzzQueue();
void publishAnnounce();

// Client Timeout Management
void checkClientTimeouts();

// Global variables (declared in mqtt_server.cpp)
extern QuizMQTTBroker mqttBroker;
extern ClientInfo gameClients[MAX_CLIENTS];
extern uint8_t gameClientCount;
extern String buzzQueue[MAX_CLIENTS];
extern uint8_t queueLength;
extern int8_t activeClientIndex;
extern Phase currentPhase;
extern bool gameLocked;

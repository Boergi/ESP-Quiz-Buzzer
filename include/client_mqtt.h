#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "protocol.h"

// Client MQTT Manager
class ClientMQTT {
private:
  WiFiClient wifiClient;
  PubSubClient mqttClient;
  String clientId;
  bool connected;
  uint32_t lastConnectionAttempt;
  uint32_t lastPing;
  
public:
  ClientMQTT();
  
  // Connection management
  void begin();
  void loop();
  bool isConnected();
  
  // WiFi functions
  bool connectWiFi();
  void disconnectWiFi();
  
  // MQTT functions
  bool connectMQTT();
  void disconnectMQTT();
  void onMessage(char* topic, byte* payload, unsigned int length);
  
  // Game communication
  void sendJoinRequest();
  void sendBuzz();
  void sendPing();
  
  // Getters
  const String& getClientId() const;
};

// MQTT Message handlers
void handleAssignment(const String& payload);
void handleGameState(const String& payload);
void handleQueue(const String& payload);
void handleCommand(const String& payload);

// Global MQTT client instance
extern ClientMQTT* clientMqtt;

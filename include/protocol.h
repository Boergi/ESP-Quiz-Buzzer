#pragma once

// MQTT Topic Namespace: quiz/
namespace Topic {
  constexpr auto ANNOUNCE = "quiz/announce";
  constexpr auto JOIN = "quiz/join";
  constexpr auto ASSIGN = "quiz/assign/";  // + clientId
  constexpr auto STATE = "quiz/state";
  constexpr auto BUZZ = "quiz/buzz";
  constexpr auto QUEUE = "quiz/queue";
  constexpr auto CMD = "quiz/cmd";
  constexpr auto PING = "quiz/ping";
}

// Game State Phases
enum class Phase : uint8_t {
  BOOT = 0,
  LOBBY,    // Waiting for clients, join allowed
  READY,    // Min clients reached, locked, ready for question
  OPEN,     // Question open, buzz allowed
  ANSWER,   // Someone buzzed, processing answers
  RESET     // Question finished, celebrating/resetting
};

// Client States
enum class ClientState : uint8_t {
  DISCONNECTED = 0,
  CONNECTING,
  ASSIGNED,           // Got color assignment
  IDLE,              // Waiting for question
  LOCKED_AFTER_BUZZ, // Buzzed, showing white
  ACTIVE_TURN,       // Currently answering
  CELEBRATE,         // Got correct answer
  WRONG_FLASH        // Got wrong answer
};

// Button Press Types
enum class ButtonPress : uint8_t {
  NONE = 0,
  SHORT,      // < 600ms
  LONG,       // >= 1200ms
  VERY_LONG   // >= 4000ms
};

// LED Animation Types
enum class AnimationType : uint8_t {
  SOLID = 0,
  PULSE,
  SPIN,
  FLASH,
  RAINBOW,
  OFF
};

// MQTT Command Types (sent via quiz/cmd)
namespace Command {
  constexpr auto LIGHT_WHITE = "LIGHT_WHITE";
  constexpr auto ANIM_ACTIVE = "ANIM_ACTIVE";
  constexpr auto IDLE_COLOR = "IDLE_COLOR";
  constexpr auto CELEBRATE = "CELEBRATE";
  constexpr auto WRONG_FLASH = "WRONG_FLASH";
  constexpr auto RESET = "RESET";
}

// JSON Message Keys
namespace JsonKey {
  // Common
  constexpr auto ID = "id";
  constexpr auto VERSION = "version";
  constexpr auto TIMESTAMP = "t";
  
  // Announce
  constexpr auto MAX_CLIENTS = "maxClients";
  constexpr auto LOCKED = "locked";
  
  // Join
  constexpr auto CAPABILITY = "cap";
  constexpr auto FIRMWARE = "fw";
  
  // Assign
  constexpr auto COLOR = "color";
  constexpr auto SLOT = "slot";
  
  // State
  constexpr auto PHASE = "phase";
  
  // Queue
  constexpr auto ORDER = "order";
  constexpr auto ACTIVE = "active";
  
  // Command
  constexpr auto CMD = "cmd";
  constexpr auto TARGET = "target";
}

// Protocol Version
constexpr auto PROTOCOL_VERSION = "1.0";

// Utility Functions for Phase Names
inline const char* phaseToString(Phase phase) {
  switch(phase) {
    case Phase::BOOT: return "BOOT";
    case Phase::LOBBY: return "LOBBY";
    case Phase::READY: return "READY";
    case Phase::OPEN: return "OPEN";
    case Phase::ANSWER: return "ANSWER";
    case Phase::RESET: return "RESET";
    default: return "UNKNOWN";
  }
}

inline Phase stringToPhase(const char* str) {
  if (strcmp(str, "BOOT") == 0) return Phase::BOOT;
  if (strcmp(str, "LOBBY") == 0) return Phase::LOBBY;
  if (strcmp(str, "READY") == 0) return Phase::READY;
  if (strcmp(str, "OPEN") == 0) return Phase::OPEN;
  if (strcmp(str, "ANSWER") == 0) return Phase::ANSWER;
  if (strcmp(str, "RESET") == 0) return Phase::RESET;
  return Phase::BOOT;
}


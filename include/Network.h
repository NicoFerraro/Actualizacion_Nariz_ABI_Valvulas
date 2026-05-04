#pragma once

#include <Arduino.h>

// Compatibility shim for Ethernet libraries that expect the newer
// Arduino-ESP32 Network API. Our current core only exposes tcpipInit().
extern void tcpipInit();

class NetworkClient;
class NetworkServer;

class NetworkClass {
 public:
  bool begin() {
    tcpipInit();
    return true;
  }
};

extern NetworkClass Network;

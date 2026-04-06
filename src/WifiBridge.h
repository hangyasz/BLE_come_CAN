#pragma once

#include <Arduino.h>
#include <WiFi.h>

#include "config.h"

class WifiBridge {
public:
    bool start();
    void tick();
    bool isStarted() const;
    String buildStartResponse() const;

private:
    bool _started = false;
    WiFiServer _server{WIFI_TCP_PORT};
    WiFiClient _client;
    uint32_t _lastHelloSentMs = 0;
};

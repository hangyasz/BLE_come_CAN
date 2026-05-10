#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "driver/twai.h"


#include "Config.h"

class WifiBridge {
public:
    bool start();
    void stop();
    void tick();
    bool isStarted() const;
    bool isClientConnected() ;
    String buildStartResponse() const;


private:
    bool _started = false;
    WiFiServer _server{WIFI_TCP_PORT};
    WiFiClient _client;
    uint32_t _lastHelloSentMs = 0;
    uint32_t _lastClientActivityMs = 0;
    String _password;  // Az aktuális random jelszó
    
    String generateRandomPassword(int length = 12);
};

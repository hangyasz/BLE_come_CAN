#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "driver/twai.h"


#include "Config.h"

class WifiBridge {
public:
    // WiFi AP indítás/leállítás és időzített feldolgozás.
    String start();
    void stop();
    void tick();

    // Aktív állapot és klienskapcsolat lekérdezése.
    bool isStarted() const;
    bool isClientConnected() ;

    // BLE felé visszaküldhető indulási válasz.
    String buildStartResponse() const;


private:
    // Az AP futási állapota.
    bool _started = false;
    // A TCP szerver, amin a telefon csatlakozik a CAN adatokhoz.
    WiFiServer _server{WIFI_TCP_PORT};
    // Aktív kliens kapcsolat.
    WiFiClient _client;
    uint32_t _lastHelloSentMs = 0;
    uint32_t _lastClientActivityMs = 0;
    String _password;  // Az aktuális random jelszó
    
    // Véletlen jelszó generálása az AP-hez.
    String generateRandomPassword(int length = 12);
};

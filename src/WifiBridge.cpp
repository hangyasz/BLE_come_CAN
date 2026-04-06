#include "WifiBridge.h"

bool WifiBridge::start() {
    if (_started) return true;

    WiFi.mode(WIFI_AP);
    if (!WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD)) {
        Serial.println("[WIFI] softAP start failed");
        return false;
    }

    _server.begin();
    _server.setNoDelay(true);
    _lastHelloSentMs = 0;
    _started = true;

    Serial.printf("[WIFI] AP started: ssid=%s ip=%s port=%d\n",
                  WIFI_AP_SSID,
                  WiFi.softAPIP().toString().c_str(),
                  WIFI_TCP_PORT);
    return true;
}

void WifiBridge::tick() {
    if (!_started) return;

    if (!_client || !_client.connected()) {
        WiFiClient incoming = _server.available();
        if (incoming) {
            if (_client) _client.stop();
            _client = incoming;
            _lastHelloSentMs = 0;
            Serial.printf("[WIFI] TCP client connected: %s\n",
                          _client.remoteIP().toString().c_str());
        }
    }

    if (_client && _client.connected() &&
        (uint32_t)(millis() - _lastHelloSentMs) >= WIFI_HELLO_INTERVAL_MS) {
        _client.print(WIFI_HELLO_MESSAGE);
        _lastHelloSentMs = millis();
    }
    if(_client && !_client.connected()) {
        Serial.println("[WIFI] TCP client disconnected");
        _client.stop();
    }
}

bool WifiBridge::isStarted() const {
    return _started;
}

String WifiBridge::buildStartResponse() const {
    String response = "OK:WIFI:";
    response += WIFI_AP_SSID;
    response += ":";
    response += WIFI_AP_PASSWORD;
    response += ":";
    response += WiFi.softAPIP().toString();
    response += ":";
    response += String(WIFI_TCP_PORT);
    return response;
}

#include "WifiBridge.h"

bool WifiBridge::start()
{
    if (_started)
        return true;

    WiFi.mode(WIFI_AP);
    if (!WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD))
    {
        Serial.println("[WIFI] softAP start failed");
        return false;
    }

    _server.begin();
    _server.setNoDelay(true);
    _lastHelloSentMs = 0;
    _lastClientActivityMs = millis();
    _started = true;

    Serial.printf("[WIFI] AP started: ssid=%s ip=%s port=%d\n",
                  WIFI_AP_SSID,
                  WiFi.softAPIP().toString().c_str(),
                  WIFI_TCP_PORT);
    return true;
}

void WifiBridge::stop()
{
    if (!_started)
        return;

    if (_client && _client.connected())
    {
        _client.stop();
    }
    _server.stop();
    WiFi.mode(WIFI_OFF);
    _started = false;
    _lastHelloSentMs = 0;
    Serial.println("[WIFI] AP stopped");
}

void WifiBridge::tick()
{
    if (!_started)
        return;

    if (!_client || !_client.connected()) {
        WiFiClient incoming = _server.available();
        if (incoming) {
            if (_client) _client.stop();
            _client = incoming;
            _lastHelloSentMs = 0;
            _lastClientActivityMs = millis();
            Serial.printf("[WIFI] TCP client connected: %s\n",
                          _client.remoteIP().toString().c_str());
        }

        if ((uint32_t)(millis() - _lastClientActivityMs) >= WIFI_InACTIVITY_TIMEOUT_MS) {
            Serial.println("[WIFI] Client inactivity timeout → disconnecting");
            stop();
        }
        return;
    }

    _lastClientActivityMs = millis();

    // Client is connected: send periodic hello
    if ((uint32_t)(millis() - _lastHelloSentMs) >= WIFI_HELLO_INTERVAL_MS)
    {
        _client.print(WIFI_HELLO_MESSAGE);
        _lastHelloSentMs = millis();
    }
}

bool WifiBridge::isClientConnected()
{
    return _started && _client && _client.connected();
}

bool WifiBridge::isStarted() const
{
    return _started;
}

String WifiBridge::buildStartResponse() const
{
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

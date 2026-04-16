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
    
    twai_message_t message;

while 
 (twai_receive(&message, pdMS_TO_TICKS(0)) == ESP_OK) {
    Serial.print("ID: 0x");
    Serial.print(message.identifier, HEX);

    Serial.print(message.extd ? " EXT" : " STD");

    Serial.print(" DLC: ");
    Serial.print(message.data_length_code);

    Serial.print(" DATA: ");
    for (int i = 0; i < message.data_length_code; i++) {
        Serial.printf("%02X ", message.data[i]);
    }

    Serial.println();
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

#include "WifiBridge.h"
#include <random>

String WifiBridge::generateRandomPassword(int length)
{
    const char* charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    String password = "";
    
    for (int i = 0; i < length; i++) {
        int randomIndex = random(0, strlen(charset));
        password += charset[randomIndex];
    }
    
    return password;
}

bool WifiBridge::start()
{
    if (_started)
        return true;

    WiFi.mode(WIFI_AP);
    
    _password = generateRandomPassword(12);

    if (!WiFi.softAP(WIFI_AP_SSID, _password.c_str(), 1, 1, 1))
    {
        Serial.println("[WIFI] softAP start failed");
        return false;
    }

    _server.begin();
    _server.setNoDelay(true);
    _lastHelloSentMs = 0;
    _lastClientActivityMs = millis();
    _started = true;

    Serial.printf("[WIFI] AP started: ssid=%s password=%s ip=%s port=%d\n",
                  WIFI_AP_SSID,
                  _password.c_str(),
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
            _client.setNoDelay(true); // ✅ Nagyon fontos a gyorsasághoz!
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
    
    // ✅ ÚJ: Kód a CAN Queue kiolvasására és bináris továbbítására
    extern QueueHandle_t canRxQueue; // Hivatkozunk a globális sorra
    if (canRxQueue != NULL && uxQueueMessagesWaiting(canRxQueue) > 0) {
        twai_message_t msg;
        uint8_t txBuffer[512]; // Kb. 36 CAN üzenet fér ide egyszerre
        int offset = 0;

        while (uxQueueMessagesWaiting(canRxQueue) > 0 && offset <= (sizeof(txBuffer) - 14)) {
            if (xQueueReceive(canRxQueue, &msg, 0) == pdTRUE) {
                txBuffer[offset++] = 0xAA; // Start byte
                
                // CAN ID (4 byte)
                txBuffer[offset++] = (msg.identifier & 0xFF);
                txBuffer[offset++] = ((msg.identifier >> 8) & 0xFF);
                txBuffer[offset++] = ((msg.identifier >> 16) & 0xFF);
                txBuffer[offset++] = ((msg.identifier >> 24) & 0xFF);
                // DLC (1 byte)
                txBuffer[offset++] = msg.data_length_code;
                
                // DATA (8 byte)
                for (int i = 0; i < 8; i++) {
                    txBuffer[offset++] = (i < msg.data_length_code) ? msg.data[i] : 0x00;
                }

        //write can data to serial for debug
            /* erial.printf("[WIFI] CAN üzenet kiolvasva: ID=0x%X DLC=%d\n", msg.identifier, msg.data_length_code);

                Serial.print(" DATA: ");
                for (int i = 0; i < msg.data_length_code; i++) {
                    Serial.printf("%02X ", msg.data[i]);
                }
                Serial.println(); */
            }
        }

        // Egyben elküldjük az összes összegyűjtött üzenetet a telefonnak
        if (offset > 0) {
            _client.write(txBuffer, offset);
        }
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
    response += _password;  // Az aktuális random jelszó
    response += ":";
    response += WiFi.softAPIP().toString();
    response += ":";
    response += String(WIFI_TCP_PORT);
    return response;
}

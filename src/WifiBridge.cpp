#include "WifiBridge.h"
#include <random>

//Random jelszó generálása az AP-hez
String WifiBridge::generateRandomPassword(int length)
{
    // Csak egyszerű, olvasható karakterekből álló jelszót generálunk.
    const char* charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    String password = "";
    
    for (int i = 0; i < length; i++) {
        int randomIndex = random(0, strlen(charset));
        password += charset[randomIndex];
    }
    
    return password;
}

// WiFi AP indítása 
String WifiBridge::start()
{
    // Ha már fut az AP, az aktuális paramétereket adjuk vissza.
    if (_started)
        return buildStartResponse();

    // WiFi csak AP módban indul.
    WiFi.mode(WIFI_AP);
    
    _password = generateRandomPassword(12);

    // AP indítása: SSID, jelszó, csatorna és rejtés beállítása és 1 eszköz engedélyezése.
    if (!WiFi.softAP(WIFI_AP_SSID, _password.c_str(), 1, 0, 1))
    {
        Serial.println("[WIFI] softAP start failed");
        return "";
    }

    // TCP szerver indítása a WiFi fölött.
    _server.begin();
    _server.setNoDelay(true);
    _lastHelloSentMs = 0;
    _lastClientActivityMs = millis();
    _started = true;

    // Kimenő log: könnyen ellenőrizhető, milyen AP jött létre.
    Serial.printf("[WIFI] AP started: ssid=%s password=%s ip=%s port=%d\n",
                  WIFI_AP_SSID,
                  _password.c_str(),
                  WiFi.softAPIP().toString().c_str(),
                  WIFI_TCP_PORT);

    return buildStartResponse();
}

void WifiBridge::stop()
{
    // Ha nem fut, nincs teendő.
    if (!_started)
        return;

    // Aktív kliens bontása, majd a szerver leállítása.
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
    // Csak aktív AP mellett van értelme a feldolgozásnak.
    if (!_started)
        return;

    // Ha még nincs kliens, várunk csatlakozásra és figyeljük az inaktivitást.
    if (!_client || !_client.connected()) {
        WiFiClient incoming = _server.available();
        if (incoming) {
            if (_client) _client.stop();
            _client = incoming;
            // Nincs TCP csomag-összevonás, gyorsabb átvitel.
            _client.setNoDelay(true);
            _lastHelloSentMs = 0;
            _lastClientActivityMs = millis();
            Serial.printf("[WIFI] TCP client connected: %s\n",
                          _client.remoteIP().toString().c_str());
        }

        // Ha túl sokáig nincs kliens aktivitás, lekapcsoljuk az AP-t.
        if ((uint32_t)(millis() - _lastClientActivityMs) >= WIFI_InACTIVITY_TIMEOUT_MS) {
            Serial.println("[WIFI] Client inactivity timeout → disconnecting");
            stop();
        }
        return;
    }

    // Aktív kliens esetén új CAN üzeneteket csomagolunk és továbbítunk.
    _lastClientActivityMs = millis();
    
    // A CAN receive queue-ból olvasunk, és bináris keretként küldjük tovább.
    extern QueueHandle_t canRxQueue;
    if (canRxQueue != NULL && uxQueueMessagesWaiting(canRxQueue) > 0) {
        twai_message_t msg;
        uint8_t txBuffer[512];
        int offset = 0;

        // Több CAN üzenetet is egy bufferbe fűzünk, amíg elférnek.
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

        // Debug log CAN adatokról, ha szükséges.
            /* erial.printf("[WIFI] CAN üzenet kiolvasva: ID=0x%X DLC=%d\n", msg.identifier, msg.data_length_code);

                Serial.print(" DATA: ");
                for (int i = 0; i < msg.data_length_code; i++) {
                    Serial.printf("%02X ", msg.data[i]);
                }
                Serial.println(); */
            }
        }

        // Az összegyűjtött kereteket egyben küldjük el.
        if (offset > 0) {
            _client.write(txBuffer, offset);
        }
    }
}


// Aktív klienskapcsolat lekérdezése: csak akkor igaz, ha az AP fut és van kapcsolódó kliens.
bool WifiBridge::isClientConnected()
{
    // Akkor aktív, ha az AP fut és a kliens is kapcsolódva van.
    return _started && _client && _client.connected();
}

// Az AP futási állapotának lekérdezése.
bool WifiBridge::isStarted() const
{
    return _started;
}

// BLE felé visszaküldhető indulási válasz összeállítása: SSID, jelszó, IP és port.
String WifiBridge::buildStartResponse() const
{
    // Ezt a választ küldjük vissza BLE-n a telefon számára.
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

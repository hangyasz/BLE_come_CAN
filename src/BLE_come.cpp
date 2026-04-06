#include "BLE_come.h"
#include "BleDevices.h"
#include "config.h"

// ═══════════════════════════════════════════════════════════
// INITIALIZATION
// ═══════════════════════════════════════════════════════════

void BLEManager::setDeviceRegistry(BleDevices* registry) {
    _registry = registry;
}

void BLEManager::init() {
    BLEDevice::init(BLE_DEVICE_NAME);
    BLEDevice::setSecurityCallbacks(this);
 
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(this);
 
    pService = pServer->createService(SERVICE_UUID);
 
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ  |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
    );

    // Add descriptors BEFORE starting service
    pCharacteristic->addDescriptor(new BLE2902());
    pCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
    pCharacteristic->setCallbacks(this);

    pServer->start();
    pService->start();

    // Start service AFTER all descriptor and permission setup
    advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(SERVICE_UUID);
    advertising->setScanResponse(false);
    advertising->setMinPreferred(0x0);

    _bleSecurity(); 
    delay(100);  // BLE stack stabilizálása
    _startAdvertising();
}

// ═══════════════════════════════════════════════════════════
// TICK
// ═══════════════════════════════════════════════════════════

void BLEManager::tick() {

    if (pendingAdvertisingRestart) {
        pendingAdvertisingRestart = false;
        _startAdvertising();
    }
    
    // Pairing window timeout (60s)
    if (pairingWindowOpen &&
        (uint32_t)(millis() - pairingWindowOpenedAtMs) >= PAIRING_WINDOW_MS) {
        pairingWindowOpen = false;
        pairingWindowOpenedAtMs = 0;
        Serial.println("[BLE] Pairing window expired");
    }

    // Name request timeout (10s)
    if (_waitingForName &&
        (uint32_t)(millis() - _authStateEnteredMs) >= NAME_REQUEST_TIMEOUT_MS) {
        Serial.println("[AUTH] Name request timeout → disconnecting");
        _abortPairing();
    }

    _wifiBridge.tick();
}

// ═══════════════════════════════════════════════════════════
// PUBLIC CONTROL
// ═══════════════════════════════════════════════════════════

void BLEManager::startBLE() {
    if (!advertising) return;

    // Check if registry is full
    if (_registry != nullptr && _registry->isFull()) {
        Serial.printf("[BLE] Max devices reached (%d) → pairing disabled\n", BLE_MAX_STORED);
        sendNotification("ERR:MAX_DEVICES");
        return;
    }

    Serial.println("[BLE] Opening pairing window...");

    // Disconnect any existing connections before pairing
    if (pServer->getConnectedCount() > 0) {
        std::map<uint16_t, conn_status_t> peers = pServer->getPeerDevices(true);
        for (auto const& peer : peers) {
            Serial.printf("[BLE] Disconnecting existing connection: conn_id=%d\n",
                          peer.first);
            pServer->disconnect(peer.first);
        }
        delay(300);
    }

    pairingWindowOpen = true;
    _waitingForName = false;

    // Open advertising for pairing
    advertising->setScanFilter(false, false);
    advertising->stop();
    delay(50);
    advertising->start();
    Serial.printf("[BLE] Pairing window opened (60s)\n");
    pairingWindowOpenedAtMs = millis();
}

void BLEManager::stopBLE() {
    advertising->stop();
    pairingWindowOpen = false;
    pairingWindowOpenedAtMs = 0;
    Serial.println("[BLE] Advertising stopped");
}

void BLEManager::clearBonds() {
    Serial.println("[BLE] Clearing all bonds and devices...");
    _waitingForName = false;
    if (_registry) _registry->clearAll();
}

void BLEManager::sendNotification(const String& message) {
    if (!pCharacteristic || !pServer) return;
    if (pServer->getConnectedCount() == 0) return;
    pCharacteristic->setValue(message.c_str());
    pCharacteristic->notify();
    Serial.printf("[BLE] → %s\n", message.c_str());
}

// ═══════════════════════════════════════════════════════════
// SERVER CALLBACKS
// ═══════════════════════════════════════════════════════════

void BLEManager::onConnect(BLEServer* pSrv) {
    this->pServer = pSrv;
    uint16_t connId = pSrv->getConnId();

    Serial.printf("[BLE] Connected: conn_id=%d active=%d\n", connId,
                  pSrv->getConnectedCount());

    // Pairing mode: only one device at a time
    if (pairingWindowOpen && pSrv->getConnectedCount() > 1) {
        Serial.println("[BLE] Pairing in progress → parallel connection rejected");
        pSrv->disconnect(connId);
        return;
    }

    // Normal mode: enforce max paired connections
    if (!pairingWindowOpen && (int)pSrv->getConnectedCount() > 1) {
        Serial.printf("[BLE] Max connections (%d) reached → rejecting\n",
                      BLE_MAX_PAIRED);
        pSrv->disconnect(connId);
        return;
    }

    connectionProcessRunning = true;}

void BLEManager::onDisconnect(BLEServer* pSrv) {
    this->pServer = pSrv;
    connectionProcessRunning = false;

    // Fix #2: bond törlés ELŐTT, amíg a MAC még érvényes
    if (_waitingForName) {
        Serial.println("[BLE] Pairing aborted → removing bond");
        esp_ble_remove_bond_device(_connectedMac);
        pairingWindowOpen = false;  // Fix #1: ide kerül
    }

    _waitingForName = false;
    memset(_connectedMac, 0, 6);

    // Fix #3: ESP32 quirk kompenzáció (a count 1-gyel nagyobb callbackben)
    uint32_t realCount = pSrv->getConnectedCount();
    if (realCount > 0) realCount--;
    Serial.printf("[BLE] Disconnected: conn_id=%d active=%d\n",
                  pSrv->getConnId(), realCount);

    pendingAdvertisingRestart = true;  // Restart advertising in tick() to avoid stack issues

}

// ═══════════════════════════════════════════════════════════
// SECURITY CALLBACKS
// ═══════════════════════════════════════════════════════════

uint32_t BLEManager::onPassKeyRequest() { return 0; }

void BLEManager::onPassKeyNotify(uint32_t key) {
    Serial.printf("[BLE] PIN: %06u\n", key);
}

bool BLEManager::onConfirmPIN(uint32_t) { return true; }

bool BLEManager::onSecurityRequest() { 
   return true;
}

void BLEManager::onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) {
    if (!cmpl.success) {
        Serial.println("[BLE] BLE authentication FAILED");
        _abortPairing();
        return;
    }

    Serial.printf("[BLE] BLE bond OK: %s\n", getMacString(cmpl.bd_addr).c_str());
    memcpy(_connectedMac, cmpl.bd_addr, 6);

    // Check if this is a known device
    if (_registry && _registry->containsMac(_connectedMac)) {
        // Known device → allow connection
        char name[33] = {};
        _registry->getName(_connectedMac, name, sizeof(name));
        Serial.printf("[AUTH] Known device: \"%s\" → OK\n", name);
        sendNotification("RECOGNIZED");  // TCP-like confirmation
        _waitingForName = false;
        return;
    }

    // New device in pairing window → request name
    if (!pairingWindowOpen) {
        Serial.println("[AUTH] Unknown device + pairing window closed → rejecting");
        _abortPairing();
        return;
    }

    // Request device name
    _waitingForName = true;
    sendNotification("REQUEST_NAME");  // TCP-like: must be confirmed
    Serial.println("[AUTH] New device detected → requesting name (with indication)");
    _authStateEnteredMs = millis();  // Start timeout BEFORE sending
}

// ═══════════════════════════════════════════════════════════
// CHARACTERISTIC CALLBACKS
// ═══════════════════════════════════════════════════════════

void BLEManager::onRead(BLECharacteristic*) {}

void BLEManager::onWrite(BLECharacteristic* pChar) {
    String value = pChar->getValue().c_str();
    value.trim();
    if (value.isEmpty()) return;

    Serial.printf("[BLE] ← Write: %s\n", value.c_str());

    

    if (value == "WIFI:START") {
        if (!_wifiBridge.start()) {
            sendNotification("ERR:WIFI_START");
            return;
        }

        sendNotification(_wifiBridge.buildStartResponse());
        return;
    }

    // Name response: N:DeviceName
    if (value.startsWith("N:") && _waitingForName) {
        String name = value.substring(2);
        name.trim();

        if (name.isEmpty() || name.length() > 32) {
            sendNotification("ERR:INVALID_NAME");
            if (pServer) pServer->disconnect(pServer->getConnId());
            return;
        }

        // Save to registry
        if (_registry) {
            _registry->addDevice(_connectedMac, name.c_str());
        }

        _waitingForName = false;
        pairingWindowOpen = false;

        sendNotification("OK:PAIRED:" + name);  // TCP-like confirmation
        Serial.printf("[BLE] Paired: %s\n", name.c_str());
        return;
    }

    // LIST command
    if (value == "LIST") {
        String resp = "DEVICES:";
        if (!_registry || _registry->count() == 0) {
            resp += "EMPTY";
        } else {
            bool first = true;
            for (const auto& d : _registry->getDevices()) {
                if (!first) resp += ";";
                resp += String(d.name) + "@" + getMacString(d.mac);
                first = false;
            }
        }
        sendNotification(resp);
        return;
    }

    // CLEAR command
    if (value == "CLEAR") {
        clearBonds();
        sendNotification("OK:CLEARED");
        return;
    }

    sendNotification("ERR:UNKNOWN");
}

// ═══════════════════════════════════════════════════════════
// PRIVATE HELPERS
// ═══════════════════════════════════════════════════════════

void BLEManager::_abortPairing() {
    esp_ble_remove_bond_device(_connectedMac);
    if (pServer && pServer->getConnectedCount() > 0) {
        pServer->disconnect(pServer->getConnId());
    }
    _waitingForName = false;
    memset(_connectedMac, 0, 6);
    pairingWindowOpen = false;
    connectionProcessRunning = false;
    pairingWindowOpenedAtMs = 0;
    _authStateEnteredMs = 0;
    Serial.println("[AUTH] Pairing aborted");
}

void BLEManager::_startAdvertising() {
    advertising->stop();        // ← kötelező!
    delay(200);                 // stack cleanup idő
    advertising->setScanFilter(false, false);
    if (advertising->start()) {
        Serial.println("[BLE] Advertising started");
    } else {
        Serial.println("[BLE] Advertising FAILED");
    }
}

void BLEManager::_bleSecurity() {
    BLESecurity::setAuthenticationMode(true, true, true);  // bond, mitm, sc
    BLESecurity::setCapability(ESP_IO_CAP_OUT);            // PIN display
    BLESecurity::setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    BLESecurity::setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    BLESecurity::setKeySize(16);
    BLESecurity::setPassKey(false);
    BLESecurity::regenPassKeyOnConnect(true);
    //esp_ble_gap_config_local_privacy(true);

    Serial.println("[BLE] Security: MITM + SC + BOND + Privacy");
}
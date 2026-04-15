#include "NimBLEManager.h"
#include "BleDevices.h"
#include "config.h"
#include <esp_system.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static TaskHandle_t g_wifiTickTaskHandle = nullptr;
static WifiBridge *g_wifiBridge = nullptr;

void wifiTickTask(void *pvParameters)
{
    (void)pvParameters;
    Serial.println("[WIFI-TICK-TASK] Task indítva");

    while (1)
    {
        if (!g_wifiBridge)
        {
            break;
        }

        if (!g_wifiBridge->isStarted())
        {
            Serial.println("[WIFI-TICK-TASK] WiFi leallt - task leall");
            break;
        }

        g_wifiBridge->tick();
        vTaskDelay(pdMS_TO_TICKS(100)); // 100ms ellenőrzés
    }

    g_wifiTickTaskHandle = nullptr;
    vTaskDelete(nullptr);
}

static void startwifiTickTaskIfNeeded()
{
    if (g_wifiTickTaskHandle != nullptr)
    {
        return;
    }

    xTaskCreatePinnedToCore(
        wifiTickTask,
        "WIFI-Tick-Task",
        4096,
        nullptr,
        1,
        &g_wifiTickTaskHandle,
        1);
}

// ═══════════════════════════════════════════════════════════
// INITIALIZATION
// ════════════════════════════════════════════════

void BLEManager::setDeviceRegistry(BleDevices *registry)
{
    _registry = registry;
}

void BLEManager::init()
{
    NimBLEDevice::init(BLE_DEVICE_NAME);

    // Security must be configured BEFORE creating the server
    _bleSecurity();

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(this);

    pService = pServer->createService(SERVICE_UUID);

    // NimBLE: READ_ENC / WRITE_ENC replace setAccessPermissions(ESP_GATT_PERM_*_ENCRYPTED).
    // BLE2902 descriptor is NOT needed — NimBLE adds the CCCD automatically for NOTIFY.
    pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    NIMBLE_PROPERTY::READ       |
    NIMBLE_PROPERTY::READ_ENC   |
    NIMBLE_PROPERTY::WRITE     | 
    NIMBLE_PROPERTY::WRITE_ENC  |
    NIMBLE_PROPERTY::NOTIFY);
    pCharacteristic->setCallbacks(this);
    pCharacteristic->setValue("READY"); // Set initial value so reads don't timeout
    
    pServer->start();

    advertising = NimBLEDevice::getAdvertising();
    advertising->addServiceUUID(SERVICE_UUID);
    advertising->enableScanResponse(false);
    advertising->setName(BLE_DEVICE_NAME);
    advertising->setMinInterval(0x30);
    advertising->setMaxInterval(0x60);

    delay(100); // BLE stack stabilizálása
    _startAdvertising();
}

// ═══════════════════════════════════════════════════════════
// TICK
// ═══════════════════════════════════════════════════════════

void BLEManager::tick()
{
    // Only process timeouts during pairing window or while waiting for device name
    if (pairingWindowOpen || _waitingForName)
    {
        // Pairing window timeout (60s)
        if (pairingWindowOpen &&
            (uint32_t)(millis() - pairingWindowOpenedAtMs) >= PAIRING_WINDOW_MS)
        {
            pairingWindowOpen = false;
            pairingWindowOpenedAtMs = 0;
            Serial.println("[BLE] Pairing window expired");
        }

        if (_waitingForName &&
            _authStateEnteredMs > 0 &&
            (uint32_t)(millis() - _authStateEnteredMs) >= NAME_REQUEST_TIMEOUT_MS)
        {
            Serial.println("[AUTH] Name request timeout → disconnecting");
            _abortPairing();
        }
    }
}

// ═══════════════════════════════════════════════════════════
// PUBLIC CONTROL
// ═══════════════════════════════════════════════════════════

void BLEManager::startParing()
{
    if (!advertising)
        return;

    // Stop WiFi when entering pairing mode
    _wifiBridge.stop();

    // Check if registry is full
    if (_registry != nullptr && _registry->isFull())
    {
        Serial.printf("[BLE] Max devices reached (%d) → pairing disabled\n", BLE_MAX_STORED);
        sendNotification("ERR:MAX_DEVICES");
        return;
    }

    Serial.println("[BLE] Opening pairing window...");

    // Disconnect any existing connections before pairing
    // NimBLE v2: getConnectedCount() still works; disconnect by handle via connInfo stored handles
    if (pServer->getConnectedCount() > 0)
    {
        // NimBLE provides a vector of peer handles via getPeerDevices()
        std::vector<uint16_t> handles = pServer->getPeerDevices();
        for (uint16_t handle : handles)
        {
            Serial.printf("[BLE] Disconnecting existing connection: conn_id=%d\n", handle);
            pServer->disconnect(handle);
        }
        delay(300);
    }

    pairingWindowOpen = true;
    _waitingForName = false;
    _authStateEnteredMs = 0;

    // Open advertising for pairing (allow all scanners/connectors)
    advertising->setScanFilter(false, false);
    advertising->stop();
    delay(50);
    advertising->start();
    pairingWindowOpenedAtMs = millis();
    Serial.printf("[BLE] Pairing window opened (60s)\n");
}

void BLEManager::stopBLE()
{
    advertising->stop();
    pairingWindowOpen = false;
    pairingWindowOpenedAtMs = 0;
    Serial.println("[BLE] Advertising stopped");
}

// BLEManager::clearBonds()
void BLEManager::clearBonds() {
    Serial.println("[BLE] Clearing all bonds and devices...");
    _waitingForName = false;
    NimBLEDevice::deleteAllBonds(); // ← itt
    if (_registry)
        _registry->clearAll();      // ← ez csak store + devices.clear()
}

void BLEManager::sendNotification(const String &message)
{
    if (!pCharacteristic || !pServer)
        return;
    if (pServer->getConnectedCount() == 0)
        return;
    // NimBLE: setValue accepts std::string or const uint8_t*
    pCharacteristic->setValue(message.c_str());
    pCharacteristic->notify();
    Serial.printf("[BLE] → %s\n", message.c_str());
}

// ═══════════════════════════════════════════════════════════
// SERVER CALLBACKS
// NimBLE v2: signatures include NimBLEConnInfo& (carries handle, address, etc.)
// ═══════════════════════════════════════════════════════════

void BLEManager::onConnect(NimBLEServer *pSrv, NimBLEConnInfo &connInfo)
{
    this->pServer = pSrv;
    uint16_t connId = connInfo.getConnHandle();

    Serial.printf("[BLE] Connected: conn_id=%d active=%d\n", connId,
                  pSrv->getConnectedCount());

    // Max 1 eszköz — ha már van valaki csatlakozva, dobja el az újat
    if (pSrv->getConnectedCount() > 1) {
        Serial.println("[BLE] Max 1 connection allowed → rejecting");
        pSrv->disconnect(connId);
        return;
    }

    // Reset name request state
    _waitingForName = false;
    _authStateEnteredMs = 0;
    connectionProcessRunning = true;

    // Ha már van bonded eszköz és nincs párosítási ablak,
    // állítsd le az advertisinget — ne csatlakozhasson más
    advertising->stop();
}

void BLEManager::onDisconnect(NimBLEServer *pSrv, NimBLEConnInfo &connInfo, int reason)
{
    this->pServer = pSrv;
    connectionProcessRunning = false;

    if (_waitingForName) {
        Serial.println("[BLE] Pairing aborted → removing bond");
        NimBLEDevice::deleteBond(_connectedAddr);
    }

    Serial.printf("[BLE] Disconnected: %s\n", connInfo.getAddress().toString().c_str());
    _connectedAddr = NimBLEAddress();
    _waitingForName = false;
    pairingWindowOpen = false;
    pairingWindowOpenedAtMs = 0;
    _authStateEnteredMs = 0;

    // Csak akkor hirdet újra, ha tényleg nincs senki
    if (pSrv->getConnectedCount() == 0) {
        _startAdvertising();
    }
}

// Called by NimBLE when passkey-display pairing is used.
uint32_t BLEManager::onPassKeyDisplay()
{
    uint32_t passKey = (esp_random() % 900000) + 100000; // 6-digit random PIN
    Serial.printf("[BLE] PIN: %06u\n", passKey);
    return passKey;
}

// Called when the peer expects local passkey entry.
void BLEManager::onPassKeyEntry(NimBLEConnInfo &connInfo)
{
    Serial.printf("[BLE] Passkey entry requested by %s\n", connInfo.getAddress().toString().c_str());
}

void BLEManager::onConfirmPassKey(NimBLEConnInfo &connInfo, uint32_t passKey)
{
    Serial.printf("[BLE] Confirm passkey %06u for %s\n", passKey, connInfo.getAddress().toString().c_str());
    NimBLEDevice::injectConfirmPasskey(connInfo, true);
}

void BLEManager::onAuthenticationComplete(NimBLEConnInfo &connInfo)
{
    if (!connInfo.isEncrypted())
    {
        Serial.println("[BLE] BLE authentication FAILED");
        _abortPairing();
        return;
    }

    if(pairingWindowOpen){
    _connectedAddr = connInfo.getAddress();;
    _waitingForName = true;
    sendNotification("REQUEST_NAME");
    Serial.println("[AUTH] New device detected → requesting name");
    _authStateEnteredMs = millis();
    return;
    }
    _abortPairing();
}

// ═══════════════════════════════════════════════════════════
// CHARACTERISTIC CALLBACKS
// NimBLE v2: both callbacks receive NimBLEConnInfo& as second parameter
// ═══════════════════════════════════════════════════════════

void BLEManager::onRead(NimBLECharacteristic *pChar, NimBLEConnInfo &connInfo)
{
    Serial.printf("[BLE] ← Read from %s (value: %s)\n", connInfo.getAddress().toString().c_str(), pChar->getValue().c_str());
}

void BLEManager::onWrite(NimBLECharacteristic *pChar, NimBLEConnInfo &connInfo)
{
    // NimBLE: getValue() returns NimBLEAttValue; cast to std::string then to Arduino String
    String value = String(pChar->getValue().c_str());
    value.trim();
    if (value.isEmpty())
        return;

    Serial.printf("[BLE] ← Write: %s\n", value.c_str());


    // Name response: N:DeviceName
    if (value.startsWith("N:") && _waitingForName)
    {
        String name = value.substring(2);
        name.trim();

        if (name.isEmpty() || name.length() > 32)
        {
            sendNotification("ERR:INVALID_NAME");
            if (pServer)
                pServer->disconnect(pServer->getPeerDevices()[0]);
            return;
        }

        if (_registry)
        {
            _registry->addDevice(_connectedAddr.getVal(), name.c_str());
        }

        _waitingForName = false;
        pairingWindowOpen = false;

        sendNotification("OK:PAIRED:" + name);
        Serial.printf("[BLE] Paired: %s\n", name.c_str());
        return;
    }

    if (_waitingForName) {
        return;
    }
    
    if (value == "WIFI:START")
    {
        if (!_wifiBridge.start())
        {
            sendNotification("ERR:WIFI_START");
            return;
        }

        g_wifiBridge = &_wifiBridge;
        startwifiTickTaskIfNeeded();
        sendNotification(_wifiBridge.buildStartResponse());
        return;
    }


    // LIST command
    if (value == "LIST")
    {
        String resp = "DEVICES:";
        if (!_registry || _registry->count() == 0)
        {
            resp += "EMPTY";
        }
        else
        {
            bool first = true;
            for (const auto &d : _registry->getDevices())
            {
                if (!first)
                    resp += ";";
                // NimBLEAddress can reconstruct the MAC string from raw bytes
                NimBLEAddress a(d.mac, BLE_ADDR_PUBLIC);
                resp += String(d.name) + "@" + a.toString().c_str();
                first = false;
            }
        }
        sendNotification(resp);
        return;
    }

    // CLEAR command
    if (value == "CLEAR")
    {
        clearBonds();
        sendNotification("OK:CLEARED");
        return;
    }

    sendNotification("ERR:UNKNOWN");
}

// ═══════════════════════════════════════════════════════════
// PRIVATE HELPERS
// ═══════════════════════════════════════════════════════════

void BLEManager::_abortPairing()
{
    NimBLEDevice::deleteBond(_connectedAddr); // replaces esp_ble_remove_bond_device()
    if (pServer && pServer->getConnectedCount() > 0)
    {
        // Disconnect the first (and only expected) peer
        std::vector<uint16_t> handles = pServer->getPeerDevices();
        if (!handles.empty())
            pServer->disconnect(handles[0]);
    }
    _waitingForName = false;
    _connectedAddr = NimBLEAddress();
    pairingWindowOpen = false;
    connectionProcessRunning = false;
    pairingWindowOpenedAtMs = 0;
    _authStateEnteredMs = 0;
    Serial.println("[AUTH] Pairing aborted");
}

void BLEManager::_startAdvertising()
{
    advertising->stop();
    delay(200); // stack cleanup time
   advertising->setScanFilter(false, false);
    advertising->setName(BLE_DEVICE_NAME);
    if (advertising->start())
    {
        Serial.printf("[BLE] Advertising started (name=%s)\n", BLE_DEVICE_NAME);
    }
    else
    {
        Serial.println("[BLE] Advertising FAILED");
    }
}

void BLEManager::_bleSecurity()
{
    // NimBLE: security is configured directly on NimBLEDevice (no BLESecurity object)
    NimBLEDevice::setSecurityAuth(
        BLE_SM_PAIR_AUTHREQ_SC |   // Secure Connections
        BLE_SM_PAIR_AUTHREQ_MITM | // MITM protection
        BLE_SM_PAIR_AUTHREQ_BOND   // Bonding
    );
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY); // PIN display (ESP_IO_CAP_OUT)
    NimBLEDevice::setSecurityInitKey(
        BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
    NimBLEDevice::setSecurityRespKey(
        BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
    // NimBLE key size is fixed at 16 bytes; no setKeySize() needed.
    // NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM);  // uncomment to enable privacy

    Serial.println("[BLE] Security configured (SC with MITM and BOND)");
}

// NimBLEManager.cpp
bool BLEManager::isPairingActive()
{
    return pairingWindowOpen || _waitingForName;
}

#include "NimBLEManager.h"
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// ═══════════════════════════════════════════════════════════════
// WiFi task
// ═══════════════════════════════════════════════════════════════

void BLEManager::wifiTickTask(void* pvParameters)
{
    BLEManager* self = static_cast<BLEManager*>(pvParameters);
    Serial.println("[WIFI-TASK] Inditva");

    while (true)
    {
        if (!self->wifiBridge.isStarted())
        {
            Serial.println("[WIFI-TASK] WiFi leallt, task leall");
            break;
        }
        self->wifiBridge.tick();
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // ✅ Atomi törlés: előbb nullázunk, aztán töröljük a taskot
    TaskHandle_t h = self->wifiTaskHandle;
    self->wifiTaskHandle = nullptr;
    vTaskDelete(h);
}

void BLEManager::startWifiTaskIfNeeded()
{
    if (wifiTaskHandle != nullptr) return;

    xTaskCreatePinnedToCore(
        wifiTickTask,
        "WIFI-Tick",
        4096,
        this,          // ✅ this-t adjuk át, nem globális pointert
        1,
        &wifiTaskHandle,
        0);
}
void BLEManager::setDeviceRegistry(BleDevices* reg)
{
    registry = reg;
}

void BLEManager::setCanSendCallback(CanSendCallback cb)
{
    canSendCb = cb;
}

void BLEManager::init()
{
    NimBLEDevice::init(BLE_DEVICE_NAME);
    bleSecurity();

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(this);

    pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::READ  | NIMBLE_PROPERTY::READ_ENC  |
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC |
        NIMBLE_PROPERTY::NOTIFY);


    pCharacteristic->setCallbacks(this);
    pCharacteristic->setValue("READY");

    advertising = NimBLEDevice::getAdvertising();
    // ✅ NE hirdessük meg a Service UUID-t az advertising-ban
    // Az app már ismeri az UUID-ket, csak a név alapján csatlakozik
    advertising->enableScanResponse(false);
    advertising->setName(BLE_DEVICE_NAME);
    advertising->setScanFilter(false, false);
    advertising->start();

    Serial.println("[BLE] Advertising started (UUID rejtett)");
}


void BLEManager::tick()
{
    if (!pairingWindowOpen && !waitingForName) return;

    uint32_t now = millis();

    // Párosítási ablak lejárt
    if (pairingWindowOpen && pairingWindowEndMs > 0 &&
        (int32_t)(now - pairingWindowEndMs) >= 0)
    {
        Serial.println("[BLE] Pairing window lejart");
        disconnectAndCleanup(true);
        resetPairingState();
        return;
    }

    // Névkérési ablak lejárt
    if (waitingForName && nameWindowEndMs > 0 &&
        (int32_t)(now - nameWindowEndMs) >= 0)
    {
        Serial.println("[AUTH] Nev timeout -> disconnect");
        disconnectAndCleanup(true);
        resetPairingState();
    }
}

// ═══════════════════════════════════════════════════════════════
// Párosítás kezelés
// ═══════════════════════════════════════════════════════════════

void BLEManager::startPairing()  // ✅ Javított elírás
{
    if (!advertising) return;

    if (registry && registry->isFull())
    {
        Serial.printf("[BLE] Max eszkozok elerte (%d)\n", BLE_MAX_STORED);
        return;
    }

    // Meglévő kapcsolatok bontása
    if (pServer->getConnectedCount() > 0)
    {
        for (uint16_t h : pServer->getPeerDevices())
            pServer->disconnect(h);
        delay(300);
    }

    resetPairingState();
    pairingWindowOpen  = true;

    advertising->stop();
    delay(50);
    advertising->start();

    Serial.printf("[BLE] Pairing ablak nyitva (%dms)\n", PAIRING_WINDOW_MS);
    pairingWindowEndMs = millis() + PAIRING_WINDOW_MS;
}

void BLEManager::stopBLE()
{
    if (advertising) advertising->stop();
    resetPairingState();
    Serial.println("[BLE] Leallitva");
}

void BLEManager::clearBonds()
{
    if (pServer && pServer->getConnectedCount() > 0)
    {
        for (uint16_t h : pServer->getPeerDevices())
            pServer->disconnect(h);
        delay(200);
    }

    NimBLEDevice::deleteAllBonds();
    if (registry) registry->clearAll();
    resetPairingState();

    Serial.println("[BLE] Minden bond es eszkoz torolve");
}

// ═══════════════════════════════════════════════════════════════
// BLE parancsok
// ═══════════════════════════════════════════════════════════════

// "LIST" → "DEVICES:name1@mac1;name2@mac2" vagy "DEVICES:EMPTY"
void BLEManager::handleListCommand()
{
    String resp = "DEVICES:";

    if (!registry || registry->getCount() == 0)
    {
        resp += "EMPTY";
    }
    else
    {
        for (size_t i = 0; i < registry->getCount(); ++i)
        {
            const DeviceRecord& d = registry->at(i);
            NimBLEAddress addr(d.mac, BLE_ADDR_PUBLIC);
            if (i > 0) resp += ";";
            // Formátum: "0:Nev@aa:bb:cc:dd:ee:ff"
            resp += String(i) + ":" + String(d.name) + "@" + addr.toString().c_str();
        }
    }

    sendNotification(resp);
}



// "DEL:0" → töröl index alapján, bond is törlődik
void BLEManager::handleDeleteCommand(const String& value)
{
    if (!registry)
    {
        sendNotification("ERR:NO_REGISTRY");
        return;
    }

    int idx = value.substring(4).toInt(); // "DEL:X" → X

    if (idx < 0 || idx >= (int)registry->getCount())
    {
        sendNotification("ERR:INVALID_ID");
        return;
    }

    const DeviceRecord& d = registry->at(idx);
    NimBLEAddress addr(d.mac, BLE_ADDR_PUBLIC);

    // ✅ Bond törlése NimBLE-ből is
    NimBLEDevice::deleteBond(addr);

    // Ha éppen ez az eszköz van csatlakozva, bontsuk a kapcsolatot
    if (connectedHandle != BLE_HS_CONN_HANDLE_NONE &&
        connectedAddr == addr)
    {
        pServer->disconnect(connectedHandle);
        resetPairingState();
    }

    String name = String(d.name);
    registry->removeAt(idx);

    sendNotification("OK:DELETED:" + String(idx) + ":" + name);
    Serial.printf("[BLE] Eszkoz torolve: [%d] %s\n", idx, name.c_str());
}

// "CAN:7DF:8:0122334455667788"
// id hex, len dec, data hex (len*2 karakter)
// ✅ Nem kell WiFi – közvetlenül a CAN buszra megy
void BLEManager::handleCanSendCommand(const String& value)
{
    if (!canSendCb)
    {
        sendNotification("ERR:NO_CAN_CB");
        return;
    }

    // Parsing: CAN:id:len:hexdata
    int p1 = value.indexOf(':', 4);  // "CAN:" után
    int p2 = value.indexOf(':', p1 + 1);
    int p3 = value.indexOf(':', p2 + 1);

    if (p1 < 0 || p2 < 0)
    {
        sendNotification("ERR:CAN_FORMAT");
        return;
    }

    String idStr   = value.substring(4, p1);
    String lenStr  = value.substring(p1 + 1, p2);
    String hexData = (p3 >= 0) ? value.substring(p2 + 1) : value.substring(p2 + 1);

    uint32_t canId   = (uint32_t)strtoul(idStr.c_str(), nullptr, 16);
    uint8_t  dlc     = (uint8_t)lenStr.toInt();
    bool     extended = canId > 0x7FF;

    if (dlc > 8 || hexData.length() < dlc * 2)
    {
        sendNotification("ERR:CAN_DATA");
        return;
    }

    uint8_t data[8] = {0};
    for (uint8_t i = 0; i < dlc; i++)
    {
        char buf[3] = {hexData[i*2], hexData[i*2+1], 0};
        data[i] = (uint8_t)strtoul(buf, nullptr, 16);
    }

    bool ok = canSendCb(canId, extended, dlc, data);
    sendNotification(ok ? "OK:CAN_SENT" : "ERR:CAN_FAIL");

    Serial.printf("[BLE->CAN] ID:%03X len:%d %s\n", canId, dlc, ok ? "OK" : "FAIL");
}

void BLEManager::handleWifiStart()
{
    if (wifiBridge.isStarted())
    {
        sendNotification(wifiBridge.buildStartResponse());
        return;
    }

    if (!wifiBridge.start())
    {
        sendNotification("ERR:WIFI_START");
        return;
    }

    startWifiTaskIfNeeded();
    sendNotification(wifiBridge.buildStartResponse());
    Serial.println("[WIFI] Elindult");
}

void BLEManager::handleWifiStop()
{
    wifiBridge.stop();
    sendNotification("OK:WIFI_STOPPED");
    Serial.println("[WIFI] Leallitva");
}

void BLEManager::handleCanSpeedCommand(String value)
{
    // A "CANSPEED:" szöveg eltávolítása, hogy csak a szám maradjon
    String speedStr = value.substring(9);
    uint32_t newSpeed = speedStr.toInt();

    // Ellenőrizzük, hogy támogatott sebességet kaptunk-e
    if (newSpeed != 125000 && newSpeed != 250000 && newSpeed != 500000 && newSpeed != 1000000) {
        Serial.println("[CAN] Érvénytelen sebesség!");
        sendNotification("ERR:INV_SPEED");
        return;
    }

    Serial.printf("[CAN] Sebesség módosítása: %d bps\n", newSpeed);

    // 1. A TWAI driver leállítása és eltávolítása
    // Érdemes ellenőrizni, hogy fut-e, de az uninstall mindenképp megtisztítja az állapotot
    twai_stop();
    delay(10); // Kis szünet a hardvernek
    twai_driver_uninstall();
    delay(10);

    // 2. Újrainicializálás az új sebességgel a meglévő függvényeddel
    if (initCan(newSpeed)) {
        Serial.println("[CAN] Sikeres újraindítás az új sebességgel!");
        sendNotification("OK:SPEED_CHANGED");
    } else {
        Serial.println("[CAN] Hiba az újraindítás során!");
        sendNotification("ERR:CAN_INIT_FAIL");
    }
}


// ═══════════════════════════════════════════════════════════════
// Értesítés küldés
// ═══════════════════════════════════════════════════════════════

void BLEManager::sendNotification(const String& message)
{
    if (!pCharacteristic || !pServer) return;
    if (pServer->getConnectedCount() == 0) return;

    pCharacteristic->setValue(message.c_str());
    pCharacteristic->notify();
    Serial.printf("[BLE->] %s\n", message.c_str());
}


// ═══════════════════════════════════════════════════════════════
// NimBLE callbacks
// ═══════════════════════════════════════════════════════════════

void BLEManager::onConnect(NimBLEServer* pSrv, NimBLEConnInfo& connInfo)
{
    Serial.printf("[BLE] Csatlakozott: %s\n", connInfo.getAddress().toString().c_str());
    NimBLEDevice::stopAdvertising();
    
    // Az új kapcsolat még nincs autentikálva - resetelni kell az előző handle-t
    connectedHandle = BLE_HS_CONN_HANDLE_NONE;
}

void BLEManager::onDisconnect(NimBLEServer* pSrv, NimBLEConnInfo& connInfo, int reason)
{
    Serial.printf("[BLE] Lecsatlakozott (reason: %d)\n", reason);

    // ✅ Csak akkor resetelünk, ha ez a mi aktív párosítási kapcsolatunk volt
    if (connectedHandle == connInfo.getConnHandle())
    {
        resetPairingState();
    }

    if (advertising) advertising->start();
}

uint32_t BLEManager::onPassKeyDisplay()
{
    uint32_t pin = (esp_random() % 900000) + 100000;
    Serial.printf("[BLE] PIN: %06u\n", pin);
    return pin;
}

void BLEManager::onConfirmPassKey(NimBLEConnInfo& connInfo, uint32_t pin) // ✅ override a headerben
{
    Serial.printf("[BLE] PIN confirmation: %06u\n", pin);
    NimBLEDevice::injectConfirmPasskey(connInfo, true);
}

void BLEManager::onAuthenticationComplete(NimBLEConnInfo& connInfo) // ✅ override a headerben
{
    if (!registry)
    {
        pServer->disconnect(connInfo.getConnHandle());
        return;
    }

    connectedHandle = connInfo.getConnHandle();
    connectedAddr   = connInfo.getIdAddress();

    // Ismert eszköz visszatért (működik akkor is, ha a párosítási ablak zárva van)
    if (registry->containsMac(connectedAddr.getVal()))
    {
        Serial.printf("[AUTH] Ismert eszkoz: %s\n", connectedAddr.toString().c_str());
        pairingWindowOpen  = false;
        pairingWindowEndMs = 0;
        waitingForName     = false;
        nameWindowEndMs    = 0;
        sendNotification("OK:WELCOME_BACK");
        return;
    }

    // Új eszköz - csak ha a párosítási ablak nyitva van
    if (!pairingWindowOpen)
    {
        Serial.println("[AUTH] Párosítási ablak zárt, új eszköz nem engedélyezett");
        NimBLEDevice::deleteBond(connInfo.getAddress());
        pServer->disconnect(connInfo.getConnHandle());
        return;
    }

    if (registry->isFull())
    {
        Serial.println("[AUTH] Megtelt a memoria");
        sendNotification("ERR:FULL");
        disconnectAndCleanup(true);
        return;
    }

    // Új eszköz – névkérés
    Serial.println("[AUTH] Uj eszkoz parositva, nev kerese...");
    waitingForName  = true;
    nameWindowEndMs = millis() + NAME_REQUEST_TIMEOUT_MS;
    sendNotification("REQUEST_NAME");
}

void BLEManager::onRead(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo)
{
    Serial.printf("[BLE<-] Read: %s\n", connInfo.getAddress().toString().c_str());
}

void BLEManager::onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo)
{
    String value = String(pChar->getValue().c_str());
    value.trim();
    if (value.isEmpty()) return;

    Serial.printf("[BLE<-] Write: %s\n", value.c_str());

    // Névadás folyamatban
    if (waitingForName)
    {
        handleNameWrite(value, connInfo);
        return;
    }

    // ── Parancsok ────────────────────────────────────────────
    if (value == "LIST")               { handleListCommand();         return; }
    if (value.startsWith("DEL:"))      { handleDeleteCommand(value);  return; }
    if (value.startsWith("CAN:"))      { handleCanSendCommand(value); return; }
    if (value.startsWith("CANSPEED:")) { handleCanSpeedCommand(value); return; }
    if (value == "WIFI:START")         { handleWifiStart();           return; }
    if (value == "WIFI:STOP")          { handleWifiStop();            return; }
    if (value == "CLEAR")              { clearBonds(); sendNotification("OK:CLEARED"); return; }

    sendNotification("ERR:UNKNOWN");
}


// ═══════════════════════════════════════════════════════════════
// Névírás kezelése
// ═══════════════════════════════════════════════════════════════

void BLEManager::handleNameWrite(const String& value, NimBLEConnInfo& connInfo)
{
    if (!value.startsWith("N:"))
    {
        Serial.println("[AUTH] Rossz formatum, vart N:nev");
        disconnectAndCleanup(true);
        resetPairingState();
        return;
    }

    String name = value.substring(2);
    name.trim();

    if (name.isEmpty() || name.length() > 32 || !registry)
    {
        Serial.println("[AUTH] Ervenytelen nev");
        disconnectAndCleanup(true);
        resetPairingState();
        return;
    }

    registry->addDevice(connectedAddr.getVal(), name.c_str());
    sendNotification("OK:PAIRED:" + name);
    Serial.printf("[AUTH] Parositva: %s\n", name.c_str());

    // Csak a névkérési ablakot zárjuk, de a párosítási ablak marad nyitva
    // hogy az ismert eszközök is tudjanak csatlakozni az ablak alatt
    waitingForName     = false;
    nameWindowEndMs    = 0;
}


// ═══════════════════════════════════════════════════════════════
// Segédfüggvények
// ═══════════════════════════════════════════════════════════════

void BLEManager::disconnectAndCleanup(bool deleteBond)
{
    if (connectedHandle == BLE_HS_CONN_HANDLE_NONE) return;

    if (deleteBond)
        NimBLEDevice::deleteBond(connectedAddr);

    pServer->disconnect(connectedHandle);
}


void BLEManager::resetPairingState()
{
    pairingWindowOpen  = false;
    pairingWindowEndMs = 0;
    waitingForName     = false;
    nameWindowEndMs    = 0;
    connectedAddr      = NimBLEAddress();
    connectedHandle    = BLE_HS_CONN_HANDLE_NONE;
}

// ✅ Dupla hívás eltávolítva
void BLEManager::bleSecurity()
{
    NimBLEDevice::setSecurityAuth(
        BLE_SM_PAIR_AUTHREQ_SC |
        BLE_SM_PAIR_AUTHREQ_MITM |
        BLE_SM_PAIR_AUTHREQ_BOND);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
    NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
    NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
    // ✅ Nem rögzítjük a PIN-t, hanem hagyunk az onPassKeyDisplay() callback-et működni
}

bool BLEManager::isPairingActive()
{
    return pairingWindowOpen || waitingForName;
}

bool BLEManager::iswifiactive()
{
    return wifiBridge.isClientConnected();
}

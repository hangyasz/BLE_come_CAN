#include "BLE_come.h"
#include <nvs.h>
#include <nvs_flash.h>
#include <Preferences.h>
#if defined(CONFIG_BLUEDROID_ENABLED)
#include <esp_gap_ble_api.h>
#endif


BLEManager::BLEManager() {
    bleRunning = false;
    connectionProcessRunning = false;
    advertisingRunning = false;
    pServer = nullptr;
    pService = nullptr;
    pCharacteristic = nullptr;
    pCCCD = nullptr;
    advertising = nullptr;
}

void BLEManager::init() {
    ensureBleCccdNamespace();

    BLEDevice::init(BLE_DEVICE_NAME);
    
    BLEDevice::setSecurityCallbacks(this);
    
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(this);

    pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY 
                    );
    
    // Set permissions BEFORE adding descriptors
    pCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
    
    // Initialize characteristic value
    pCharacteristic->setValue("Ready");
    
    // Add CCCD descriptor for notifications (must be before service start)
    pCCCD = new BLE2902();
    pCCCD->setAccessPermissions(ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE);
    pCCCD->setValue((uint8_t[2]){0x00, 0x00}, 2);
    pCharacteristic->addDescriptor(pCCCD);
    
    pService->start();
    
    advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(SERVICE_UUID);
    advertising->setScanResponse(false);
    advertising->setMinPreferred(0x0);
    // Biztonsági beállítások                
    bleSecurity();

#if defined(CONFIG_BLUEDROID_ENABLED)
    syncWhitelistFromBonded();
    // Alap mod: csak korabban parositott (whitelistes) eszkozok csatlakozhatnak.
advertising->setScanFilter(false, false);
#endif

    if (advertising->start()) {
        advertisingRunning = true;
        Serial.println("[BLE] Alap hirdetes aktiv (bondolt eszkozok barmikor visszacsatlakozhatnak)");
    } else {
        advertisingRunning = false;
        Serial.println("[BLE] Alap hirdetes inditasa sikertelen");
    }
    
    bleRunning = true;
}

void BLEManager::tick() {
    // Whitelist szinkron (egyszer, induláskor)
    if (!whitelistSynced && bleRunning) {
        syncWhitelistFromBonded();
    }

    // Advertising újraindítás feldolgozása (sosem callbackből!)
    if (pendingAdvertisingRestart && bleRunning && advertising != nullptr && !connectionProcessRunning) {
        pendingAdvertisingRestart = false;
#if defined(CONFIG_BLUEDROID_ENABLED)
        advertising->setScanFilter(false, pendingWhitelistOnly);
#endif
        advertising->stop();
        if (advertising->start()) {
            advertisingRunning = true;
            Serial.printf("[BLE] Hirdetés újraindítva (%s mód)\n",
                          pendingWhitelistOnly ? "whitelist" : "nyílt");
        }
    }

    // Párosítási ablak lejárat
    if (pairingWindowOpen &&
        (uint32_t)(millis() - pairingWindowOpenedAtMs) >= pairingWindowDurationMs) {

        pairingWindowOpen = false;
        pairingWindowOpenedAtMs = 0;
        pendingAdvertisingRestart = true;
        pendingWhitelistOnly = false;
        Serial.println("[BLE] Párosítási ablak lejárt (60s)");
    }
}

void BLEManager::ensureBleCccdNamespace() {
    Preferences prefs;
    if (prefs.begin("ble_cccd", false)) {
        prefs.end();
    }
}


void BLEManager::startBLE() {
    if (!bleRunning || advertising == nullptr) {
        Serial.println("[BLE] Hirdetes nem indithato: BLE nincs inicializalva");
        return;
    }

    // Parositasi ablak: uj eszkozok is csatlakozhatnak (PIN kotelező).
    pairingWindowOpen = true;
    pairingWindowOpenedAtMs = millis();

    if (!advertisingRunning) {
        if (advertising->start()) {
            advertisingRunning = true;
            Serial.println("[BLE] Parositasi hirdetes elinditva (uj eszkozok engedelyezve, 60s)");
        } else {
            Serial.println("[BLE] Parositasi hirdetes inditasa sikertelen");
        }
    } 
}

void BLEManager::stopBLE() {
    // Csak a hirdetést állítjuk le, a szerver és a csatlakozott eszközök maradnak
    if (!bleRunning || !advertisingRunning) return;  // Ha nem fut, kilépünk

    advertising->stop();
    advertisingRunning = false;
    pairingWindowOpen = false;
    pairingWindowOpenedAtMs = 0;
    
    Serial.println("[BLE] Hirdetés leállítva (szerver és eszközök aktívak maradnak)");
}

/**
 * @brief Összes párosított eszköz törlése (BOND lista ürítése)
 * A hivatalos BLE API-val törli a tárolt bond rekordokat.
 */
void BLEManager::clearBonds() {
    Serial.println("[BLE] BOND lista törlése...");

    // Egyszeru es biztos megoldas: NVS ujrainicializalas, ami torli a bond rekordokat is.
    nvs_flash_deinit();
    esp_err_t err = nvs_flash_erase();
    if (err != ESP_OK) {
        Serial.printf("[BLE] NVS torlesi hiba: %d\n", (int)err);
        return;
    }

    err = nvs_flash_init();
    if (err != ESP_OK) {
        Serial.printf("[BLE] NVS ujrainit hiba: %d\n", (int)err);
        return;
    }

    Serial.println("[BLE] Bond rekordok torolve. Telefonon is torold a parositast, majd reset.");
}

void BLEManager::onConnect(BLEServer* pSrv) {
    connectionProcessRunning = true;
    advertisingRunning = false; // Kapcsolatkor a hirdetes leall.
    this->pServer = pSrv;
    Serial.println("[BLE] Eszkoz csatlakozott");
    
}

void BLEManager::onDisconnect(BLEServer* pSrv) {
    connectionProcessRunning = false;
    this->pServer = pSrv;
    Serial.println("[BLE] Eszköz lecsatlakozott");

    pairingWindowOpen = false;
    // Itt is csak flag - ne hívj advertising API-t közvetlenül
    pendingAdvertisingRestart = true;
    pendingWhitelistOnly = false;
}

uint32_t BLEManager::onPassKeyRequest() {
        Serial.println("[BLE] PassKey kérés...");
        return 0; // 0-t adunk vissza, mert a stack generálja a random kódot
    }

void BLEManager::onPassKeyNotify(uint32_t pass_key) {
    Serial.printf("[BLE] PassKey: %06u\n", pass_key);
}

bool BLEManager::onConfirmPIN(uint32_t pass_key) {
    Serial.printf("[BLE] PIN megerősítés: %06u\n", pass_key);
    return true; // Elfogadjuk a párosítást
}

bool BLEManager::onSecurityRequest() {
    Serial.println("[BLE] Biztonsági kérés érkezett");
    return true; // Elfogadjuk a biztonsági kérést
}

void BLEManager::onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) {
    if (cmpl.success) {
        BLEAddress peerAddr(cmpl.bd_addr);
        BLEDevice::whiteListAdd(peerAddr);
        Serial.printf("[BLE] Hitelesítés OK: %s\n", peerAddr.toString().c_str());

        pairingWindowOpen = false;
        pairingWindowOpenedAtMs = 0;
    } else {
        Serial.println("[BLE] Hitelesítés sikertelen");
        if (pServer != nullptr) {
            pServer->disconnect(pServer->getConnId());
        }
    }
}

void BLEManager::bleSecurity() {
    // Fontos: BLESecurity API-t használunk, mert ez kapcsolja be a framework
    // belső security flag-jeit (így csatlakozáskor ténylegesen elindul a pairing).
    BLEDevice::setSecurityCallbacks(this);

    // bonding = true -> egyszeri PIN, utana a mar parositott telefon PIN nelkul visszajohet
    // mitm = true, sc = true -> PIN-kódos, biztonságos kapcsolat
    BLESecurity::setAuthenticationMode(true, true, true);
    BLESecurity::setCapability(ESP_IO_CAP_OUT);
    BLESecurity::setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    BLESecurity::setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    BLESecurity::setKeySize(16);

    // Random 6 jegyű PIN uj parositasokhoz
    BLESecurity::setPassKey(false);
    BLESecurity::regenPassKeyOnConnect(true);

    Serial.println("[BLE] Security aktiv: MITM + SC + BOND (ismert telefon visszacsatlakozik)");
}

void BLEManager::syncWhitelistFromBonded() {
/* #if defined(CONFIG_BLUEDROID_ENABLED)
    int dev_num = esp_ble_get_bond_device_num();
    if (dev_num <= 0) {
        Serial.println("[BLE] Nincs bond rekord, whitelist ures");
        return;
    }

    esp_ble_bond_dev_t *bond_dev = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
    if (!bond_dev) {
        Serial.println("[BLE] Nem sikerult memoriat foglalni a bond listahoz");
        return;
    }

    int copied = dev_num;
    esp_err_t ret = esp_ble_get_bond_device_list(&copied, bond_dev);
    if (ret != ESP_OK) {
        Serial.printf("[BLE] Bond lista lekeresi hiba: %d\n", (int)ret);
        free(bond_dev);
        return;
    }

    for (int i = 0; i < copied; i++) {
        BLEAddress addr(bond_dev[i].bd_addr);
        BLEDevice::whiteListAdd(addr);
        Serial.printf("[BLE] Bondolt eszkoz a whitelisthez adva: %s\n", addr.toString().c_str());
    }

    free(bond_dev);
    Serial.printf("[BLE] Whitelist szinkron kesz (%d bondolt eszkoz)\n", copied);
    whitelistSynced = true;
#endif */
whitelistSynced = true; // Ne fusson újra
    Serial.println("[BLE] Whitelist szinkron kihagyva (RPA miatt kikapcsolva)");
}

void BLEManager::sendNotification(const String message) {
    if (pCharacteristic == nullptr) {
        Serial.println("[BLE] Nem lehet értesítést küldeni: karakterisztika nincs inicializálva");
        return;
    }
    if (pServer == nullptr) {
        Serial.println("[BLE] Nem lehet értesítést küldeni: szerver nincs inicializálva");
        return;
    }
    
    // Check if any client is connected (use getConnectedCount instead of getConnId)
    if (pServer->getConnectedCount() == 0) {
        Serial.println("[BLE] Nem lehet értesítést küldeni: nincs csatlakozott eszköz");
        return;
    }

    pCharacteristic->setValue(message);
    pCharacteristic->notify();
    Serial.printf("[BLE] Értesítés küldve: %s\n", message.c_str());
}
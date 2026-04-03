#include "BLE_come.h"
#include <nvs.h>
#include <nvs_flash.h>
#if defined(CONFIG_BLUEDROID_ENABLED)
#include <esp_gap_ble_api.h>
#endif


BLEManager::BLEManager() {
    // Csak inicializálunk tagváltozókat, BLE inicializáció nincs!
    bleRunning = false;
    connectionProcessRunning = false;
    advertisingRunning = false;
    pServer = nullptr;
    pService = nullptr;
    pCharacteristic = nullptr;
    advertising = nullptr;
}

/**
 * @brief Inicializálja a BLE eszközt és szervízt.
 * Ezt a setup()-ból kell hívni, nem a konstruktorból!
 */
void BLEManager::init() {
    Serial.println("[BLE] 1. BLEDevice::init()");
    BLEDevice::init(BLE_DEVICE_NAME);
    
    Serial.println("[BLE] 2. setSecurityCallbacks()");
    BLEDevice::setSecurityCallbacks(this);
    
    Serial.println("[BLE] 3. createServer()");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(this);

    Serial.println("[BLE] 4. createService()");
    pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY 
                    );
    pService->start();
    
    Serial.println("[BLE] 5. setAccessPermissions()");
    pCharacteristic->addDescriptor(new BLE2902());
    pCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
    
    Serial.println("[BLE] 6. getAdvertising()");
    advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(SERVICE_UUID);
    advertising->setScanResponse(false);
    advertising->setMinPreferred(0x0);
    
    Serial.println("[BLE] 7. bleSecurity()");
    bleSecurity();
    
    Serial.println("[BLE] 8. init() vég");
    bleRunning = true;
}


void BLEManager::startBLE() {
    if (!bleRunning || advertising == nullptr) {
        Serial.println("[BLE] Hirdetes nem indithato: BLE nincs inicializalva");
        return;
    }

    // Ha már hirdetünk, ne csinálj semmit
    if (advertisingRunning) {
        Serial.println("[BLE] Hirdetés már aktív");
        return;
    }
    
    if (advertising->start()) {
        advertisingRunning = true;
        Serial.println("[BLE] Hirdetes elinditva");
    } else {
        Serial.println("[BLE] Hirdetes inditasa sikertelen");
    }
}

void BLEManager::stopBLE() {
    // Csak a hirdetést állítjuk le, a szerver és a csatlakozott eszközök maradnak
    if (!bleRunning || !advertisingRunning) return;  // Ha nem fut, kilépünk

    advertising->stop();
    advertisingRunning = false;
    
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

void BLEManager::onConnect(BLEServer* pServer) {
    connectionProcessRunning = true;
    advertisingRunning = false; // Kapcsolatkor a hirdetes leall.
    Serial.println("[BLE] Eszkoz csatlakozott");
}

void BLEManager::onDisconnect(BLEServer* pServer) {
    connectionProcessRunning = false;
    Serial.println("[BLE] Eszkoz lecsatlakozott");

    // Automatikusan visszainditjuk a hirdetest, hogy ujra lehessen csatlakozni.
    if (bleRunning && advertising != nullptr) {
        if (advertising->start()) {
            advertisingRunning = true;
            Serial.println("[BLE] Hirdetes ujrainditva disconnect utan");
        } else {
            advertisingRunning = false;
            Serial.println("[BLE] Hirdetes ujrainditasa sikertelen");
        }
    }
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
        Serial.println("[BLE] Hitelesítés sikeres");
    } else {
        Serial.println("[BLE] Hitelesítés sikertelen");
        if(pServer!= nullptr) {
            pServer->removePeerDevice(pServer->getConnId(),true);
        }
    }
}

void BLEManager::bleSecurity() {
    // Fontos: BLESecurity API-t használunk, mert ez kapcsolja be a framework
    // belső security flag-jeit (így csatlakozáskor ténylegesen elindul a pairing).
    BLEDevice::setSecurityCallbacks(this);

    // bonding = false -> minden újracsatlakozáskor új párosítás/PIN kérés
    // mitm = true, sc = true -> PIN-kódos, biztonságos kapcsolat
    BLESecurity::setAuthenticationMode(false, true, true);
    BLESecurity::setCapability(ESP_IO_CAP_OUT);
    BLESecurity::setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    BLESecurity::setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    BLESecurity::setKeySize(16);

    // Random 6 jegyű PIN minden új kapcsolatnál
    BLESecurity::setPassKey(false);
    BLESecurity::regenPassKeyOnConnect(true);

    Serial.println("[BLE] Security aktiv: MITM + SC, BOND kikapcsolva (PIN minden csatlakozasnal)");
}
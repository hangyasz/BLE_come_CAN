#pragma once

#include <Arduino.h>
#include "config.h"
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLESecurity.h>
#include <BLEUtils.h>



class BLEManager:public BLEServerCallbacks,
                public BLESecurityCallbacks {
public:
    BLEManager();   
    void init();
    void tick();
    void startBLE();
    void stopBLE();
    void clearBonds();
    void sendNotification(const String message);
private:
    bool bleRunning=false;
    bool connectionProcessRunning = false;
    bool advertisingRunning = false;
    bool pairingWindowOpen = false;
    bool whitelistSynced = false;
    bool pendingAdvertisingRestart = false;
    bool pendingWhitelistOnly = false;
    uint32_t pairingWindowOpenedAtMs = 0;
    uint32_t pairingWindowDurationMs = 60000;
    BLEServer* pServer = nullptr;
    BLEService* pService = nullptr;
    BLECharacteristic* pCharacteristic = nullptr;
    BLE2902* pCCCD = nullptr;
    BLEAdvertising* advertising=nullptr;

    void     ensureBleCccdNamespace();
    void     bleSecurity();
    void     syncWhitelistFromBonded();
     // ---- BLEServerCallbacks ----

    void onConnect(BLEServer* pSrv)    override;
    void onDisconnect(BLEServer* pSrv) override;

    // ---- BLESecurityCallbacks ----
    uint32_t onPassKeyRequest()                          override;
    void     onPassKeyNotify(uint32_t pass_key)          override;
    bool     onConfirmPIN(uint32_t pass_key)             override;
    bool     onSecurityRequest()                         override;
    void     onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override;
    
};
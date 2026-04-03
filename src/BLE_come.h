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
    void startBLE();
    void stopBLE();
    void clearBonds();
private:
    bool bleRunning=false;
    bool connectionProcessRunning = false;
    bool advertisingRunning = false;
    BLEServer* pServer = nullptr;
    BLEService* pService = nullptr;
    BLECharacteristic* pCharacteristic = nullptr;
    BLEAdvertising* advertising=nullptr;

    void     bleSecurity();
     // ---- BLEServerCallbacks ----

    void onConnect(BLEServer* pServer)    override;
    void onDisconnect(BLEServer* pServer) override;

    // ---- BLESecurityCallbacks ----
    uint32_t onPassKeyRequest()                          override;
    void     onPassKeyNotify(uint32_t pass_key)          override;
    bool     onConfirmPIN(uint32_t pass_key)             override;
    bool     onSecurityRequest()                         override;
    void     onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override;
    
};
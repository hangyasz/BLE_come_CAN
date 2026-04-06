#pragma once

#include <Arduino.h>
#include "config.h"
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLESecurity.h>
#include <BLEUtils.h>
#include "BleDevices.h"

class BLEManager : public BLEServerCallbacks,
                   public BLESecurityCallbacks,
                   public BLECharacteristicCallbacks {
public:
    BLEManager() = default;

    void setDeviceRegistry(BleDevices* registry);
    void init();
    void tick();
    void startBLE();    // Párosítási ablak megnyitása
    void stopBLE();     // Hirdetés leállítása
    void clearBonds();
    void sendNotification(const String& message);

private:
    BleDevices* _registry = nullptr;

    // ── Aktív munkamenet ──────────────────────────────────
    esp_bd_addr_t _connectedMac = {};
    uint16_t      _connectedId  = 0;
    bool          _waitingForName = false;
    uint32_t      _nameRequestTimestamp = 0;
    uint32_t      _authStateEnteredMs = 0;

    // ── BLE objektumok ────────────────────────────────────
    BLEServer*         pServer         = nullptr;
    BLEService*        pService        = nullptr;
    BLECharacteristic* pCharacteristic = nullptr;
    BLEAdvertising*    advertising     = nullptr;

    // ── Flagek ───────────────────────────────────────────
    bool     advertisingRunning  = false;
    bool     pairingWindowOpen   = false;
    bool     connectionProcessRunning = false;
    uint32_t pairingWindowOpenedAtMs = 0;

    // ── Privát metódusok ──────────────────────────────────
    void _bleSecurity();
    void _abortPairing();
    void _startAdvertising();

    // ── Callbacks ─────────────────────────────────────────
    void     onConnect(BLEServer* pSrv)                         override;
    void     onDisconnect(BLEServer* pSrv)                      override;
    uint32_t onPassKeyRequest()                                  override;
    void     onPassKeyNotify(uint32_t pass_key)                  override;
    bool     onConfirmPIN(uint32_t pass_key)                     override;
    bool     onSecurityRequest()                                 override;
    void     onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override;
    void     onRead(BLECharacteristic* pChar)                    override;
    void     onWrite(BLECharacteristic* pChar)                   override;
};
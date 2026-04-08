#pragma once

#include <Arduino.h>
#include "config.h"
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>
#include <NimBLEAdvertising.h>
#include "BleDevices.h"
#include "WifiBridge.h"

// NimBLE v2.x: security callbacks are merged into NimBLEServerCallbacks.
// There is no separate NimBLESecurityCallbacks class.

class BLEManager : public NimBLEServerCallbacks,
                   public NimBLECharacteristicCallbacks {
public:
    BLEManager() = default;

    void setDeviceRegistry(BleDevices* registry);
    void init();
    void tick();
    void startBLE();    // Párosítási ablak megnyitása
    void stopBLE();     // Hirdetés leállítása
    void clearBonds();
    void sendNotification(const String& message);
    bool isPairingActive();

private:
    BleDevices* _registry = nullptr;

    // ── Aktív munkamenet ──────────────────────────────────
    NimBLEAddress _connectedAddr;           // replaces esp_bd_addr_t _connectedMac
    uint16_t      _connectedId  = 0;
    bool          _waitingForName = false;
    uint32_t      _nameRequestTimestamp = 0;
    uint32_t      _authStateEnteredMs = 0;

    // ── BLE objektumok ────────────────────────────────────
    NimBLEServer*         pServer         = nullptr;
    NimBLEService*        pService        = nullptr;
    NimBLECharacteristic* pCharacteristic = nullptr;
    NimBLEAdvertising*    advertising     = nullptr;

    // ── Flagek ───────────────────────────────────────────
    bool     pairingWindowOpen        = false;
    bool     connectionProcessRunning = false;
    bool     pendingAdvertisingRestart = false;
    uint32_t pairingWindowOpenedAtMs  = 0;
    WifiBridge _wifiBridge;

    // ── Privát metódusok ──────────────────────────────────
    void _bleSecurity();
    void _abortPairing();
    void _startAdvertising();

    // ── NimBLEServerCallbacks ─────────────────────────────
    void onConnect(NimBLEServer* pSrv, NimBLEConnInfo& connInfo)                override;
    void onDisconnect(NimBLEServer* pSrv, NimBLEConnInfo& connInfo, int reason) override;

    // Security callbacks (were BLESecurityCallbacks, now part of NimBLEServerCallbacks)
    uint32_t onPassKeyDisplay();
    void     onPassKeyEntry(NimBLEConnInfo& connInfo);
    void     onConfirmPassKey(NimBLEConnInfo& connInfo, uint32_t passKey);
    void     onAuthenticationComplete(NimBLEConnInfo& connInfo);

    // ── NimBLECharacteristicCallbacks ─────────────────────
    // NimBLE v2: callbacks also receive NimBLEConnInfo&
    void onRead(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo)          override;
    void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo)         override;
};

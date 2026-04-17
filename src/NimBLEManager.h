#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEAdvertising.h>
#include "BleDevices.h"
#include "WifiBridge.h"
#include "config.h"

// CAN küldés callback – így a BLEManager nem függ közvetlenül a TWAI-tól
typedef bool (*CanSendCallback)(uint32_t id, bool extended, uint8_t len, uint8_t* data);

class BLEManager : public NimBLEServerCallbacks,
                   public NimBLECharacteristicCallbacks
{
public:
    BLEManager() = default;

    void setDeviceRegistry(BleDevices* registry);
    void setCanSendCallback(CanSendCallback cb);  // CAN küldéshez
    void init();
    void tick();
    void startPairing();  
    void stopBLE();
    void clearBonds();
    void sendNotification(const String& message);
    bool isPairingActive();
    bool iswifiactive();

private:
    BleDevices*      registry  = nullptr;
    CanSendCallback  canSendCb = nullptr;
    WifiBridge       wifiBridge;

    // ── Aktív munkamenet ──────────────────────────────────
    NimBLEAddress connectedAddr;
    uint16_t      connectedHandle   = BLE_HS_CONN_HANDLE_NONE;
    bool          waitingForName    = false;

    // ── BLE objektumok ────────────────────────────────────
    NimBLEServer*         pServer         = nullptr;
    NimBLEService*        pService        = nullptr;
    NimBLECharacteristic* pCharacteristic = nullptr;
    NimBLEAdvertising*    advertising     = nullptr;

    // ── Párosítási ablak ──────────────────────────────────
    bool     pairingWindowOpen   = false;
    uint32_t pairingWindowEndMs  = 0;
    uint32_t nameWindowEndMs     = 0;

    // ── WiFi task ─────────────────────────────────────────
    TaskHandle_t wifiTaskHandle = nullptr;
    void startWifiTaskIfNeeded();
    static void wifiTickTask(void* pvParameters);

    // ── Parancs feldolgozók ───────────────────────────────
    void handleListCommand();
    void handleDeleteCommand(const String& value);
    void handleCanSendCommand(const String& value);
    void handleWifiStart();
    void handleWifiStop();
    void handleNameWrite(const String& value, NimBLEConnInfo& connInfo);
    void handleCanSpeedCommand(String value);

    // ── Segéd ─────────────────────────────────────────────
    void disconnectAndCleanup(bool deleteBond = true);
    void resetPairingState();
    void bleSecurity();

    // ── NimBLE callbacks ──────────────────────────────────
    void     onConnect(NimBLEServer* pSrv, NimBLEConnInfo& connInfo)                override;
    void     onDisconnect(NimBLEServer* pSrv, NimBLEConnInfo& connInfo, int reason) override;
    uint32_t onPassKeyDisplay()                                                      override;
    void     onConfirmPassKey(NimBLEConnInfo& connInfo, uint32_t pin)                override;
    void     onAuthenticationComplete(NimBLEConnInfo& connInfo)                     override; // ✅ override
    void     onRead(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo)          override;
    void     onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo)         override;
};
#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEAdvertising.h>
#include "BleDevices.h"
#include "WifiBridge.h"
#include "Config.h"

// CAN küldés callback.
typedef bool (*CanSendCallback)(uint32_t id, bool extended, uint8_t len, uint8_t* data);

//NimBLEManager: a BLE szerver és kapcsolódó logika kezelése
class BLEManager : public NimBLEServerCallbacks,
                   public NimBLECharacteristicCallbacks
{
public:
    BLEManager() = default;

    // Eszközregiszter beállítása a párosított eszközök kezeléséhez.
    void setDeviceRegistry(BleDevices* registry);
    // CAN küldés callback beállítása.
    void setCanSendCallback(CanSendCallback cb);
    // Inicializálás, periodikus tick és vezérlő parancsok.
    void init();
    void tick();
    void startPairing();  
    void stopBLE();
    void clearBonds();
    void sendNotification(const String& message);
    // Állapotlekérdezések.
    bool isPairingActive();
    bool isWifiActive();
    bool isWifiStarted();

private:
    //párosított eszközök kezeléséhez.
    BleDevices*      registry  = nullptr;
    //CAN küldés callback.
    CanSendCallback  canSendCb = nullptr;
    // WiFi kapcsolat kezelése.
    WifiBridge       wifiBridge;

    //A jelenleg csatlakoztatott eszköz adatai.
    NimBLEAddress connectedAddr;
    uint16_t      connectedHandle   = BLE_HS_CONN_HANDLE_NONE;
    bool          waitingForName    = false;

    //NimBLE objektumok
    NimBLEServer*         pServer         = nullptr;
    NimBLEService*        pService        = nullptr;
    NimBLECharacteristic* pCharacteristic = nullptr;
    NimBLEAdvertising*    advertising     = nullptr;

    //Párosítási állapot és időzítés kezeléséhez.
    bool     pairingWindowOpen   = false;
    uint32_t pairingWindowEndMs  = 0;
    uint32_t nameWindowEndMs     = 0;

    //WiFi háttér task
    TaskHandle_t wifiTaskHandle = nullptr;
    void startWifiTaskIfNeeded();
    static void wifiTickTask(void* pvParameters);

    // ismétlődő CAN küldés ──────────────────────
    struct {
        bool active = false;
        uint32_t canId = 0;
        uint8_t dlc = 0;
        uint8_t data[8] = {0};
        uint32_t intervalMs = 100;
    } activeCanMsg;
    TaskHandle_t canModTaskHandle = nullptr;
    void startCanModTask();
    void stopCanModTask();
    static void canModTask(void* pvParameters);

    //BLE parancsok kezelése
    void handleListCommand();
    void handleDeleteCommand(const String& value);
    void handleCanSendCommand(const String& value);
    void handleCanSetdCommand(const String& value);  // Toggle CANMOD
    void handleCanModStop();
    void handleWifiStart();
    void handleWifiStop();
    void handleNameWrite(const String& value, NimBLEConnInfo& connInfo);
    void handleCanSpeedCommand(String value);

    //eszköz törlése, kapcsolat bontása és párosítási állapot visszaállítása
    void disconnectAndCleanup(bool deleteBond = true);
    // visszaállítja a párosítási állapotot, bontja a kapcsolatot és törli a bond-ot, ha szükséges.
    void resetPairingState();
    // Biztonsági funkciók beállítása és kezelése a NimBLE-ben.
    void bleSecurity();

    //NimBLE callback-ek
    void     onConnect(NimBLEServer* pSrv, NimBLEConnInfo& connInfo)                override;
    void     onDisconnect(NimBLEServer* pSrv, NimBLEConnInfo& connInfo, int reason) override;
    uint32_t onPassKeyDisplay()                                                      override;
    void     onConfirmPassKey(NimBLEConnInfo& connInfo, uint32_t pin)                override;
    void     onAuthenticationComplete(NimBLEConnInfo& connInfo)                     override; // ✅ override
    void     onRead(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo)          override;
    void     onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo)         override;
};
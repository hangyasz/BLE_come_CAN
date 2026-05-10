#pragma once

#include <Arduino.h>
#include <esp_gap_ble_api.h>
#include <array>
#include "BleDeviceStore.h"
#include "Config.h"
#include <nvs_flash.h>

//tárolja a párosított eszközöket memóriában, és kezeli a perzisztens tárolást

class BleDevices {
private:
    // Tárolt eszközök tömbjei fixed méretű tömbb
    std::array<DeviceRecord, BLE_MAX_STORED> devices{};
    
    // A jelenleg tárolt eszközök száma
    size_t deviceCount = 0;
    
    // Persistens tárolót kezelő helper
    BleDeviceStore store;
    
    // Belső segédfüggvény: MAC cím keresése a tömbben
    int findByMac(const uint8_t mac[6]) const;
    
    // Belső segédfüggvény: aktuális lista mentése persistent tárolóba
    bool saveCurrentDevices();

public:
    // Konstruktor (default inicializálás)
    BleDevices() = default;

    // Inicializálás: betölti a mentett eszközöket a persistent tárolóból
    void init();
    
    // Új eszköz hozzáadása vagy meglévő frissítése
    void addDevice(const esp_bd_addr_t mac, const char* name);
    
    // Eszköz eltávolítása MAC cím alapján
    bool removeDevice(const esp_bd_addr_t mac);
    
    // Ellenőrzés: szerepel-e az adott MAC a listában?
    bool containsMac(const esp_bd_addr_t mac) const;    
    // Eszköz eltávolítása index alapján
    bool removeAt(size_t index);
    
    // Az aktuálisan tárolt eszközök száma
    size_t getCount() const;
    
    // Ellenőrzés: megtelt-e a tár?
    bool isFull() const;
    
    // Visszaadja a megadott indexű eszköz adatait
    const DeviceRecord& at(size_t index) const;
    
    // Az összes eszköz törlése
    void clearAll();
};
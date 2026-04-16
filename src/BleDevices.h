#pragma once

#include <Arduino.h>
#include <esp_gap_ble_api.h>
#include <array>
#include "BleDeviceStore.h"
#include "config.h"
#include <nvs_flash.h>

class BleDevices {
private:
    std::array<DeviceRecord, BLE_MAX_STORED> devices{};
    size_t deviceCount = 0;
    BleDeviceStore store;
    int findByMac(const uint8_t mac[6]) const;
    bool saveCurrentDevices();

public:
    BleDevices() = default;

    void init();
    bool reload();

    // MAC cím alapú eszközkezelés (bond maga gondoskodik az IRK-ról)
    void addDevice(const esp_bd_addr_t mac, const char* name);
    bool removeDevice(const esp_bd_addr_t mac);
    bool containsMac(const esp_bd_addr_t mac) const;
    bool getName(const esp_bd_addr_t mac, char* outName, size_t maxLen) const;
    bool   removeAt(size_t index);         // ✅ új: index alapú törlés
    size_t getCount() const;               // ✅ const fix
    
    bool isFull() const;
    const DeviceRecord* data() const;
    const DeviceRecord& at(size_t index) const;
    void clearAll();
};
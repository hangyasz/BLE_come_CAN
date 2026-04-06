#pragma once

#include <Arduino.h>
#include <esp_gap_ble_api.h>
#include <vector>
#include "BleDeviceStore.h"
#include "config.h"
#include <nvs_flash.h>

class BleDevices {
private:
    std::vector<DeviceRecord> devices;
    BleDeviceStore            store;
    int findByMac(const uint8_t mac[6]) const;

public:
    BleDevices() = default;

    void init();
    bool reload();

    // MAC cím alapú eszközkezelés (bond maga gondoskodik az IRK-ról)
    void addDevice(const esp_bd_addr_t mac, const char* name);
    bool removeDevice(const esp_bd_addr_t mac);
    bool containsMac(const esp_bd_addr_t mac) const;
    bool getName(const esp_bd_addr_t mac, char* outName, size_t maxLen) const;
    
    int  count() const;
    bool isFull() const;
    const std::vector<DeviceRecord>& getDevices() const;
    void clearAll();
};
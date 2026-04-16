#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <array>
#include <cstring>
#include "config.h"

// Mac cím alapú eszközinformáció
struct DeviceRecord {
    esp_bd_addr_t mac;  // 6 bájtos MAC cím
    char    name[33];   // Max 32 char + null};
};

class BleDeviceStore {
private:
    Preferences prefs;
    static constexpr const char* kNamespace  = "ble_store";
    static constexpr const char* kRecordsKey = "records";

public:
    bool saveDevices(const std::array<DeviceRecord, BLE_MAX_STORED>& devices, size_t count) {
        if (!prefs.begin(kNamespace, false)) {
            Serial.println("[STORE] open rw failed");
            return false;
        }

        if (count == 0) {
            prefs.remove(kRecordsKey);
        } else {
            if (count > BLE_MAX_STORED) {
                count = BLE_MAX_STORED;
            }

            size_t bytes = count * sizeof(DeviceRecord);
            size_t written = prefs.putBytes(kRecordsKey, devices.data(), bytes);
            if (written != bytes) {
                Serial.println("[STORE] write size mismatch!");
                prefs.end();
                return false;
            }
        }
        prefs.end();
        return true;
    }

    bool loadDevices(std::array<DeviceRecord, BLE_MAX_STORED>& records, size_t& outCount) {
        outCount = 0;

        if (!prefs.begin(kNamespace, true)) {
            Serial.println("[STORE] open ro failed");
            return false;
        }

        size_t bytes = prefs.getBytesLength(kRecordsKey);
        if (bytes == 0) {
            prefs.end();
            return true;
        }

        if ((bytes % sizeof(DeviceRecord)) != 0) {
            Serial.println("[STORE] invalid blob – törölve");
            prefs.end();
            prefs.begin(kNamespace, false);
            prefs.remove(kRecordsKey);
            prefs.end();
            return false;
        }

        size_t recordCount = bytes / sizeof(DeviceRecord);
        if (recordCount > BLE_MAX_STORED) {
            recordCount = BLE_MAX_STORED;
        }

        size_t readBytes = recordCount * sizeof(DeviceRecord);
        size_t r = prefs.getBytes(kRecordsKey, records.data(), readBytes);
        prefs.end();

        if (r != readBytes) {
            outCount = 0;
            return false;
        }

        outCount = recordCount;
        return true;
    }
};
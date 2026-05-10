#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <array>
#include <cstring>
#include "Config.h"


// Perzisztens BLE eszközlista tárolás


// MAC cím alapú eszközinformáció.
// Fix méretű struktúra, hogy egyszerűen menthető legyen bináris formában.
struct DeviceRecord {
    esp_bd_addr_t mac;  // 6 bájtos BLE MAC cím
    char name[33];      // Max 32 karakter + lezáró '\0'
};

class BleDeviceStore {
private:
    Preferences prefs;
    // NVS namespace és kulcs, ahol a rekordok tárolódik.
    static constexpr const char* kNamespace  = "ble_store";
    static constexpr const char* kRecordsKey = "records";

public:
    // Eszközlista mentése NVS-be.
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

    // Eszközlista betöltése NVS-ből.
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

        // A blob mérete csak akkor jó, ha pontosan rekordméret többszöröse.
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
            // Védőkorlát: sosem olvasunk a lokális tömb kapacitásán túl.
            recordCount = BLE_MAX_STORED;
        }

        size_t readBytes = recordCount * sizeof(DeviceRecord);
        size_t r = prefs.getBytes(kRecordsKey, records.data(), readBytes);
        prefs.end();

        if (r != readBytes) {
            // Részleges olvasás esetén inkább hibát adunk vissza.
            outCount = 0;
            return false;
        }

        outCount = recordCount;
        return true;
    }
};
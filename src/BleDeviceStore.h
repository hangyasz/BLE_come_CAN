#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <vector>
#include <cstring>

// Mac cím alapú eszközinformáció
struct DeviceRecord {
    esp_bd_addr_t mac;  // 6 bájtos MAC cím
    char    name[33];   // Max 32 char + null
    uint32_t lastConnected; // Utolsó csatlakozás időpontja
};

class BleDeviceStore {
private:
    Preferences prefs;
    static constexpr const char* kNamespace  = "ble_store";
    static constexpr const char* kRecordsKey = "records";

public:
    bool saveDevices(const std::vector<DeviceRecord>& devices) {
        if (!prefs.begin(kNamespace, false)) {
            Serial.println("[STORE] open rw failed");
            return false;
        }
        if (devices.empty()) {
            prefs.remove(kRecordsKey);
        } else {
            size_t bytes   = devices.size() * sizeof(DeviceRecord);
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

    std::vector<DeviceRecord> getDevices() {
        std::vector<DeviceRecord> records;
        if (!prefs.begin(kNamespace, true)) {
            Serial.println("[STORE] open ro failed");
            return records;
        }
        size_t bytes = prefs.getBytesLength(kRecordsKey);
        if (bytes == 0) { prefs.end(); return records; }

        if ((bytes % sizeof(DeviceRecord)) != 0) {
            Serial.println("[STORE] invalid blob – törölve");
            prefs.end();
            prefs.begin(kNamespace, false);
            prefs.remove(kRecordsKey);
            prefs.end();
            return records;
        }
        records.resize(bytes / sizeof(DeviceRecord));
        size_t r = prefs.getBytes(kRecordsKey, records.data(), bytes);
        prefs.end();
        if (r != bytes) records.clear();
        return records;
    }
};
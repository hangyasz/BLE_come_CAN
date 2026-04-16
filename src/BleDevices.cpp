#include "BleDevices.h"

#include "config.h"

int BleDevices::findByMac(const uint8_t mac[6]) const {
    for (size_t i = 0; i < deviceCount; ++i)
        if (memcmp(devices[i].mac, mac, 6) == 0) return (int)i;
    return -1;
}

bool BleDevices::saveCurrentDevices() {
    return store.saveDevices(devices, deviceCount);
}

void BleDevices::init() { 
    if (!store.loadDevices(devices, deviceCount)) {
        deviceCount = 0;
    }

    saveCurrentDevices();
    Serial.printf("[BleDevices] %d eszköz betöltve\n", (int)deviceCount);
}

bool BleDevices::reload() {
    if (!store.loadDevices(devices, deviceCount)) {
        deviceCount = 0;
    }

    saveCurrentDevices();
    Serial.printf("[BleDevices] %d eszköz újra betöltve\n", (int)deviceCount);
    return true;
}

void BleDevices::addDevice(const esp_bd_addr_t mac, const char* name) {
    if (!mac || !name) return;

    int idx = findByMac(mac);
    DeviceRecord r = {};
    memcpy(r.mac, mac, 6);
    strncpy(r.name, name, 32);
    r.name[32] = '\0';
    if (idx >= 0) {
        devices[idx] = r;  // Frissítés
    } else {
        if (deviceCount >= BLE_MAX_STORED) {
            Serial.printf("[BleDevices] Maximum %d eszköz elérve, új eszköz nem menthető\n", BLE_MAX_STORED);
            return;
        }
        devices[deviceCount++] = r;
    }

    if (saveCurrentDevices()) {
        Serial.printf("[BleDevices] Mentve: %s -> \"%s\"\n", 
                      getMacString(mac).c_str(), name);
    }
}

bool BleDevices::removeDevice(const esp_bd_addr_t mac) {
    int idx = findByMac(mac);
    if (idx < 0) return false;
    Serial.printf("[BleDevices] Törölve: \"%s\"\n", devices[idx].name);
    for (size_t i = (size_t)idx; i + 1 < deviceCount; ++i) {
        devices[i] = devices[i + 1];
    }
    if (deviceCount > 0) {
        --deviceCount;
    }
    return saveCurrentDevices();
}

bool BleDevices::containsMac(const esp_bd_addr_t mac) const {
    return findByMac(mac) >= 0;
}

bool BleDevices::getName(const esp_bd_addr_t mac,
                          char* outName, size_t maxLen) const {
    int idx = findByMac(mac);
    if (idx < 0) return false;
    strncpy(outName, devices[idx].name, maxLen - 1);
    outName[maxLen - 1] = '\0';
    return true;
}

// ── removeAt hozzáadva ─────────────────────────────────────────
bool BleDevices::removeAt(size_t idx)
{
    if (idx >= deviceCount) return false;

    Serial.printf("[BleDevices] Torles index alapjan: [%d] \"%s\"\n",
                  (int)idx, devices[idx].name);

    for (size_t i = idx; i + 1 < deviceCount; ++i)
        devices[i] = devices[i + 1];

    --deviceCount;
    return saveCurrentDevices();
}

// ── getCount const fix ─────────────────────────────────────────
size_t BleDevices::getCount() const  // ✅ const hozzáadva
{
    return deviceCount;
}

bool BleDevices::isFull() const { return deviceCount >= BLE_MAX_STORED; }

const DeviceRecord* BleDevices::data() const {
    return devices.data();
}

const DeviceRecord& BleDevices::at(size_t index) const {
    return devices.at(index);
}
// BleDevices::clearAll()
void BleDevices::clearAll() {
    deviceCount = 0;
    saveCurrentDevices();
    // NimBLE bond törlés NEM itt
    Serial.println("[BleDevices] Törölve. Telefonon is töröld, majd reset.");
}


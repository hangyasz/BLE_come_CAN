#include "BleDevices.h"

#include "config.h"

int BleDevices::findByMac(const uint8_t mac[6]) const {
    for (size_t i = 0; i < devices.size(); ++i)
        if (memcmp(devices[i].mac, mac, 6) == 0) return (int)i;
    return -1;
}

void BleDevices::init() { 
    devices = store.getDevices();
    Serial.printf("[BleDevices] %d eszköz betöltve\n", (int)devices.size());
}

bool BleDevices::reload() {
    devices = store.getDevices();
    Serial.printf("[BleDevices] %d eszköz újra betöltve\n", (int)devices.size());
    return true;
}

void BleDevices::addDevice(const esp_bd_addr_t mac, const char* name) {
    if (!mac || !name) return;

    int idx = findByMac(mac);
    DeviceRecord r = {};
    memcpy(r.mac, mac, 6);
    strncpy(r.name, name, 32);
    r.name[32] = '\0';
    r.lastConnected = millis();

    if (idx >= 0) {
        devices[idx] = r;  // Frissítés
    } else {
        devices.push_back(r);
    }

    if (store.saveDevices(devices)) {
        Serial.printf("[BleDevices] Mentve: %s -> \"%s\"\n", 
                      getMacString(mac).c_str(), name);
    }
}

bool BleDevices::removeDevice(const esp_bd_addr_t mac) {
    int idx = findByMac(mac);
    if (idx < 0) return false;
    Serial.printf("[BleDevices] Törölve: \"%s\"\n", devices[idx].name);
    devices.erase(devices.begin() + idx);
    return store.saveDevices(devices);
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

int BleDevices::count() const { return (int)devices.size(); }

bool BleDevices::isFull() const { return (int)devices.size() >= BLE_MAX_STORED; }

const std::vector<DeviceRecord>& BleDevices::getDevices() const { return devices; }
// BleDevices::clearAll()
void BleDevices::clearAll() {
    devices.clear();
    store.saveDevices(devices);
    // NimBLE bond törlés NEM itt
    Serial.println("[BleDevices] Törölve. Telefonon is töröld, majd reset.");
}
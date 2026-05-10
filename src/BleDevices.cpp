#include "BleDevices.h"

#include "Config.h"

// ------------------------------------------------------------------
// BleDevices.cpp
// - Kezeli a korábban párosított / tárolt BLE eszközök listáját
// - A fájl felelős a memóriában tartott `devices` tömb CRUD műveleteiért
// - Mentés/Betöltés a `store` helperen keresztül történik (persistencia)
// Megjegyzés: minden komment magyarul, röviden és célratörően.
// ------------------------------------------------------------------

// Keresés MAC cím alapján. Visszatér az index-szel a tömbben, vagy -1 ha nincs.
int BleDevices::findByMac(const uint8_t mac[6]) const {
    for (int i = 0; i < deviceCount; ++i)
        if (memcmp(devices[i].mac, mac, 6) == 0) return i;
    return -1;
}


// Mentés: a jelenlegi `devices` tömböt elmenti a perzisztens tárolóba.
// Visszaadja, hogy a mentés sikeres volt-e.
bool BleDevices::saveCurrentDevices() {
    return store.saveDevices(devices, deviceCount);
}


// Inicializálás: betölti a mentett eszközöket. Ha nincs mentés, 0 elemmel folytat.
// Végén frissíti a perzisztens tárolót (biztonsági mentés/normalizálás céljából).
void BleDevices::init() { 
    if (!store.loadDevices(devices, deviceCount)) {
        deviceCount = 0;
    }

    saveCurrentDevices();
    Serial.printf("[BleDevices] %d eszköz betöltve\n", (int)deviceCount);
}




// Eszköz hozzáadása vagy frissítése MAC cím alapján.
// - Ha a MAC már létezik, frissítjük a nevét
// - Ha új eszköz, hozzáadjuk a listához (ha van még hely)
void BleDevices::addDevice(const esp_bd_addr_t mac, const char* name) {
    if (!mac || !name) return;
    if (deviceCount >= BLE_MAX_STORED) {
            Serial.printf("[BleDevices] Maximum %d eszköz elérve, új eszköz nem menthető\n", BLE_MAX_STORED);
            return;
        }
    int idx = findByMac(mac);
    DeviceRecord r = {};
    memcpy(r.mac, mac, 6);
    strncpy(r.name, name, 32);
    r.name[32] = '\0';

     if (idx >= 0) {
        devices[idx] = r;
    } else {
        devices[deviceCount++] = r;
    }
    // Mentés és visszajelzés a sikerességről
    if (saveCurrentDevices()) {
        Serial.printf("[BleDevices] Mentve: %s -> \"%s\"\n", 
                      getMacString(mac).c_str(), name);
    }
}


// Eszköz törlése MAC alapján.
bool BleDevices::removeDevice(const esp_bd_addr_t mac) {
    int idx = findByMac(mac);
    if (idx < 0) return false;

    Serial.printf("[BleDevices] Törölve: \"%s\"\n", devices[idx].name);
    for (int i = idx; i + 1 < deviceCount; ++i) {
        devices[i] = devices[i + 1];
    }
    if (deviceCount > 0) {
        --deviceCount;
    }
    return saveCurrentDevices();
}


// Ellenőrzi, hogy a megadott MAC szerepel-e a listában.
bool BleDevices::containsMac(const esp_bd_addr_t mac) const {
    return findByMac(mac) >= 0;
}



// Törlés index alapján
bool BleDevices::removeAt(size_t idx)
{
    if (idx >= deviceCount) return false;

    Serial.printf("[BleDevices] Torles index alapjan: [%d] \"%s\"\n",
                  (int)idx, devices[idx].name);

    for (int i = idx; i + 1 < deviceCount; ++i)
        devices[i] = devices[i + 1];

    --deviceCount;
    return saveCurrentDevices();
}


// Visszaadja a tárolt eszközök számát.
size_t BleDevices::getCount() const
{
    return deviceCount;
}


// Ellenőrzi, hogy van-e még hely új eszközök számára.
bool BleDevices::isFull() const { return deviceCount >= BLE_MAX_STORED; }


//Visszaadja a megadott indexű eszköz adatait.
const DeviceRecord& BleDevices::at(size_t index) const {
    return devices.at(index);
}


// Törli az összes eszközt a memóriából
void BleDevices::clearAll() {
    deviceCount = 0;
    saveCurrentDevices();
    Serial.println("[BleDevices] Osszes eszkoz torolve");
}


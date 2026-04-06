
#pragma once
#define BLE_DEVICE_NAME "BLE_come"
#define SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID    "beb5483e-36e1-4688-b7f5-ea07361b2611"

#define PAIRING_WINDOW_MS      60000   // 60s
#define NAME_REQUEST_TIMEOUT_MS 30000  // 30s
#define BLE_MAX_PAIRED          5
#define BLE_MAX_STORED          10

// --- CAN (TWAI) pinout -------------------------------------
#define PIN_CAN_TX          1   // SN65HVD230 TXD
#define PIN_CAN_RX          3   // SN65HVD230 RXD



// Visszaadja a MAC címet szépen formázott String-ként (pl. Terminalba kiíráshoz)
inline String getMacString(const uint8_t mac[6]) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}

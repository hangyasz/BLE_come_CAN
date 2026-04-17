
#pragma once
#define BLE_DEVICE_NAME "BLE_come"
#define SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID    "beb5483e-36e1-4688-b7f5-ea07361b2611"

#define PAIRING_WINDOW_MS   60000 // 60 másodperc
#define NAME_REQUEST_TIMEOUT_MS 30000  // 30s
#define BLE_MAX_STORED          5


// --- Wi-Fi AP + TCP stream --------------------------------
#define WIFI_AP_SSID            "ESP32_Sniffer"
#define WIFI_AP_PASSWORD        "TitkosWifi123"
#define WIFI_TCP_PORT           23
#define WIFI_HELLO_INTERVAL_MS  10
#define WIFI_HELLO_MESSAGE      "hello wifi\n"
#define WIFI_InACTIVITY_TIMEOUT_MS  60000 // 1 perc inaktivitás után WiFi AP leállítása

// --- CAN (TWAI) pinout -------------------------------------
#define PIN_CAN_TX          17  // SN65HVD230 TXD (CTX)
#define PIN_CAN_RX          16  // SN65HVD230 RXD (CRX)



// Visszaadja a MAC címet szépen formázott String-ként (pl. Terminalba kiíráshoz)
inline String getMacString(const uint8_t mac[6]) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}


// CAN bus paraméterek (GVRET kompatibilis bitmask)
#define CAN_TX GPIO_NUM_17
#define CAN_RX GPIO_NUM_16
static const uint32_t PERIODIC_CAN_PERIOD_US = 500000;
bool initCan(uint32_t speed);


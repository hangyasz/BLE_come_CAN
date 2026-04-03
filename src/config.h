// --- BLE ---------------------------------------------------
#define BLE_DEVICE_NAME "BLE_come"
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define BLE_MAX_PAIRED          5
#define BLE_MAX_CONNECTIONS     5
#define BLE_ADVERTISING_TIMEOUT_MS  60000   // 1 perc
#define BLE_HIGH_VIS_INTERVAL       100     // mss
#define BLE_LOW_VIS_INTERVAL        1600    // ms
#define BLE_NAME_REQUEST_TIMEOUT_MS 10000   // 10 mp
#define BLE_LONG_PRESS_MS           10000   // 10 mp (párosítás törlés)
#define BLE_PIN_LENGTH      6

// --- CAN (TWAI) pinout -------------------------------------
#define PIN_CAN_TX          1   // SN65HVD230 TXD
#define PIN_CAN_RX          3   // SN65HVD230 RXD

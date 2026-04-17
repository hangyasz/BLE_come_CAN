#include "CanTask.h"

// 📌 Itt foglaljuk le a memóriát a Queue-nak
QueueHandle_t canRxQueue = NULL;

// ── CAN RX FreeRTOS Task ───────────────────────────────────────
void canRxTask(void *pvParameters) {
    // 1. Visszakapjuk a BLEManager pointert a paraméterből
    BLEManager* bleManagerPtr = static_cast<BLEManager*>(pvParameters);

    // 2. Létrehozzuk a Queue-t (100 db CAN üzenet fér bele)
    canRxQueue = xQueueCreate(100, sizeof(twai_message_t));
    twai_message_t rx_msg;
    
    while(1) {
        // Csak akkor olvassuk a CAN-t, ha van csatlakozott telefon
        if (bleManagerPtr->iswifiactive()) { 
            
            // Olvasás a CAN vezérlőből (10 ms timeout)
            if (twai_receive(&rx_msg, pdMS_TO_TICKS(10)) == ESP_OK) {
                // Betoljuk a Queue-ba. Ha tele van, eldobjuk.
                xQueueSend(canRxQueue, &rx_msg, 0);
            }
            
        } else {
            // Nincs kliens: Alszunk 100ms-t, hogy ne fogyasszunk áramot
            vTaskDelay(pdMS_TO_TICKS(100));
            
            // Kiürítjük a puffert, hogy tiszta lappal induljunk
            if (canRxQueue != NULL) {
                xQueueReset(canRxQueue);
            }
        }
    }
}

// ── Indító függvény ────────────────────────────────────────────
void startCanTask(BLEManager* manager) {
    xTaskCreatePinnedToCore(
        canRxTask, 
        "CAN_RX", 
        4096, 
        manager, // ✅ Itt adjuk át a tasknak a manager pointert!
        5, 
        NULL, 
        1
    );
}

// ── CAN Inicializálás ──────────────────────────────────────────
bool initCan(uint32_t speed) {
    // Ide tedd be a konfigurációs logikát (a pinek legyenek definiálva itt vagy a config.h-ban)
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        GPIO_NUM_17, GPIO_NUM_16, TWAI_MODE_NORMAL); // Cseréld a makróidra (CAN_TX, CAN_RX)

    twai_timing_config_t t_config;
    switch (speed) {
        case 125000:  t_config = TWAI_TIMING_CONFIG_125KBITS(); break;
        case 250000:  t_config = TWAI_TIMING_CONFIG_250KBITS(); break;
        case 500000:  t_config = TWAI_TIMING_CONFIG_500KBITS(); break;
        case 1000000: t_config = TWAI_TIMING_CONFIG_1MBITS(); break;
        default: return false;
    }

    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) return false;
    if (twai_start() != ESP_OK) return false;

    return true;
}
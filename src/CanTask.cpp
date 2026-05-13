#include "CanTask.h"


// Globális queue a beérkező CAN üzenetekhez.
QueueHandle_t canRxQueue = NULL;
// CAN vételi task handle.
static TaskHandle_t canRxTaskHandle = nullptr;


void canRxTask(void *pvParameters) {
    // A task paramétereként átadott BLEManager pointer visszaállítása.
    BLEManager* bleManagerPtr = static_cast<BLEManager*>(pvParameters);

    // Queue létrehozása: legfeljebb 100 db twai_message_t elem.
    if (canRxQueue == NULL) {
        canRxQueue = xQueueCreate(100, sizeof(twai_message_t));
    }
    twai_message_t rx_msg;
    
    while(1) {
        // Event-based: ha a WiFi AP már nem fut, a CAN task leáll.
        if (!bleManagerPtr->isWifiStarted()) {
            break;
        }

        // Csak aktív WiFi kliens esetén olvassunk CAN-t,
        if (bleManagerPtr->iswifiactive()) { 
            
            // Olvasás a TWAI vezérlőből 10 ms timeouttal.
            if (twai_receive(&rx_msg, pdMS_TO_TICKS(10)) == ESP_OK) {
                xQueueSend(canRxQueue, &rx_msg, 0);
            }
            
        } else {

            vTaskDelay(pdMS_TO_TICKS(100));
            
            // Queue ürítése,
            if (canRxQueue != NULL) {
                xQueueReset(canRxQueue);
            }
        }
    }

    // Task leállítása előtt ürítjük a queue-t, hogy ne maradjanak régi üzenetek.
    if (canRxQueue != NULL) {
        xQueueReset(canRxQueue);
    }
    // Task handle nullázása és task törlése.
    canRxTaskHandle = nullptr;
    vTaskDelete(nullptr);
}

// CAN vételi task indítása az 1-es magon.
// Stack: 4096 byte, prioritás: 5.
void startCanTask(BLEManager* manager) {
    if (canRxTaskHandle != nullptr) {
        return;
    }

    xTaskCreatePinnedToCore(
        canRxTask, 
        "CAN_RX", 
        4096, 
        manager,
        5, 
        &canRxTaskHandle,
        1
    );
}


// CAN vételi task leállítása és a queue ürítése.
void stopCanTask() {
    if (canRxTaskHandle != nullptr) {
        vTaskDelete(canRxTaskHandle);
        canRxTaskHandle = nullptr;
    }

    if (canRxQueue != NULL) {
        xQueueReset(canRxQueue);
    }
}

// TWAI (CAN) periféria inicializálása a kért sebességgel.
// Támogatott sebességek: 125k, 250k, 500k, 1M bit/s.
bool initCan(uint32_t speed) {
    //TWAI konfiguráció
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        CAN_TX, CAN_RX, TWAI_MODE_NORMAL);

    // Időzítési konfiguráció kiválasztása baudrate alapján.
    twai_timing_config_t t_config;
    switch (speed) {
        case 125000:  t_config = TWAI_TIMING_CONFIG_125KBITS(); break;
        case 250000:  t_config = TWAI_TIMING_CONFIG_250KBITS(); break;
        case 500000:  t_config = TWAI_TIMING_CONFIG_500KBITS(); break;
        case 1000000: t_config = TWAI_TIMING_CONFIG_1MBITS(); break;
        default: return false;
    }

    // Szűrő: minden bejövő CAN keretet elfogad.
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    // TWAI indítása.
    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) return false;
    if (twai_start() != ESP_OK) return false;

    return true;
}
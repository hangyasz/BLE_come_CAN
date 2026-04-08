#include <Arduino.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "BleDevices.h"
#include "BleDeviceStore.h"
#include "NimBLEManager.h"

// Global BLEManager instance
static BLEManager bleManager;
static BleDevices bleDevices;


static BLEManager* g_bleManager = nullptr;
static TaskHandle_t g_bleTickTaskHandle = nullptr;



void bleTickTask(void* pvParameters) {
    (void)pvParameters;
    Serial.println("[BLE-TICK-TASK] Task indítva");
    
    while (1) {
        if (!g_bleManager) {
            break;
        }

        if (!g_bleManager->isPairingActive()) {
            Serial.println("[BLE-TICK-TASK] Pairing vege - task leall");
            break;
        }

        g_bleManager->tick();
        vTaskDelay(pdMS_TO_TICKS(100));  // 100ms ellenőrzés
    }

    g_bleTickTaskHandle = nullptr;
    vTaskDelete(nullptr);
}

static void startBleTickTaskIfNeeded() {
    if (g_bleTickTaskHandle != nullptr) {
        return;
    }

    xTaskCreatePinnedToCore(
        bleTickTask,
        "BLE-Tick-Task",
        4096,
        nullptr,
        1,
        &g_bleTickTaskHandle,
        1
    );
}

#define butonPin 0



/**
 * @brief Arduino setup() - egyszer fut le induláskor
 */
void setup() {
    // Soros kommunikáció inicializálása nyomkövetéshez
    Serial.begin(115200);
    pinMode(butonPin, INPUT_PULLUP);
    delay(100);

    g_bleManager = &bleManager;

    
    Serial.println("\n\n========================================");
    Serial.println("[SYSTEM] ESP32 BLE Device - Startup");
    Serial.println("========================================\n");

    bleDevices.init();


    bleManager.setDeviceRegistry(&bleDevices);

    // BLE Manager inicializálása
    bleManager.init();
    
    Serial.println("[SYSTEM] Setup() kész");
}


void loop() {

  if(digitalRead(butonPin) == LOW) {
    delay(1000); // Debounce
    
    // Hosszú nyomás (>2s) = BOND lista törlése
    // Rövid nyomás = Hirdetés indítása
    
    uint32_t pressTime = millis();
    while(digitalRead(butonPin) == LOW) {
        delay(10);
    }
    uint32_t pressDuration = millis() - pressTime;
    
    if (pressDuration > 2000) {
        Serial.println("[SYSTEM] HOSSZÚ nyomás detektálva - BOND lista törlése...");
        bleManager.clearBonds();
    } else {
        Serial.println("[SYSTEM] Rövid nyomás detektálva - Hirdetés indítása...");
        bleManager.startBLE();
        startBleTickTaskIfNeeded();
    }
  }
}

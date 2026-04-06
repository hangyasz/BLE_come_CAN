#include <Arduino.h>
#include <nvs_flash.h>
#include "BLE_come.h"
#include "BleDevices.h"
#include "BleDeviceStore.h"

// Global BLEManager instance
static BLEManager bleManager;
static BleDevices bleDevices;

#define butonPin 0



/**
 * @brief Arduino setup() - egyszer fut le induláskor
 */
void setup() {
    // Soros kommunikáció inicializálása nyomkövetéshez
    Serial.begin(115200);
    pinMode(butonPin, INPUT_PULLUP);
    delay(100);
    
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
  bleManager.tick();

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
    }
  }
}

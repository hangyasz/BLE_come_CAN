

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "NimBLEManager.h"
#include "Config.h"
#include "CanTask.h"

// Boot gomb pin
#define BUTTON_PIN 0

// A BLEManager felel a BLE kommunikációért és parancsokért.
static BLEManager bleManager;
// A párosított eszközök tárolója.
static BleDevices bleDevices;
// Task handle a BLE tick
static TaskHandle_t g_bleTickTaskHandle = nullptr;


// BLE tick task ezel kezelkül a párosítási állapotot.
void bleTickTask(void* pvParameters)
{
    BLEManager* mgr = static_cast<BLEManager*>(pvParameters);
    Serial.println("[BLE-TICK] Task inditva");

    while (true)
    {
        mgr->tick();
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Idde nem kellene soha eljutni de azérték nullázása és task törlése biztonsági okokból
    TaskHandle_t h = g_bleTickTaskHandle;
    g_bleTickTaskHandle = nullptr;
    vTaskDelete(h);
}


// Indítja a BLE tick task-ot
static void startBleTickTask()
{
    if (g_bleTickTaskHandle != nullptr) return;

    xTaskCreatePinnedToCore(
        bleTickTask,
        "BLE-Tick",
        4096,
        &bleManager,
        1,
        &g_bleTickTaskHandle,
        0);
}


void setup()
{
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    delay(100);

    Serial.println("\nStartup");

    // Adatbázis inicializálása a párosított eszközökhöz
    bleDevices.init();
    bleManager.setDeviceRegistry(&bleDevices);

    // CAN küldés callback: a BLEManager a bejövő CAN-küldési parancsoknál
    bleManager.setCanSendCallback([](uint32_t id, bool ext, uint8_t len, uint8_t* data) -> bool {
        twai_message_t msg = {};
        msg.extd             = ext ? 1 : 0; // extended flag
        msg.identifier       = id;
        msg.data_length_code = len;
        for (uint8_t i = 0; i < len; i++) msg.data[i] = data[i];
        // Visszatérési érték: sikeres küldés esetén true
        return twai_transmit(&msg, pdMS_TO_TICKS(10)) == ESP_OK;
    });


    // BLE inicializáció
    bleManager.init();

    // Indítjuk a BLE tick task-ot
    startBleTickTask();

    // CAN bus inicializálása; ha nem sikerül, ismételten próbáljuk
    while (!initCan(PERIODIC_CAN_PERIOD_US)) {
        Serial.println("[SYSTEM] Failed to initialize CAN bus");
        delay(1000);
    }

    Serial.println("[SYSTEM] Kesz");
}

// A loop csak a gombot figyeli
void loop()
{
    // ha nincs gombnyomás, nincs teendő
    if (digitalRead(BUTTON_PIN) == HIGH)
    {
        return;
    }

    // Gombnyomás időkezdete
    uint32_t pressStart = millis();

    // Várunk amíg felengedik a gombot
    while (digitalRead(BUTTON_PIN) == LOW)
    {
        delay(10);
    }

    // Gombnyomás időtartama
    uint32_t duration = millis() - pressStart;

    // Hosszú nyomás (>2000ms): töröljük az összes bond-ot
    if (duration > 2000)
    {
        Serial.println("[BTN] Hosszu nyomas - bond torlese");
        bleManager.clearBonds();
    }
    else
    {
        // Rövid nyomás: párosítási ablak megnyitása (pairing)
        Serial.println("[BTN] Rovid nyomas - pairing indit");
        bleManager.startPairing();
    }
}
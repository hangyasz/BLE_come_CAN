#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "BleDevices.h"
#include "NimBLEManager.h"
#include "config.h"

#define BUTTON_PIN 0

static BLEManager bleManager;
static BleDevices bleDevices;
static TaskHandle_t g_bleTickTaskHandle = nullptr;

// ── BLE tick task ──────────────────────────────────────────────
void bleTickTask(void* pvParameters)
{
    BLEManager* mgr = static_cast<BLEManager*>(pvParameters);
    Serial.println("[BLE-TICK] Task inditva");

    while (true)
    {
        mgr->tick();
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Ide sosem jut el, de biztonság kedvéért:
    TaskHandle_t h = g_bleTickTaskHandle;
    g_bleTickTaskHandle = nullptr;
    vTaskDelete(h);
}

static void startBleTickTask()
{
    if (g_bleTickTaskHandle != nullptr) return;

    xTaskCreatePinnedToCore(
        bleTickTask,
        "BLE-Tick",
        4096,
        &bleManager,   // ✅ this helyett pointer átadás
        1,
        &g_bleTickTaskHandle,
        1);
}


bool initCan(uint32_t speed) {
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
      CAN_TX, CAN_RX, TWAI_MODE_NORMAL);

  twai_timing_config_t t_config;
  switch (speed) {
    case 125000:
      t_config = TWAI_TIMING_CONFIG_125KBITS();
      break;
    case 250000:
      t_config = TWAI_TIMING_CONFIG_250KBITS();
      break;
    case 500000:
      t_config = TWAI_TIMING_CONFIG_500KBITS();
      break;
    case 1000000:
      t_config = TWAI_TIMING_CONFIG_1MBITS();
      break;
    default:
      return false;
  }

  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) return false;
  if (twai_start() != ESP_OK) return false;

  return true;
}

// ── Setup ──────────────────────────────────────────────────────
void setup()
{
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    delay(100);

    Serial.println("\n[SYSTEM] Startup");

    bleDevices.init();
    bleManager.setDeviceRegistry(&bleDevices);

    // ✅ CAN küldés callback regisztrálása
    bleManager.setCanSendCallback([](uint32_t id, bool ext, uint8_t len, uint8_t* data) -> bool {
        twai_message_t msg = {};
        msg.extd             = ext ? 1 : 0;
        msg.identifier       = id;
        msg.data_length_code = len;
        for (uint8_t i = 0; i < len; i++) msg.data[i] = data[i];
        return twai_transmit(&msg, pdMS_TO_TICKS(10)) == ESP_OK;
    });


    bleManager.init();

    // ✅ Tick task azonnal indul, mindig fut
    startBleTickTask();

    while (!initCan(PERIODIC_CAN_PERIOD_US)) {
        Serial.println("[SYSTEM] Failed to initialize CAN bus");
        delay(1000);
    }


    Serial.println("[SYSTEM] Kesz");
}

// ── Loop ───────────────────────────────────────────────────────
void loop()
{
    if (digitalRead(BUTTON_PIN) == HIGH)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
        return;
    }

    // ✅ Helyes debounce: nyomás kezdetétől mérünk
    uint32_t pressStart = millis();
    delay(50); // debounce

    while (digitalRead(BUTTON_PIN) == LOW)
    {
        delay(10);
    }

    uint32_t duration = millis() - pressStart;

    if (duration > 2000)
    {
        Serial.println("[BTN] Hosszu nyomas - bond torlese");
        bleManager.clearBonds();
    }
    else
    {
        Serial.println("[BTN] Rovid nyomas - pairing indit");
        bleManager.startPairing(); // ✅ javított elírás
    }
}
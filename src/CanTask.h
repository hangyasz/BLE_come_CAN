#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <driver/twai.h>
#include "NimBLEManager.h" 
//A Queue globálisan elérhető 
extern QueueHandle_t canRxQueue;

// CAN periféria inicializálása a megadott sebességgel (125k, 250k, 500k, 1M).
bool initCan(uint32_t speed);
// CAN vételi task indítása az 1-es magon.
void startCanTask(BLEManager* manager);
// CAN vételi task leállítása
void stopCanTask();
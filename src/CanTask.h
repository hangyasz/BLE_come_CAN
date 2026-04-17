#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <driver/twai.h>
#include "NimBLEManager.h" // Szükség van rá, hogy ismerje a BLEManager típust

// 📌 A Queue globálisan elérhető (extern), így a WifiBridge is tud belőle olvasni
extern QueueHandle_t canRxQueue;

// Függvények deklarációi
bool initCan(uint32_t speed);
void startCanTask(BLEManager* manager); // Fontos: Kéri a manager pointerét!
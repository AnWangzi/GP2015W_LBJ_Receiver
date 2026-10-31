#ifndef BATTERY_H
#define BATTERY_H
#include "battery_model.h"
typedef struct {
    uint16_t raw;
    uint8_t level, valid, external, charging;
    uint32_t errors;
} BatteryReading;
extern BatteryReading g_battery;
void Battery_Init(uint32_t now);
void Battery_Task(uint32_t now);
void Battery_Stop(void);
#endif

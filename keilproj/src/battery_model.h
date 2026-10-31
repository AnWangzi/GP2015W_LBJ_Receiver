#ifndef BATTERY_MODEL_H
#define BATTERY_MODEL_H
#include <stdint.h>
typedef enum { BatterySupply, BatteryCharging, BatteryFull, BatteryExternal } ChargeStatus;
unsigned Battery_Level(uint16_t raw);
ChargeStatus Battery_Status(int external, int charging, int valid, unsigned level);
#endif

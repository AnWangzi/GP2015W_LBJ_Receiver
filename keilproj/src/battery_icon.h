#ifndef BATTERY_ICON_H
#define BATTERY_ICON_H
#include <stdint.h>
/* 32x16, one vertical 16-bit column. 24-pixel battery plus status symbol. */
uint16_t BatteryIcon_Column(unsigned x, int valid, unsigned level,
                            int external, int charging, int phase);
#endif

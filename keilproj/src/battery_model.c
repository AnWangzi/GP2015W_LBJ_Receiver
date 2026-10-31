#include "battery_model.h"

unsigned Battery_Level(uint16_t raw)
{
    /* Original 0x4478..0x44B8: compare strictly greater than each threshold. */
    if (raw > 3264) return 5;
    if (raw > 3152) return 4;
    if (raw > 3088) return 3;
    if (raw > 3008) return 2;
    if (raw > 2816) return 1;
    return 0;
}

ChargeStatus Battery_Status(int external, int charging, int valid, unsigned level)
{
    if (!external) return BatterySupply;
    if (charging) return BatteryCharging;
    if (valid && level == 5) return BatteryFull;
    return BatteryExternal; /* Inactive charge pin alone does not prove full. */
}

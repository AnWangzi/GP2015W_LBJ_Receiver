#include "battery_icon.h"

uint16_t BatteryIcon_Column(unsigned x, int valid, unsigned level,
                            int external, int charging, int phase)
{
    unsigned b;
    uint16_t pixels = 0;
    /* Six-pixel lightning/plug marker. Charge blink never changes true bars. */
    static const uint16_t bolt[6] = {0x0100, 0x39C0, 0x1FE0, 0x07F8, 0x01CC, 0x0040};
    static const uint16_t plug[6] = {0x0060, 0x01F8, 0x0FE0, 0x0FE0, 0x01F8, 0x0060};
    if (x < 6) return !external ? 0 : charging ? (phase ? bolt[x] : 0) : plug[x];
    if (x < 8 || x >= 32) return 0;
    b = x - 8;
    /* Rounded corners, single-pixel outline, centered terminal. */
    if (b == 0 || b == 21) pixels = 0x0FF0;
    else if (b == 1 || b == 20) pixels = 0x1008;
    else if (b < 20) pixels = 0x1008;
    else pixels = 0x03C0;
    if (valid && b >= 2 && b < 20 && (b - 2) % 4 < 2 && (b - 2) / 4 < level)
        pixels |= 0x07E0;
    /* Unknown is a centered dash, distinct from a confirmed empty battery. */
    if (!valid && b >= 9 && b <= 12) pixels |= 0x0180;
    return pixels;
}

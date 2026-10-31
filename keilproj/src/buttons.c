#include "buttons.h"
#include <string.h>

void PowerHold_Init(PowerHold *s) { memset(s, 0, sizeof(*s)); }

int PowerHold_Update(PowerHold *s, int pressed, uint32_t now)
{
    if (!pressed) { s->active = s->fired = 0; return 0; }
    if (!s->active) { s->active = 1; s->since = now; }
    if (!s->fired && (uint32_t)(now - s->since) >= POWER_HOLD_MS) {
        s->fired = 1;
        return 1;
    }
    return 0;
}

void Buttons_Init(Buttons *s, uint8_t pressed, uint32_t now)
{
    unsigned i;
    memset(s, 0, sizeof(*s));
    s->raw = s->stable = pressed & 15u;
    s->armed = (uint8_t)(~pressed & 15u);
    for (i = 0; i < BUTTON_COUNT; ++i) s->changed[i] = s->down[i] = now;
}

uint16_t Buttons_Update(Buttons *s, uint8_t pressed, uint32_t now)
{
    unsigned i;
    uint8_t bit;
    uint16_t events = 0;
    pressed &= 15u;
    for (i = 0; i < BUTTON_COUNT; ++i) {
        bit = (uint8_t)(1u << i);
        if ((pressed ^ s->raw) & bit) {
            s->raw ^= bit;
            s->changed[i] = now;
        }
        if (((pressed ^ s->stable) & bit) &&
            (uint32_t)(now - s->changed[i]) >= BUTTON_DEBOUNCE_MS) {
            s->stable ^= bit;
            if (pressed & bit) {
                s->down[i] = now;
                s->long_sent &= (uint8_t)~bit;
                if (s->armed & bit) events |= BUTTON_PRESS(i);
            } else {
                if ((s->armed & bit) && !(s->long_sent & bit))
                    events |= BUTTON_SHORT(i);
                s->armed |= bit;
            }
        }
        if ((s->stable & s->raw & s->armed & bit) && !(s->long_sent & bit) &&
            (uint32_t)(now - s->down[i]) >= BUTTON_LONG_MS) {
            s->long_sent |= bit;
            events |= BUTTON_LONG(i);
        }
    }
    return events;
}

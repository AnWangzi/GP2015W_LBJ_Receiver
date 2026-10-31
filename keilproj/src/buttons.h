#ifndef BUTTONS_H
#define BUTTONS_H
#include <stdint.h>

#define BUTTON_COUNT 4
#define BUTTON_PRESS(i) (1u << (i))
#define BUTTON_SHORT(i) (1u << ((i) + 4))
#define BUTTON_LONG(i)  (1u << ((i) + 8))
#define BUTTON_DEBOUNCE_MS 30u
#define BUTTON_LONG_MS 1200u
#define POWER_HOLD_MS 1500u

typedef struct {
    uint32_t changed[BUTTON_COUNT], down[BUTTON_COUNT];
    uint8_t raw, stable, armed, long_sent;
} Buttons;

/* pressed is an active-high logical mask. Ignore keys held across boot. */
void Buttons_Init(Buttons *s, uint8_t pressed, uint32_t now);
uint16_t Buttons_Update(Buttons *s, uint8_t pressed, uint32_t now);

/* Separate from normal keys: a green key already held at cold boot is valid. */
typedef struct { uint32_t since; uint8_t active, fired; } PowerHold;
void PowerHold_Init(PowerHold *s);
int PowerHold_Update(PowerHold *s, int pressed, uint32_t now);
#endif

#ifndef POWER_SLEEP_H
#define POWER_SLEEP_H
/* Software-off only. Requires PB07 pull-up/edge IRQ and PORTB NVIC enabled.
 * Stops SysTick, enters DeepSleep only while green is released, then restores
 * Active/Sleep IRQ mode and the caller's interrupt mask before returning.
 * Current firmware uses RCH 4 MHz, automatically restarted after DeepSleep. */
void PowerSleep_Stop(void);
#endif

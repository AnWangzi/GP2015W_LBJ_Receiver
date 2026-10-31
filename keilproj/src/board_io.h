#ifndef BOARD_IO_H
#define BOARD_IO_H
#include <stdint.h>

typedef enum { LightAuto, LightOn, LightOff } LightMode;
void Board_Init(uint32_t now);
/* Call frequently from foreground; no blocking delays or per-tone IRQ. */
uint16_t Board_Task(uint32_t now);
void Board_WakeLight(uint32_t now);
void Board_TestLight(uint32_t now, uint32_t duration_ms);
void Board_SetLightMode(LightMode mode, uint32_t now);
LightMode Board_GetLightMode(void);
uint8_t Board_ReadKeyCandidates(void);
void Board_Beep(unsigned hz, unsigned on_ms, unsigned count, unsigned gap_ms, uint32_t now);
void Board_StopBeep(void);
void Board_Standby(void);
int Board_GreenPressed(void);
#endif

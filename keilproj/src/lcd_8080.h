#ifndef __LCD_8080_H__
#define __LCD_8080_H__

#include <stdint.h>

/* 引脚来自原厂固件反汇编（不是猜的）：
 *   RS(A0) = PA09
 *   /WR    = PA12
 *   /RESET = PA11 （低有效）
 *   /CS    = 硬件接地，固件从不驱动
 *   PA10   = 固件未使用
 *
 *   数据: D0=PA08 D1=PC09 D2=PC07 D3=PC06
 *         D4=PB15 D5=PB14 D6=PB13 D7=PB12
 */
void LCD_Init(void);
void LCD_Start(uint32_t now);
int LCD_PowerTask(uint32_t now);
void LCD_TestPattern(void);
void LCD_Clear(void);
void LCD_SetPos(int page, int col);
void LCD_WriteCmd(uint8_t c);
void LCD_WriteData(uint8_t d);
/* Drawing only changes RAM. Flush a bounded amount from the main loop. */
unsigned LCD_FlushStep(unsigned byte_budget);
void LCD_FlushAll(void);

#endif

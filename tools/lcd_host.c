/* Host adapter for the actual firmware glyph renderer. */
#include "lcd_buffer.h"
#include <stdint.h>
#include <string.h>
void LCD_SetPos(int page,int col) { LcdBuffer_Pos(page,col); }
void LCD_WriteData(uint8_t d) { LcdBuffer_Write(d); }
static uint8_t screen[1024];
void LCD_HostReset(void) { memset(screen,0,sizeof(screen)); LcdBuffer_Reset(); }
static void sink(unsigned p,unsigned c,const uint8_t *d,unsigned n)
{ unsigned i; for(i=0;i<n;++i) screen[p*128+c+i]=d[i]; }
const uint8_t *LCD_HostSnapshot(void)
{ while(LcdBuffer_Dirty()) LcdBuffer_Flush(16,128,sink); return screen; }

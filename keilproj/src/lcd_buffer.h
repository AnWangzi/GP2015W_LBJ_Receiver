#ifndef LCD_BUFFER_H
#define LCD_BUFFER_H
#include <stdint.h>
typedef void (*LcdSpanWriter)(unsigned page, unsigned col, const uint8_t *data, unsigned len);
void LcdBuffer_Reset(void);
void LcdBuffer_Invalidate(void);
void LcdBuffer_Pos(int page, int col);
void LcdBuffer_Write(uint8_t value);
void LcdBuffer_InvertRow(int page);
/* Hard limits on both bytes transferred and pixels examined per call. */
unsigned LcdBuffer_Flush(unsigned budget, unsigned scan_budget, LcdSpanWriter writer);
int LcdBuffer_Dirty(void);
#endif

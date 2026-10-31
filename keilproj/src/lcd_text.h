#ifndef __LCD_TEXT_H__
#define __LCD_TEXT_H__

#include <stdint.h>

/* 16x16 点阵文字。坐标以 (page, col) 给出，一个字形占 2 页 × 16 列 */
void LCD_DrawGlyph16(int page, int col, uint16_t cp);

/* 半角字符：只画字模左 8 列，步进 8 像素（一行 16 个） */
void LCD_DrawGlyph8(int page, int col, uint8_t ch);
void LCD_DrawAscii8(int page, int col, const char *s);
void LCD_DrawStr16(int page, int col, const uint16_t *s);
void LCD_DrawUtf8(int page, int col, const char *s);

#endif

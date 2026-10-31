/* ============================================================================
 * lcd_text.c — 16x16 点阵文字渲染
 *
 * 字模来自 font_data.c（由 GNU Unifont 生成）。
 * 屏是"页"结构：每页 8 行像素，一个 16x16 字形横跨 2 页。
 * 需要把行优先的字模转成列优先的页字节。
 * ==========================================================================*/

#include "lcd_8080.h"
#include "lcd_text.h"

extern const uint16_t g_font_cp[];
extern const uint8_t  g_font_columns[][32];
extern const uint16_t g_font_count;

/* 二分查找字形，找不到返回 0 */
static const uint8_t *font_find(uint16_t cp)
{
    int lo = 0;
    int hi = (int)g_font_count - 1;

    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        if (g_font_cp[mid] == cp) {
            return g_font_columns[mid];
        }
        if (g_font_cp[mid] < cp) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return 0;
}

void LCD_DrawGlyph16(int page, int col, uint16_t cp)
{
    const uint8_t *g = font_find(cp);
    int half, c;

    if (g == 0) {
        return;
    }

    for (half = 0; half < 2; half++) {
        LCD_SetPos(page + half, col);
        for (c = 0; c < 16; c++) {
            LCD_WriteData(g[half * 16 + c]);
        }
    }
}

void LCD_DrawGlyph8(int page, int col, uint8_t ch)
{
    const uint8_t *g = font_find(ch);
    int half, c;

    if (g == 0) {
        return;
    }
    for (half = 0; half < 2; half++) {
        LCD_SetPos(page + half, col);
        for (c = 0; c < 8; c++) {
            LCD_WriteData(g[half * 16 + c]);
        }
    }
}

void LCD_DrawAscii8(int page, int col, const char *s)
{
    while (*s != 0) {
        if (col > 128 - 8) {
            break;
        }
        LCD_DrawGlyph8(page, col, (uint8_t)*s);
        col += 8;
        s++;
    }
}

void LCD_DrawStr16(int page, int col, const uint16_t *s)
{
    while (*s != 0) {
        if (col > 128 - 16) {
            break;
        }
        LCD_DrawGlyph16(page, col, *s);
        col += 16;
        s++;
    }
}

void LCD_DrawUtf8(int page, int col, const char *s)
{
    const uint8_t *p = (const uint8_t *)s;
    while (*p) {
        uint16_t cp;
        unsigned width;
        if (*p < 0x80) { cp = *p++; width = 8; }
        else if ((*p & 0xF0) == 0xE0 && p[1] && p[2] &&
                 (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
            cp = (uint16_t)(((*p & 15) << 12) | ((p[1] & 63) << 6) | (p[2] & 63));
            p += 3; width = 16;
        } else { cp = '?'; ++p; width = 8; }
        if (col < 0 || col + (int)width > 128) break;
        if (!font_find(cp)) { cp = '?'; width = 8; }
        if (width == 8) LCD_DrawGlyph8(page, col, (uint8_t)cp);
        else LCD_DrawGlyph16(page, col, cp);
        col += (int)width;
    }
}

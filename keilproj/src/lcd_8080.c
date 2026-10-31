/* ============================================================================
 * lcd_8080.c — 8 位并口点阵 LCD 驱动（GPIO 位操作，只写）
 *
 * ★ 引脚全部来自原厂固件反汇编，不是猜的 ★
 *
 *   数据位（函数 0x8EF4 逐位展开）：
 *     bit0 -> PA08   bit1 -> PC09   bit2 -> PC07   bit3 -> PC06
 *     bit4 -> PB15   bit5 -> PB14   bit6 -> PB13   bit7 -> PB12
 *
 *   控制（函数 0x5050 初始化 + 0x8EF4 写入 + 0x2B6E 绘图）：
 *     PA12 = /WR     0x8FA8: CLR -> nop -> SET   （每个字节脉冲一次）
 *     PA11 = /RESET  0x505A: CLR -> 10ms -> SET -> 2ms
 *     PA09 = RS(A0)  RS=0 发命令，RS=1 发数据（/CS 硬件接地，固件不驱动）
 *     PA10 = 固件从未使用（不是 LCD 信号，之前是量错了）
 *
 *   初始化序列（函数 0x5050）：
 *     A0 C8 A2 24 81 <contrast> 2C 2E 2F AF
 *   注意：没有 0xE2 软复位 —— 厂家用的是硬件复位脉冲。
 *
 *   寻址（函数 0x77E4 / 0x77CE）：
 *     页   : 0xB0 | page
 *     列高 : 0x10 | (col >> 4)
 *     列低 : col & 0x0F
 * ==========================================================================*/

#include "hc32l17x.h"
#include "gpio.h"
#include "sysctrl.h"
#include "lcd_8080.h"
#include "lcd_buffer.h"

typedef struct {
    en_gpio_port_t port;
    en_gpio_pin_t  pin;
} PinDef;

/* D0..D7 */
static const PinDef DATA_PINS[8] = {
    { GpioPortA, GpioPin8  },   /* D0  PA08 */
    { GpioPortC, GpioPin9  },   /* D1  PC09 */
    { GpioPortC, GpioPin7  },   /* D2  PC07 */
    { GpioPortC, GpioPin6  },   /* D3  PC06 */
    { GpioPortB, GpioPin15 },   /* D4  PB15 */
    { GpioPortB, GpioPin14 },   /* D5  PB14 */
    { GpioPortB, GpioPin13 },   /* D6  PB13 */
    { GpioPortB, GpioPin12 },   /* D7  PB12 */
};

#define RS_PORT   GpioPortA
#define RS_PIN    GpioPin9      /* PA09 */

#define WR_PORT   GpioPortA
#define WR_PIN    GpioPin12     /* PA12 */

#define RST_PORT  GpioPortA
#define RST_PIN   GpioPin11     /* PA11 */

/* 对比度：原厂 = 配置字节[0x0B] + 4，这里先用 0x20 */
#define LCD_CONTRAST  0x20

/* --------------------------- 基础 -------------------------------------- */
static void pin_out(en_gpio_port_t port, en_gpio_pin_t pin)
{
    stc_gpio_cfg_t cfg;
    DDL_ZERO_STRUCT(cfg);
    cfg.bOutputVal = TRUE;
    cfg.enDir      = GpioDirOut;
    cfg.enDrv      = GpioDrvH;
    cfg.enPu       = GpioPuDisable;
    cfg.enPd       = GpioPdDisable;
    cfg.enOD       = GpioOdDisable;
    cfg.enCtrlMode = GpioFastIO;
    Gpio_Init(port, pin, &cfg);
}

static uint8_t power_phase;
static uint32_t power_since;

void LCD_Init(void)
{
    int i;

    Sysctrl_SetPeripheralGate(SysctrlPeripheralGpio, TRUE);

    for (i = 0; i < 8; i++) {
        pin_out(DATA_PINS[i].port, DATA_PINS[i].pin);
    }
    pin_out(RS_PORT, RS_PIN);
    pin_out(WR_PORT, WR_PIN);
    pin_out(RST_PORT, RST_PIN);

    /* 空闲态 */
    Gpio_SetIO(WR_PORT, WR_PIN);
    Gpio_SetIO(RST_PORT, RST_PIN);
    Gpio_ClrIO(RS_PORT, RS_PIN);

    power_phase = 0;
}

/* --------------------------- 总线 -------------------------------------- */
static void bus_write(uint8_t b)
{
    uint32_t a = (uint32_t)(b & 1u) << 8;
    uint32_t pb = ((uint32_t)(b & 0x10u) << 11) | ((uint32_t)(b & 0x20u) << 9) |
                  ((uint32_t)(b & 0x40u) << 7) | ((uint32_t)(b & 0x80u) << 5);
    uint32_t pc = ((uint32_t)(b & 2u) << 8) | ((uint32_t)(b & 4u) << 5) |
                  ((uint32_t)(b & 8u) << 3);
    /* Only data pins: preserve keys, radio, backlight and buzzer on these ports. */
    M0P_GPIO->PABCLR = (1u << 8) & ~a; M0P_GPIO->PABSET = a;
    M0P_GPIO->PBBCLR = 0xF000u & ~pb; M0P_GPIO->PBBSET = pb;
    M0P_GPIO->PCBCLR = 0x02C0u & ~pc; M0P_GPIO->PCBSET = pc;
}

static void lcd_write(uint8_t v, int rs)
{
    if (rs) {
        M0P_GPIO->PABSET = 1u << 9;
    } else {
        M0P_GPIO->PABCLR = 1u << 9;
    }

    bus_write(v);

    M0P_GPIO->PABCLR = 1u << 12;
    /* Original 0x8FA8 used one NOP between CLR and SET. Keep four. */
    __NOP(); __NOP(); __NOP(); __NOP();
    M0P_GPIO->PABSET = 1u << 12;
}

void LCD_WriteCmd(uint8_t c)  { lcd_write(c, 0); }
void LCD_WriteData(uint8_t d) { LcdBuffer_Write(d); }

/* ------------------------- 初始化序列 ---------------------------------- */
void LCD_Start(uint32_t now)
{
    Gpio_ClrIO(RST_PORT, RST_PIN);
    power_phase = 1; power_since = now;
}

int LCD_PowerTask(uint32_t now)
{
    if (power_phase == 1 && (uint32_t)(now-power_since) >= 10u) {
        Gpio_SetIO(RST_PORT, RST_PIN);
        power_phase = 2; power_since = now;
    } else if (power_phase == 2 && (uint32_t)(now-power_since) >= 2u) {
    LCD_WriteCmd(0xA0);          /* ADC 选择：正常 */
    LCD_WriteCmd(0xC8);          /* COM 扫描方向：反向 */
    LCD_WriteCmd(0xA2);          /* 偏压 1/9 */
    LCD_WriteCmd(0x24);          /* 电阻比 */
    LCD_WriteCmd(0x81);          /* 对比度设置 */
    LCD_WriteCmd(LCD_CONTRAST);
    LCD_WriteCmd(0x2C);          /* 升压 ON */
        power_phase = 3; power_since = now;
    } else if (power_phase == 3 && (uint32_t)(now-power_since) >= 2u) {
        LCD_WriteCmd(0x2E);
        power_phase = 4; power_since = now;
    } else if (power_phase == 4 && (uint32_t)(now-power_since) >= 2u) {
        LCD_WriteCmd(0x2F);
        power_phase = 5; power_since = now;
    } else if (power_phase == 5 && (uint32_t)(now-power_since) >= 2u) {
        LCD_Clear();
        power_phase = 6;
    }
    return power_phase >= 6;
}

/* --------------------------- 绘图 -------------------------------------- */
static void gotoxy(int page, int col)
{
    LCD_WriteCmd(0xB0 | (page & 0x0F));
    LCD_WriteCmd(0x10 | ((col >> 4) & 0x0F));
    LCD_WriteCmd(col & 0x0F);
}

void LCD_Clear(void)
{
    LcdBuffer_Reset();
    LcdBuffer_Invalidate();
}

/* 供文字模块使用的定位接口 */
void LCD_SetPos(int page, int col)
{
    LcdBuffer_Pos(page, col);
}

static void write_span(unsigned page,unsigned col,const uint8_t *data,unsigned len)
{
    gotoxy((int)page,(int)col);
    while (len--) lcd_write(*data++,1);
}
unsigned LCD_FlushStep(unsigned byte_budget)
{
    unsigned n;
    if(power_phase < 6) return 0;
    n=LcdBuffer_Flush(byte_budget,128u,write_span);
    if(power_phase==6 && !LcdBuffer_Dirty()) { LCD_WriteCmd(0xAF); power_phase=7; }
    return n;
}
void LCD_FlushAll(void)
{ while (power_phase >= 6 && LcdBuffer_Dirty()) LCD_FlushStep(128u); }

void LCD_TestPattern(void)
{
    int page, col;

    for (page = 0; page < 8; page++) {
        LCD_SetPos(page, 0);
        for (col = 0; col < 128; col++) {
            if (page == 0) {
                LCD_WriteData(0xFF);                       /* 第 1 页全黑 */
            } else if (page == 1) {
                LCD_WriteData((col & 1) ? 0xAA : 0x55);    /* 第 2 页竖条纹 */
            } else if (page == 2) {
                LCD_WriteData(0xFF);                       /* 第 3 页全黑 */
            } else {
                LCD_WriteData(0x00);
            }
        }
    }
    LCD_FlushAll();
}

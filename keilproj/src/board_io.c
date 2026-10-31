/* Verified against the original self-test at 0x71EA..0x7286:
 * "light self-test": PB03 low -> on, high -> off.
 * "vibration self-test": PA05 (not driven here).
 * "sound self-test": 0x5F04 -> 0x1728 selects PC08 AF1 = PCA_CH2.
 * 0x1780 / 0x5E18 configures PCA extended PWM, ECOM+MAT+TOG.
 * Key mapping verified by KEY TEST on the physical board:
 * green=PB07, up=PB06, down=PB05, red=PB04. All active-low with pull-up.
 */
#include "hc32l17x.h"
#include "gpio.h"
#include "sysctrl.h"
#include "buttons.h"
#include "board_io.h"

static Buttons keys;
static LightMode light_mode;
static uint32_t light_since, tone_since, last_scan, pca_clock;
static unsigned tone_hz, tone_on_ms, tone_gap_ms, tone_left;
static uint8_t tone_on;
static uint8_t lamp_on = 255;
static uint8_t light_test;
static uint32_t light_test_since, light_test_ms;

static uint8_t raw_keys(void)
{
    uint32_t b = M0P_GPIO->PBIN;
    return (uint8_t)((!(b & (1u << 7))) | ((!(b & (1u << 6))) << 1) |
                     ((!(b & (1u << 5))) << 2) | ((!(b & (1u << 4))) << 3));
}

uint8_t Board_ReadKeyCandidates(void)
{
    uint32_t b = M0P_GPIO->PBIN, c = M0P_GPIO->PCIN;
    return (uint8_t)((!(b & (1u << 4))) | ((!(b & (1u << 5))) << 1) |
        ((!(b & (1u << 6))) << 2) | ((!(b & (1u << 7))) << 3) |
        ((!(b & (1u << 8))) << 4) | ((!(b & (1u << 9))) << 5) |
        ((!(b & (1u << 11))) << 6) | ((!(c & 1u)) << 7));
}

static void pin_init(en_gpio_port_t port, en_gpio_pin_t pin, int input, int high)
{
    stc_gpio_cfg_t cfg;
    DDL_ZERO_STRUCT(cfg);
    cfg.bOutputVal = high ? TRUE : FALSE;
    cfg.enDir = input ? GpioDirIn : GpioDirOut;
    cfg.enPu = input ? GpioPuEnable : GpioPuDisable;
    cfg.enPd = GpioPdDisable;
    cfg.enOD = GpioOdDisable;
    cfg.enDrv = GpioDrvH;
    cfg.enCtrlMode = GpioFastIO;
    Gpio_Init(port, pin, &cfg);
}

static void lamp(int on)
{
    if (lamp_on == (uint8_t)on) return;
    /* Atomic set/clear, PB12..15 are the LCD data bus. */
    if (on) M0P_GPIO->PBBCLR = 1u << 3;
    else M0P_GPIO->PBBSET = 1u << 3;
    lamp_on = (uint8_t)on;
}

static void tone_off(void)
{
    M0P_PCA->CCON = 0;
    Gpio_SetAfMode(GpioPortC, GpioPin8, GpioAf0);
    M0P_GPIO->PCBCLR = 1u << 8;
    tone_on = 0;
}

static void tone_start(void)
{
    uint32_t period = pca_clock / tone_hz;
    if (period < 4) period = 4;
    if (period > 65535) period = 65535;
    M0P_PCA->CCON = 0;
    M0P_PCA->CNT = 0;
    M0P_PCA->CARR = period - 1;
    M0P_PCA->CCAP2 = period / 2;
    Gpio_SetAfMode(GpioPortC, GpioPin8, GpioAf1);
    M0P_PCA->CCON = 1u << 6;
    tone_on = 1;
}

void Board_Init(uint32_t now)
{
    Sysctrl_SetPeripheralGate(SysctrlPeripheralGpio, TRUE);
    pin_init(GpioPortB, GpioPin3, 0, 1);
    pin_init(GpioPortC, GpioPin8, 0, 0);
    /* Original key scan 0x4640 and input/IRQ setup 0x8516..0x856A. */
    pin_init(GpioPortB, GpioPin4, 1, 1);
    pin_init(GpioPortB, GpioPin5, 1, 1);
    pin_init(GpioPortB, GpioPin6, 1, 1);
    pin_init(GpioPortB, GpioPin7, 1, 1);
    Sysctrl_SetPeripheralGate(SysctrlPeripheralPca, TRUE);
    M0P_PCA->CCON = 0;
    M0P_PCA->CMOD = 2u << 1; /* CPS=2: PCLK /8, no watchdog or IRQ. */
    M0P_PCA->CCAPM2 = 0x4C; /* ECOM, MAT, TOG; original extended 16-bit PWM. */
    M0P_PCA->EPWM = 1;
    pca_clock = Sysctrl_GetPClkFreq() / 8;
    Buttons_Init(&keys, raw_keys(), now);
    last_scan = now;
    tone_left = 0;
    tone_off();
    Board_SetLightMode(LightAuto, now);
}

void Board_WakeLight(uint32_t now)
{
    /* Activity refreshes AUTO only. It must never edit the user's setting. */
    if (light_mode == LightAuto) {
        light_since = now;
        lamp(1);
    }
}

void Board_TestLight(uint32_t now, uint32_t duration_ms)
{
    light_test = duration_ms != 0;
    light_test_since = now;
    light_test_ms = duration_ms;
    if (light_test) lamp(1);
}

void Board_SetLightMode(LightMode mode, uint32_t now)
{
    light_mode = mode;
    light_test = 0;
    light_since = now;
    lamp(mode != LightOff);
}

LightMode Board_GetLightMode(void) { return light_mode; }

void Board_StopBeep(void)
{
    tone_left = 0;
    tone_off();
}

void Board_Standby(void)
{
    Board_StopBeep();
    Board_SetLightMode(LightOff, 0);
    Sysctrl_SetPeripheralGate(SysctrlPeripheralPca, FALSE);
}

int Board_GreenPressed(void)
{
    return !(M0P_GPIO->PBIN & (1u << 7));
}

void Board_Beep(unsigned hz, unsigned on_ms, unsigned count, unsigned gap_ms, uint32_t now)
{
    if (!hz || !on_ms || !count) { Board_StopBeep(); return; }
    tone_hz = hz;
    tone_on_ms = on_ms;
    tone_gap_ms = gap_ms;
    tone_left = count;
    tone_since = now;
    tone_start();
}

uint16_t Board_Task(uint32_t now)
{
    uint16_t events = 0;
    if (light_test && (uint32_t)(now - light_test_since) >= light_test_ms) light_test = 0;
    lamp(light_test || light_mode == LightOn ||
         (light_mode == LightAuto && (uint32_t)(now - light_since) < 20000u));
    if (tone_left) {
        if (tone_on && (uint32_t)(now - tone_since) >= tone_on_ms) {
            tone_off();
            --tone_left;
            tone_since = now;
        } else if (!tone_on && (uint32_t)(now - tone_since) >= tone_gap_ms) {
            tone_start();
            tone_since = now;
        }
    }
    if ((uint32_t)(now - last_scan) >= 5u) {
        last_scan = now;
        events = Buttons_Update(&keys, raw_keys(), now);
    }
    return events;
}

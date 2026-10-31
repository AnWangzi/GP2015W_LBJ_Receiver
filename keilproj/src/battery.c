/* Original firmware evidence:
 * 0x1638 calls 0x4880 (PA00 analog mode); 0x1640 selects ADC channel0.
 * ADC0 maps to PA00 in adc.h; this driver explicitly selects analog mode.
 * 0x15F8: 12-cycle sampling, internal2.5V reference, right aligned.
 * 0x4428: filter and 5-level thresholds.
 * 0x54C8 reads PC13 (external supply, high); 0x5536 reads PC00
 * (charge indication, low). PC13 input has no pull, PC00 has pull-up.
 */
#include "hc32l17x.h"
#include "gpio.h"
#include "sysctrl.h"
#include "battery.h"
#include <string.h>

BatteryReading g_battery;
static uint16_t readings[16];
static unsigned count, next;
static uint32_t sum, last_sample, conversion_since, charge_since;
static uint32_t init_since;
static uint8_t pending, charge_raw, init_phase;

static void charge_pin(en_gpio_pin_t pin, int pullup)
{
    stc_gpio_cfg_t cfg;
    DDL_ZERO_STRUCT(cfg);
    cfg.enDir = GpioDirIn;
    cfg.enPu = pullup ? GpioPuEnable : GpioPuDisable;
    cfg.enPd = GpioPdDisable;
    cfg.enCtrlMode = GpioFastIO;
    Gpio_Init(GpioPortC, pin, &cfg);
}

static uint8_t read_charge(void)
{
    uint32_t c = M0P_GPIO->PCIN;
    return (uint8_t)(((c >> 13) & 1u) | ((!(c & 1u)) << 1));
}

void Battery_Init(uint32_t now)
{
    memset(&g_battery, 0, sizeof(g_battery));
    memset(readings, 0, sizeof(readings));
    count = next = 0;
    sum = 0;
    pending = 0;
    last_sample = now - 250u;
    charge_pin(GpioPin13, 0);
    charge_pin(GpioPin0, 1);
    charge_raw = read_charge();
    charge_since = now;
    Gpio_SetAnalogMode(GpioPortA, GpioPin0);
    Sysctrl_SetPeripheralGate(SysctrlPeripheralAdcBgr, TRUE);
    M0P_ADC->CR0 = 0;
    M0P_BGR->CR |= 1u;
    init_phase = 1;
    init_since = now;
    /* DDL delay10us/delay1ms reprogram and disable SysTick! Let the
     * foreground tick supply two nonblocking reference settling periods. */
}

void Battery_Stop(void)
{
    M0P_ADC->SGLSTART = 0;
    M0P_ADC->CR0 = 0;
    M0P_BGR->CR &= ~1u;
    Sysctrl_SetPeripheralGate(SysctrlPeripheralAdcBgr, FALSE);
    pending = 0;
    init_phase = 0;
}

void Battery_Task(uint32_t now)
{
    uint8_t state = read_charge();
    uint16_t value;
    if (state != charge_raw) { charge_raw = state; charge_since = now; }
    if ((uint32_t)(now - charge_since) >= 100u) {
        g_battery.external = state & 1u;
        g_battery.charging = (uint8_t)((state >> 1) & 1u);
    }
    if (init_phase == 1) {
        if ((uint32_t)(now - init_since) >= 1u) {
            M0P_ADC->CR0 = 1u;
            init_phase = 2;
            init_since = now;
        }
        return;
    }
    if (init_phase == 2) {
        if ((uint32_t)(now - init_since) >= 1u) {
            /* adc.h: EN bit0, CLKDIV8 bits3:2, REF2p5 bit9,
             * SAMPLE12 bits13:12, INREF bit14. */
            M0P_ADC->CR0 = 0x720Du;
            M0P_ADC->CR1 = 0;
            M0P_ADC->ICR = ~1u;
            M0P_ADC->SGLSTART = 0;
            init_phase = 3;
        }
        return;
    }
    if (init_phase != 3) return;
    if (pending) {
        if (M0P_ADC->IFR & 1u) {
            value = (uint16_t)(M0P_ADC->RESULT & 4095u);
            M0P_ADC->ICR = ~1u;
            M0P_ADC->SGLSTART = 0;
            sum -= readings[next];
            readings[next] = value;
            sum += value;
            next = (next + 1) & 15u;
            if (count < 16) ++count;
            g_battery.raw = (uint16_t)(sum / count);
            g_battery.level = (uint8_t)Battery_Level(g_battery.raw);
            g_battery.valid = 1;
            pending = 0;
            last_sample = now;
        } else if ((uint32_t)(now - conversion_since) >= 20u) {
            M0P_ADC->SGLSTART = 0;
            pending = 0;
            last_sample = now;
            g_battery.valid = 0;
            ++g_battery.errors;
        }
    } else if ((uint32_t)(now - last_sample) >= 250u) {
        M0P_ADC->ICR = ~1u;
        M0P_ADC->SGLSTART = 1u;
        conversion_since = now;
        pending = 1;
    }
}

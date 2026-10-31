/* Regression for the real driver: DDL delays previously destroyed SysTick. */
#include <assert.h>
#include <stdio.h>
#include "hc32l17x.h"
#include "gpio.h"
#include "sysctrl.h"
#include "battery.h"
TestGpio test_gpio;
TestBgr test_bgr;
TestAdc test_adc;
TestSysTick test_tick = {7,3999,1234};
static int gate_on;
void Sysctrl_SetPeripheralGate(unsigned gate,int enable)
{ assert(gate == SysctrlPeripheralAdcBgr); gate_on=enable; }
void Gpio_Init(unsigned port,en_gpio_pin_t pin,stc_gpio_cfg_t *cfg)
{
    assert(port == GpioPortC && (pin == 0 || pin == 13));
    assert(cfg->enDir == GpioDirIn && !cfg->enPd);
    assert(cfg->enPu == (pin == 0));
}
void Gpio_SetAnalogMode(unsigned port,en_gpio_pin_t pin)
{ assert(port == GpioPortA && pin == 0); }
static void tick_unchanged(void)
{ assert(test_tick.CTRL==7 && test_tick.LOAD==3999 && test_tick.VAL==1234); }
int main(void)
{
    uint32_t t;
    test_gpio.PCIN=1; /* unplugged, inactive charging */
    Battery_Init(0); tick_unchanged();
    assert(gate_on && test_bgr.CR==1 && !test_adc.CR0);
    Battery_Task(0); assert(!test_adc.CR0);
    Battery_Task(1); assert(test_adc.CR0==1 && !test_adc.SGLSTART);
    Battery_Task(2); assert(test_adc.CR0==0x720D && !test_adc.SGLSTART);
    Battery_Task(3); assert(test_adc.SGLSTART==1);
    test_adc.RESULT=3300; test_adc.IFR=1;
    Battery_Task(4);
    assert(g_battery.valid && g_battery.level==5 && g_battery.raw==3300);
    assert(!test_adc.SGLSTART && !g_battery.errors); tick_unchanged();
    test_adc.IFR=0;
    Battery_Task(253); assert(!test_adc.SGLSTART);
    Battery_Task(254); assert(test_adc.SGLSTART);
    Battery_Task(273); assert(test_adc.SGLSTART);
    Battery_Task(274); assert(!test_adc.SGLSTART && !g_battery.valid && g_battery.errors==1);
    test_gpio.PCIN=1u<<13; /* external present, charge active low */
    Battery_Task(300); Battery_Task(399); assert(!g_battery.external);
    Battery_Task(400); assert(g_battery.external && g_battery.charging);
    test_gpio.PCIN=(1u<<13)|1u;
    Battery_Task(500); Battery_Task(600); assert(g_battery.external && !g_battery.charging);
    test_gpio.PCIN=1;
    Battery_Task(700); Battery_Task(800); assert(!g_battery.external);
    tick_unchanged(); Battery_Stop(); tick_unchanged();
    assert(!gate_on && !test_bgr.CR && !test_adc.CR0 && !test_adc.SGLSTART);
    /* Reboot across uint32 wrap, continuously service the foreground. */
    Battery_Init(0xFFFFFFFEu);
    for(t=0;t<6;++t) Battery_Task(0xFFFFFFFEu+t);
    assert(test_adc.SGLSTART); tick_unchanged();
    puts("PASS real battery driver: settling, conversion, timeout, charge debounce, restart/wrap; SysTick preserved");
    return 0;
}

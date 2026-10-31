/* Test the actual board driver, including activity from all four keys. */
#include <assert.h>
#include <stdio.h>
#include "hc32l17x.h"
#include "gpio.h"
#include "sysctrl.h"
#include "board_io.h"
#include "buttons.h"
TestGpio test_gpio;
TestPca test_pca;
void Gpio_Init(unsigned port,en_gpio_pin_t pin,stc_gpio_cfg_t *cfg)
{ (void)port;(void)pin;(void)cfg; }
void Gpio_SetAfMode(unsigned port,en_gpio_pin_t pin,unsigned af)
{ assert(port==GpioPortC && pin==8 && af<=1); }
void Sysctrl_SetPeripheralGate(unsigned gate,int enable) { (void)gate;(void)enable; }
unsigned Sysctrl_GetPClkFreq(void) { return 4000000; }
static void clear_writes(void) { test_gpio.PBBCLR=test_gpio.PBBSET=0; }
int main(void)
{
    unsigned mode,key,t;
    uint32_t now=0;
    uint16_t events;
    test_gpio.PBIN=0xFFFF;
    Board_Init(now);
    for(mode=LightOn;mode<=LightOff;++mode) {
        Board_SetLightMode((LightMode)mode,now);
        clear_writes();
        for(key=0;key<4;++key) {
            test_gpio.PBIN &= ~(1u << (7-key));
            for(t=0;t<50;++t) {
                events=Board_Task(++now);
                if(events & 15u) Board_WakeLight(now);
                assert(Board_GetLightMode()==(LightMode)mode);
            }
            test_gpio.PBIN=0xFFFF;
            for(t=0;t<50;++t) Board_Task(++now);
        }
        now+=30000; Board_Task(now); Board_WakeLight(now); Board_Task(++now);
        assert(Board_GetLightMode()==(LightMode)mode);
        assert(!test_gpio.PBBCLR && !test_gpio.PBBSET); /* no activity override */
    }
    Board_SetLightMode(LightAuto,now);
    clear_writes(); Board_Task(now+19999); assert(!test_gpio.PBBSET);
    Board_Task(now+20000); assert(test_gpio.PBBSET==(1u<<3));
    clear_writes(); Board_WakeLight(now+20001);
    assert(Board_GetLightMode()==LightAuto && test_gpio.PBBCLR==(1u<<3));
    now+=20001; clear_writes(); Board_Task(now+19999); assert(!test_gpio.PBBSET);
    Board_Task(now+20000); assert(test_gpio.PBBSET==(1u<<3));
    now+=20000;
    Board_SetLightMode(LightOff,now); clear_writes();
    Board_TestLight(now,3000); assert(test_gpio.PBBCLR==(1u<<3));
    assert(Board_GetLightMode()==LightOff);
    clear_writes(); Board_Task(now+2999); assert(!test_gpio.PBBSET);
    Board_Task(now+3000); assert(test_gpio.PBBSET==(1u<<3) && Board_GetLightMode()==LightOff);
    now=0xFFFFFF00u; Board_SetLightMode(LightAuto,now); clear_writes();
    Board_Task(now+19999); assert(!test_gpio.PBBSET);
    Board_Task(now+20000); assert(test_gpio.PBBSET==(1u<<3));
    puts("PASS actual board backlight: all keys preserve ON/OFF, AUTO wake/expiry, temporary self-test, tick wrap");
    return 0;
}

/* Execute the real DeepSleep entry code against simulated core/GPIO state.
 * Tests IRQ races and register transitions; not a current-consumption test. */
#include <assert.h>
#include <stdio.h>
#include "hc32l17x.h"
#include "gpio.h"
#include "power_sleep.h"
#include "buttons.h"
TestGpio test_gpio;
TestSysTick test_tick;
TestScb test_scb;
static uint32_t mask, irq_mode;
static unsigned waits, pending, handled, edge_at_barrier, spurious;
uint32_t __get_PRIMASK(void) { return mask; }
void __disable_irq(void) { mask=1; }
void __set_PRIMASK(uint32_t value)
{
    assert(!(SCB->SCR & (SCB_SCR_SLEEPDEEP_Msk|SCB_SCR_SLEEPONEXIT_Msk)));
    assert(irq_mode==GpioSfIrqActSlpMode);
    mask=value;
    if (!mask && pending) { ++handled; pending=0; }
}
void Gpio_SfIrqModeCfg(unsigned mode) { assert(mask==1); irq_mode=mode; }
void __DSB(void)
{
    if (edge_at_barrier && (SCB->SCR&SCB_SCR_SLEEPDEEP_Msk)) {
        test_gpio.PBIN &= ~(1u<<7); pending=1; edge_at_barrier=0;
    }
}
void __ISB(void) { }
void __WFI(void)
{
    assert(mask==1 && irq_mode==GpioSfIrqDpslpMode);
    assert((SCB->SCR&SCB_SCR_SLEEPDEEP_Msk) && !(SCB->SCR&SCB_SCR_SLEEPONEXIT_Msk));
    assert(!(SCB->SCR&1u)); /* no accidental write to reserved SCR bit 0 */
    assert(!SysTick->CTRL && !SysTick->VAL && SysTick->LOAD==3999);
    ++waits;
    if (spurious) { spurious=0; return; }
    test_gpio.PBIN &= ~(1u<<7); pending=1;
}
static void setup(void)
{
    mask=waits=pending=handled=edge_at_barrier=spurious=0;
    irq_mode=GpioSfIrqActSlpMode;
    test_gpio.PBIN=0xFFFFu;
    test_tick.CTRL=7; test_tick.LOAD=3999; test_tick.VAL=1234;
    test_scb.SCR=SCB_SCR_SLEEPONEXIT_Msk | (1u<<4); test_scb.ICSR=0;
}
int main(void)
{
    PowerHold hold;
    setup(); PowerSleep_Stop();
    assert(waits==1 && handled==1 && !mask && test_scb.SCR==(1u<<4));
    assert(test_scb.ICSR==SCB_ICSR_PENDSTCLR_Msk);
    setup(); test_gpio.PBIN &= ~(1u<<7); pending=1;
    PowerSleep_Stop(); assert(!waits && handled==1 && !mask); /* press before final check */
    setup(); edge_at_barrier=1;
    PowerSleep_Stop(); assert(waits==1 && handled==1 && !edge_at_barrier); /* check/WFI race */
    setup(); mask=1; PowerSleep_Stop(); assert(waits==1 && !handled && pending && mask==1);
    setup(); spurious=1; PowerSleep_Stop();
    assert(waits==1 && !handled && (test_gpio.PBIN&(1u<<7)));
    PowerSleep_Stop(); assert(waits==2 && handled==1); /* non-key wake goes back to sleep */
    /* Repeat short wakes: elapsed time must never accumulate across presses. */
    PowerHold_Init(&hold);
    assert(!PowerHold_Update(&hold,1,0));
    assert(!PowerHold_Update(&hold,1,1499));
    assert(!PowerHold_Update(&hold,0,1499));
    setup(); PowerSleep_Stop();
    assert(!PowerHold_Update(&hold,1,5000));
    assert(!PowerHold_Update(&hold,1,6499));
    assert(PowerHold_Update(&hold,1,6500));
    puts("PASS DeepSleep: tick off, green wake, early press, pending IRQ race, mask/IRQ restore, hold reset");
    return 0;
}

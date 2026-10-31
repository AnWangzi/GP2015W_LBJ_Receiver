#include "hc32l17x.h"
#include "gpio.h"
#include "power_sleep.h"

void PowerSleep_Stop(void)
{
    uint32_t mask=__get_PRIMASK();
    /* RM rev1.80 3.4.2: an enabled pending IRQ wakes WFI with PRIMASK=1,
     * but its handler runs only after unmasking. This closes the check/sleep
     * race without clearing a green-key interrupt that arrived meanwhile. */
    __disable_irq();
    SysTick->CTRL=0;
    SysTick->VAL=0;
    SCB->ICSR=SCB_ICSR_PENDSTCLR_Msk;
    Gpio_SfIrqModeCfg(GpioSfIrqDpslpMode);
    SCB->SCR &= ~SCB_SCR_SLEEPONEXIT_Msk;
    if (M0P_GPIO->PBIN & (1u<<7)) {
        SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
        __DSB(); __ISB();
        __WFI();
    }
    /* The RCH clock used on entry is restored by hardware (RM 2.3).
     * Clear deep/on-exit flags before allowing the PB07 handler or SysTick.
     * Long-press qualification uses ordinary Sleep between 1 ms ticks. */
    SCB->SCR &= ~(SCB_SCR_SLEEPDEEP_Msk|SCB_SCR_SLEEPONEXIT_Msk);
    Gpio_SfIrqModeCfg(GpioSfIrqActSlpMode);
    __DSB(); __ISB();
    __set_PRIMASK(mask);
}

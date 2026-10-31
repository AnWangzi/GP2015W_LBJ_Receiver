/* HC32L176 IAP transport. User continuity measurement, 2026-10-25:
 * rear contacts in the user's left-to-right orientation: 3=PF00/TX,
 * 7=PF01/RX, 9=GND, 4=RESETB. UART electrical tests remain pending. */
#include <string.h>
#include "hc32l17x.h"
#include "gpio.h"
#include "sysctrl.h"
#include "flash.h"
#include "iap_wire.h"
static IapWire wire;
static uint8_t response[32];
static uint32_t milliseconds;
static int flash_ready;
extern void Iap_Jump(void);

static uint32_t tick(void)
{
    if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) ++milliseconds;
    return milliseconds;
}
static void pin(en_gpio_port_t port, en_gpio_pin_t number, int input, int high)
{
    stc_gpio_cfg_t cfg;
    memset(&cfg,0,sizeof(cfg));
    cfg.enDir=input ? GpioDirIn : GpioDirOut;
    cfg.enPu=input ? GpioPuEnable : GpioPuDisable;
    cfg.enCtrlMode=GpioFastIO; cfg.bOutputVal=high ? TRUE : FALSE;
    Gpio_Init(port,number,&cfg);
}
const uint8_t *Iap_FlashAt(uint32_t address) { return (const uint8_t *)address; }
static int writable(uint32_t address, unsigned length)
{
    return address>=IAP_APP_BASE && address<IAP_PENDING_META+IAP_PAGE_SIZE &&
        length<=IAP_PENDING_META+IAP_PAGE_SIZE-address;
}
int Iap_ErasePage(uint32_t address)
{
    if (!flash_ready || address%IAP_PAGE_SIZE || !writable(address,IAP_PAGE_SIZE)) return 0;
    if (Flash_OpModeConfig(FlashSectorEraseMode)!=Ok) return 0;
    if (Flash_SectorErase(address)!=Ok) { Flash_OpModeConfig(FlashReadMode); return 0; }
    return Flash_OpModeConfig(FlashReadMode)==Ok;
}
int Iap_Program(uint32_t address, const void *data, unsigned length)
{
    int ok;
    if (!flash_ready || (address&3u) || (length&3u) || !length || !writable(address,length)) return 0;
    if (Flash_OpModeConfig(FlashWriteMode)!=Ok) return 0;
    ok=Flash_Write32(address,(uint32_t *)data,length/4)==Ok;
    if (Flash_OpModeConfig(FlashReadMode)!=Ok) return 0;
    return ok && !memcmp((const void *)address,data,length);
}
static void launch(void)
{
    __disable_irq();
    SysTick->CTRL=0;
    NVIC->ICER[0]=0xFFFFFFFFu; NVIC->ICPR[0]=0xFFFFFFFFu;
    SCB->ICSR=SCB_ICSR_PENDSTCLR_Msk|SCB_ICSR_PENDSVCLR_Msk;
    M0P_UART1->SCON=0;
    Gpio_SetAfMode(GpioPortF,GpioPin0,GpioAf0);
    Gpio_SetAfMode(GpioPortF,GpioPin1,GpioAf0);
    pin(GpioPortF,GpioPin0,1,1); pin(GpioPortF,GpioPin1,1,1);
    Flash_LockAll();
    *(volatile uint32_t *)(IAP_REQUEST_ADDR+8)=IAP_APP_TAG;
    __DSB(); __ISB(); Iap_Jump();
}
static void uart_start(void)
{
    Sysctrl_SetPeripheralGate(SysctrlPeripheralUart1,TRUE);
    pin(GpioPortF,GpioPin0,0,1); pin(GpioPortF,GpioPin1,1,1);
    Gpio_SetAfMode(GpioPortF,GpioPin0,GpioAf3);
    Gpio_SetAfMode(GpioPortF,GpioPin1,GpioAf3);
    M0P_UART1->SCON=0;
    M0P_UART1->SCNT=13;
    M0P_UART1->ICR=0;
    M0P_UART1->SCON=0x250u; /* Mode1, OVER=8, REN, 8N1. */
}
static int send_reply(unsigned length)
{
    unsigned i;
    uint32_t since;
    M0P_UART1->ICR=~2u;
    for (i=0;i<length;++i) {
        since=tick();
        while (!(M0P_UART1->ISR & 8u)) if ((uint32_t)(tick()-since)>100u) return 0;
        M0P_UART1->SBUF=response[i];
        /* TC is software-cleared (RM20.4.5), not TXE. Wait and clear per
         * byte like the vendor polling driver, including the final ACK byte
         * before RUN jumps away and disables the UART. */
        since=tick();
        while (!(M0P_UART1->ISR & 2u)) if ((uint32_t)(tick()-since)>100u) return 0;
        M0P_UART1->ICR=~2u;
    }
    return 1;
}
int main(void)
{
    volatile uint32_t *request=(volatile uint32_t *)IAP_REQUEST_ADDR;
    uint32_t now, green_since=0;
    int requested, forced, valid, run, green_tracking=0;
    unsigned length;
    uint8_t byte;
    __disable_irq();
    requested=request[0]==IAP_REQUEST_MAGIC && request[1]==~IAP_REQUEST_MAGIC;
    request[0]=request[1]=request[2]=request[3]=0;
    Sysctrl_SetPeripheralGate(SysctrlPeripheralGpio,TRUE);
    pin(GpioPortB,GpioPin7,1,1); pin(GpioPortB,GpioPin4,1,1);
    forced=!(M0P_GPIO->PBIN&(1u<<7)) && !(M0P_GPIO->PBIN&(1u<<4));
    /* Quiet, receive-only hardware state during programming/recovery. */
    pin(GpioPortB,GpioPin3,0,1); pin(GpioPortC,GpioPin8,0,0);
    pin(GpioPortA,GpioPin2,0,1); pin(GpioPortA,GpioPin6,0,1);
    flash_ready=Flash_Init(1,FALSE)==Ok;
    if (flash_ready) {
        /* 1 bit protects four 512-byte sectors: keep first 8 KiB locked.
         * RM rev1.80 section10.8.13. No chip erase or read-protect changes. */
        flash_ready=Flash_LockSet(FlashLock0,0xFFFFFFF0u)==Ok &&
                    Flash_LockSet(FlashLock1,0xFFFFFFFFu)==Ok;
    }
    valid=Iap_Recover();
    if (valid && !requested && !forced) launch();
    SysTick->LOAD=SystemCoreClock/1000u-1; SysTick->VAL=0;
    SysTick->CTRL=SysTick_CTRL_ENABLE_Msk|SysTick_CTRL_CLKSOURCE_Msk;
    uart_start(); IapWire_Init(&wire);
    for (;;) {
        now=tick();
        if (M0P_UART1->ISR & (4u|16u)) {
            M0P_UART1->ICR=~(4u|16u|1u); wire.used=0;
        } else if (M0P_UART1->ISR & 1u) {
            byte=(uint8_t)M0P_UART1->SBUF;
            M0P_UART1->ICR=~1u;
            length=IapWire_Byte(&wire,byte,now,response,&run);
            if (length && send_reply(length) && run) launch();
        }
        if (!(M0P_GPIO->PBIN&(1u<<7)) && (M0P_GPIO->PBIN&(1u<<4)) && !wire.transfer.receiving) {
            if (!green_tracking) { green_since=now; green_tracking=1; }
            if ((uint32_t)(now-green_since)>=1500u && Iap_ActiveValid()) launch();
        } else green_tracking=0;
    }
}

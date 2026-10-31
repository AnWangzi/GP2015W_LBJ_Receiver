#include "iap_entry.h"
#include "hc32l17x.h"
#include "../iap/iap_layout.h"
#ifdef LBJ_IAP_APP
__attribute__((section("IAP_TAG"), used))
const uint32_t g_iap_image_tag[4]={IAP_APP_TAG,IAP_BOARD_ID,IAP_APP_BASE,IAP_IMAGE_MAX};
#endif
int IapEntry_Available(void)
{
#ifdef LBJ_IAP_APP
    return 1;
#else
    return 0;
#endif
}
void IapEntry_Start(void)
{
#ifdef LBJ_IAP_APP
    volatile uint32_t *request=(volatile uint32_t *)IAP_REQUEST_ADDR;
    __disable_irq();
    request[0]=IAP_REQUEST_MAGIC; request[1]=~IAP_REQUEST_MAGIC;
    __DSB(); NVIC_SystemReset();
#endif
}

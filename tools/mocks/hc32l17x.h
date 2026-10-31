#ifndef TEST_HC32L17X_H
#define TEST_HC32L17X_H
#include <stdint.h>
typedef struct { uint32_t PCIN,PBIN,PBBCLR,PBBSET,PCBCLR; } TestGpio;
typedef struct { uint32_t CCON,CNT,CARR,CCAP2,CMOD,CCAPM2,EPWM; } TestPca;
typedef struct { uint32_t CR; } TestBgr;
typedef struct { uint32_t CR0, CR1, ICR, SGLSTART, IFR, RESULT; } TestAdc;
typedef struct { uint32_t CTRL, LOAD, VAL; } TestSysTick;
typedef struct { uint32_t SCR, ICSR; } TestScb;
extern TestGpio test_gpio;
extern TestBgr test_bgr;
extern TestAdc test_adc;
extern TestSysTick test_tick;
extern TestPca test_pca;
extern TestScb test_scb;
#define M0P_GPIO (&test_gpio)
#define M0P_BGR (&test_bgr)
#define M0P_ADC (&test_adc)
#define M0P_PCA (&test_pca)
#define SysTick (&test_tick)
#define SCB (&test_scb)
#define SCB_SCR_SLEEPDEEP_Msk (1u<<2)
#define SCB_SCR_SLEEPONEXIT_Msk (1u<<1)
#define SCB_ICSR_PENDSTCLR_Msk (1u<<25)
uint32_t __get_PRIMASK(void);
void __set_PRIMASK(uint32_t mask);
void __disable_irq(void);
void __DSB(void);
void __ISB(void);
void __WFI(void);
#endif

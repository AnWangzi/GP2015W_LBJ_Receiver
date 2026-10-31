#ifndef TEST_GPIO_H
#define TEST_GPIO_H
#include <string.h>
#define DDL_ZERO_STRUCT(x) memset(&(x),0,sizeof(x))
typedef unsigned en_gpio_pin_t;
typedef unsigned en_gpio_port_t;
enum { GpioPin0=0, GpioPin3=3, GpioPin4=4, GpioPin5=5, GpioPin6=6,
       GpioPin7=7,GpioPin8=8,GpioPin13=13, GpioPortA=0,GpioPortB=64,GpioPortC=128,
       GpioDirOut=0,GpioDirIn=1, GpioPuDisable=0, GpioPuEnable=1,
       GpioPdDisable=0, GpioFastIO=1,GpioOdDisable=0,GpioDrvH=1,GpioAf0=0,GpioAf1=1 };
typedef struct { unsigned enDir,enPu,enPd,enCtrlMode,bOutputVal,enOD,enDrv; } stc_gpio_cfg_t;
void Gpio_Init(unsigned port, en_gpio_pin_t pin, stc_gpio_cfg_t *cfg);
void Gpio_SetAnalogMode(unsigned port, en_gpio_pin_t pin);
void Gpio_SetAfMode(unsigned port,en_gpio_pin_t pin,unsigned af);
enum { GpioSfIrqDpslpMode=1, GpioSfIrqActSlpMode=0 };
void Gpio_SfIrqModeCfg(unsigned mode);
#endif

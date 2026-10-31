#ifndef TEST_SYSCTRL_H
#define TEST_SYSCTRL_H
enum { SysctrlPeripheralAdcBgr=16,SysctrlPeripheralGpio=32,SysctrlPeripheralPca=33, FALSE=0, TRUE=1 };
void Sysctrl_SetPeripheralGate(unsigned gate, int enable);
unsigned Sysctrl_GetPClkFreq(void);
#endif

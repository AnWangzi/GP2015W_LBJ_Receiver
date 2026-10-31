/* Flash simulation: 1->0 writes, 512-byte erases, per-word power failures. */
#include <assert.h>
#include <string.h>
#include "iap_wire.h"
static uint8_t flash[0x20000];
static int remaining=-1;
static unsigned mutations;
static IapWire wire;
static uint8_t response[32];
static int allowed(uint32_t address,unsigned length)
{ return address>=IAP_APP_BASE && address<IAP_PENDING_META+512 && length<=IAP_PENDING_META+512-address; }
const uint8_t *Iap_FlashAt(uint32_t address) { assert(address<sizeof(flash)); return flash+address; }
static int power(void) { if (!remaining) return 0; if (remaining>0) --remaining; ++mutations; return 1; }
int Iap_ErasePage(uint32_t address)
{
    assert(allowed(address,512) && !(address%512));
    if (!power()) return 0;
    memset(flash+address,255,512); return 1;
}
int Iap_Program(uint32_t address,const void *data,unsigned length)
{
    const uint8_t *p=data;
    unsigned i,j;
    assert(allowed(address,length) && !(address%4) && !(length%4));
    for (i=0;i<length;i+=4) {
        if (!power()) return 0;
        for (j=0;j<4;++j) {
            assert((flash[address+i+j]&p[i+j])==p[i+j]);
            flash[address+i+j]=p[i+j];
        }
    }
    return 1;
}
void Sim_Init(void) { memset(flash,255,sizeof(flash)); remaining=-1; mutations=0; IapWire_Init(&wire); }
void Sim_Copy(uint32_t address,const void *data,unsigned length)
{ assert(address+length<=sizeof(flash)); memcpy(flash+address,data,length); }
void Sim_FailAfter(int count) { remaining=count; mutations=0; }
unsigned Sim_Mutations(void) { return mutations; }
unsigned Sim_Byte(uint8_t byte,uint32_t now) { int run; return IapWire_Byte(&wire,byte,now,response,&run); }
const uint8_t *Sim_Response(void) { return response; }
void Sim_WireReset(void) { IapWire_Init(&wire); }

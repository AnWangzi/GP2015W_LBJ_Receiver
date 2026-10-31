#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "lcd_buffer.h"
static uint8_t panel[1024];
static unsigned transferred,calls;
static void sink(unsigned page,unsigned col,const uint8_t *data,unsigned len)
{
    assert(page<8 && col<128 && len && col+len<=128);
    memcpy(panel+page*128+col,data,len); transferred+=len; ++calls;
}
static void drain(void)
{
    unsigned n=0;
    while(LcdBuffer_Dirty()) {
        assert(LcdBuffer_Flush(16,128,sink)<=16);
        assert(++n<=128);
    }
}
int main(void)
{
    unsigned i,p,prev;
    LcdBuffer_Reset();
    assert(!LcdBuffer_Dirty() && !LcdBuffer_Flush(16,128,sink));
    for(p=0;p<8;++p) {
        LcdBuffer_Pos(p,0);
        for(i=0;i<128;++i) LcdBuffer_Write((uint8_t)(i+p+1));
    }
    assert(LcdBuffer_Dirty());
    drain();
    for(p=0;p<8;++p) for(i=0;i<128;++i) assert(panel[p*128+i]==(uint8_t)(i+p+1));
    prev=transferred;
    /* A frame changed in RAM then restored must produce no LCD flicker. */
    LcdBuffer_Pos(0,0); LcdBuffer_Write(0);
    LcdBuffer_Pos(0,0); LcdBuffer_Write(1);
    assert(!LcdBuffer_Dirty() && !LcdBuffer_Flush(16,1024,sink));
    assert(transferred==prev);
    LcdBuffer_Pos(4,57); LcdBuffer_Write(0xAA);
    drain(); assert(transferred==prev+1 && panel[4*128+57]==0xAA);
    prev=transferred;
    /* Reject overlong row streams and invalid cursor positions. */
    LcdBuffer_Pos(7,127); LcdBuffer_Write(0x55); LcdBuffer_Write(0x99);
    LcdBuffer_Pos(-1,0); LcdBuffer_Write(0x99);
    LcdBuffer_Pos(8,0); LcdBuffer_Write(0x99);
    LcdBuffer_Pos(0,128); LcdBuffer_Write(0x99);
    drain(); assert(transferred==prev+1 && panel[1023]==0x55);
    LcdBuffer_Pos(0,0); LcdBuffer_Write(0xFF);
    assert(!LcdBuffer_Flush(0,128,sink));
    assert(!LcdBuffer_Flush(16,0,sink));
    assert(!LcdBuffer_Flush(16,128,0));
    drain(); assert(panel[0]==0xFF);
    assert(calls>0);
    prev=transferred;
    LcdBuffer_Invalidate(); drain();
    assert(transferred==prev+1024 && panel[0]==0xFF);
    puts("PASS LCD diff buffer: transfer budget, complete frames, zero unchanged writes, transient clears hidden, one-byte update, bounds");
    return 0;
}

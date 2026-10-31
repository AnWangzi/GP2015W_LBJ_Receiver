#include "lcd_buffer.h"
#include <string.h>
#define WIDTH 128u
#define SIZE 1024u
static uint8_t desired[SIZE], shown[SIZE];
static uint8_t valid[SIZE/8];
static unsigned cursor, scan;

void LcdBuffer_Reset(void)
{
    memset(desired,0,sizeof(desired));
    memset(shown,0,sizeof(shown));
    memset(valid,0xFF,sizeof(valid));
    cursor=scan=0;
}
void LcdBuffer_Invalidate(void) { memset(valid,0,sizeof(valid)); scan=0; }
static int dirty(unsigned i)
{ return !(valid[i/8] & (1u << (i%8))) || desired[i]!=shown[i]; }
void LcdBuffer_Pos(int page,int col)
{
    cursor = page >= 0 && page < 8 && col >= 0 && col < 128 ?
        (unsigned)page * WIDTH + (unsigned)col : SIZE;
}
void LcdBuffer_Write(uint8_t value)
{
    if (cursor < SIZE) {
        desired[cursor]=value;
        /* A stream past the visible row must not wrap onto another page. */
        cursor = (cursor % WIDTH == WIDTH-1) ? SIZE : cursor+1;
    }
}
void LcdBuffer_InvertRow(int page)
{
    unsigned i;
    if (page < 0 || page > 6) return;
    for (i=(unsigned)page*WIDTH; i<(unsigned)(page+2)*WIDTH; ++i)
        desired[i] ^= 0xFF;
}
unsigned LcdBuffer_Flush(unsigned budget,unsigned scan_budget,LcdSpanWriter writer)
{
    unsigned written=0, examined=0, start, count,i;
    if (!writer) return 0;
    while (written < budget && examined < scan_budget) {
        if (!dirty(scan)) {
            scan=(scan+1) & (SIZE-1); ++examined;
            continue;
        }
        start=scan; count=0;
        do {
            ++scan; ++count; ++examined;
        } while (written+count < budget && examined < scan_budget &&
                 scan < SIZE && (scan % WIDTH) && dirty(scan));
        writer(start/WIDTH,start%WIDTH,&desired[start],count);
        memcpy(&shown[start],&desired[start],count);
        for(i=start;i<start+count;++i) valid[i/8] |= (uint8_t)(1u << (i%8));
        written+=count;
        scan &= SIZE-1;
    }
    return written;
}
int LcdBuffer_Dirty(void)
{
    unsigned i;
    for(i=0;i<SIZE/8;++i) if(valid[i]!=0xFF) return 1;
    return memcmp(desired,shown,SIZE) != 0;
}

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "radio_task.h"
#include "si446x.h"
extern const uint8_t g_radio_cfg[];
extern const uint16_t g_radio_cfg_len;
volatile uint16_t g_radio_fail_offset;
volatile uint8_t g_radio_cmd_error,g_radio_cmd_id;
static uint8_t command[16];
static unsigned cmd_len,response_len,cfg_offset,cfg_count,cal_count,status_count,polls,mode,tunes;
static uint32_t sent_at,origin,low_at;
static int is_cfg,sdn;
void SI446X_InitPins(void) { sdn=1; }
void SI446X_SetShutdown(int high) { sdn=high; }
void SI446X_AsyncBegin(const uint8_t *cmd,unsigned len,unsigned n,uint32_t now)
{
    assert(!sdn && (uint32_t)(now-origin)>=120u && len>0 && len<=16 && n<=16);
    memcpy(command,cmd,len); cmd_len=len; response_len=n; sent_at=now; polls=0;
    is_cfg=0;
    if(cfg_offset<g_radio_cfg_len && len==g_radio_cfg[cfg_offset] &&
       !memcmp(cmd,g_radio_cfg+cfg_offset+1,len)) {
        is_cfg=1; ++cfg_count; if(cmd[0]==0x17) ++cal_count;
        cfg_offset+=len+1;
    }
    if(cmd[0]==0x23) ++status_count;
    if(cmd[0]==0x33) ++tunes;
}
int SI446X_AsyncPoll(uint8_t *reply,uint32_t now)
{
    if(mode==1) return (uint32_t)(now-sent_at)>=300 ? -1 : 0;
    if(++polls<3) return 0;
    memset(reply,0,response_len);
    if(is_cfg) assert(response_len==0);
    else if(command[0]==1) { assert(response_len==8); reply[1]=0x44; reply[2]=0x63; }
    else if(command[0]==0x12) {
        if(command[1]==0x40) memcpy(reply,"\x3E\x09\x60\x7E",4);
        else if(command[3]==0x51) reply[0]=mode==2 ? 0x0A : 8;
        else memcpy(reply,"\x01\x53\x01\x83\x2B",5);
    } else if(command[0]==0x33) reply[0]=tunes>=3 ? 8 : 6;
    else if(command[0]==0x22) { assert(response_len==8); reply[2]=54; }
    else if(command[0]==0x23 && mode==3 && cfg_count) reply[0]=8;
    else assert(command[0]==0x23 || command[0]==0x02 || command[0]==0x32);
    assert(cmd_len>0);
    return 1;
}
static void run(unsigned test_mode,uint32_t start)
{
    unsigned i;
    mode=test_mode; origin=start; low_at=0;
    cfg_offset=cfg_count=cal_count=status_count=tunes=0;
    Radio_Begin(start);
    for(i=0;i<3000;++i) {
        Radio_Task(start+i);
        if(!sdn && !low_at) low_at=i;
        Radio_Task(start+i); /* repeated tick must not issue extra work */
    }
    assert(low_at==100);
}
int main(void)
{
    run(0,0);
    assert(g_radio.ready && g_radio.state==8 && !g_radio.error);
    assert(cfg_offset==g_radio_cfg_len && cfg_count>20 && cal_count==2);
    assert(status_count==cfg_count+1);
    assert(g_radio.rssi==-107 && g_radio.peak==-107 && !g_radio.timeouts);
    run(0,0xFFFFFF80u);
    assert(g_radio.ready && !g_radio.error && cfg_offset==g_radio_cfg_len);
    run(1,0); assert(!g_radio.ready && g_radio.error && g_radio.timeouts==1 && !cfg_count);
    run(2,0); assert(!g_radio.ready && !strcmp(g_radio.error,"BAND CHECK"));
    run(3,0); assert(!g_radio.ready && !strcmp(g_radio.error,"CONFIG CMD ERROR") && cfg_count==1);
    puts("PASS async radio: original config stream/order, two IRCAL, property checks, tune wait, RSSI, CTS stalls, errors, tick wrap");
    return 0;
}

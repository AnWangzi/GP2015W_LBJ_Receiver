/* One bounded SPI transfer/poll at a time; no CTS or reset delay loops. */
#include "radio_task.h"
#include "si446x.h"
#include <string.h>
extern const uint8_t g_radio_cfg[];
extern const uint16_t g_radio_cfg_len;
enum { HoldHigh,HoldLow,Power,Part,Clear,Config,Status,Freq,Band,Bcr,Start,Tune,
       Running,RunRssi,RunState,Failed };
RadioReading g_radio;
static unsigned phase,pending,offset;
static uint32_t since,last_tick,last_rssi,last_state;
static uint8_t reply[16];
static const char *const errors[] = {"RESET HIGH","RESET LOW","POWER_UP CTS","PART_INFO",
    "CLEAR STATUS","CONFIG / IRCAL","CHIP STATUS","FREQ READ","BAND READ","BCR READ",
    "START_RX CTS","RX STATE READ"};
static const uint8_t pu[]={0x02,0x01,0x01,0x01,0x8C,0xBA,0x80};
static const uint8_t part[]={0x01},status[]={0x23,0x00};
static const uint8_t freq[]={0x12,0x40,4,0},band[]={0x12,0x20,1,0x51},bcr[]={0x12,0x20,5,0x22};
static const uint8_t rx[]={0x32,0,0,0,0,0,0,0},state[]={0x33},rssi[]={0x22,0xFF};

static void fail(const char *why) { g_radio.error=why; phase=Failed; pending=0; }
static void begin(const uint8_t *cmd,unsigned len,unsigned n,uint32_t now)
{ SI446X_AsyncBegin(cmd,len,n,now); pending=1; }
void Radio_Begin(uint32_t now)
{
    memset(&g_radio,0,sizeof(g_radio)); g_radio.rssi=g_radio.peak=-134;
    SI446X_InitPins(); SI446X_SetShutdown(1);
    phase=HoldHigh; pending=offset=0; since=now; last_tick=now-1u;
    last_rssi=last_state=now;
    g_radio_fail_offset=0xFFFF; g_radio_cmd_error=g_radio_cmd_id=0;
}
void Radio_Task(uint32_t now)
{
    int result;
    if(now==last_tick || phase==Failed) return;
    last_tick=now;
    if(phase==HoldHigh) {
        if((uint32_t)(now-since)>=100u) { SI446X_SetShutdown(0); since=now; phase=HoldLow; }
        return;
    }
    if(phase==HoldLow) {
        if((uint32_t)(now-since)<20u) return;
        phase=Power;
    }
    if(pending) {
        result=SI446X_AsyncPoll(reply,now);
        if(!result) return;
        pending=0;
        if(result<0) {
            ++g_radio.timeouts;
            if(phase>=RunRssi) { g_radio.state=0xFF; phase=Running; return; }
            fail(errors[phase]); return;
        }
        switch(phase) {
        case Power: phase=Part; break;
        case Part:
            if(reply[1]!=0x44 || reply[2]!=0x63) { fail("PART NOT 4463"); return; }
            phase=Clear; break;
        case Clear: phase=Config; break;
        case Config: phase=Status; break;
        case Status:
            if((reply[0]|reply[1]) & 0x08) {
                g_radio_cmd_error=reply[2]; g_radio_cmd_id=reply[3];
                fail("CONFIG CMD ERROR"); return;
            }
            offset+=1u+g_radio_cfg[offset];
            phase=offset<g_radio_cfg_len ? Config : Freq;
            if(phase==Freq) g_radio_fail_offset=0xFFFF;
            break;
        case Freq:
            if(memcmp(reply,"\x3E\x09\x60\x7E",4)) { fail("FREQ CHECK"); return; }
            phase=Band; break;
        case Band:
            if(reply[0]!=8) { fail("BAND CHECK"); return; }
            phase=Bcr; break;
        case Bcr:
            if(memcmp(reply,"\x01\x53\x01\x83\x2B",5)) { fail("BCR CHECK"); return; }
            phase=Start; break;
        case Start: phase=Tune; since=now; break;
        case Tune:
            g_radio.state=reply[0];
            if(reply[0]==8) { g_radio.ready=1; phase=Running; last_rssi=last_state=now; }
            else if((uint32_t)(now-since)>=100u) { fail("STATE NOT RX08"); return; }
            break;
        case RunRssi:
            g_radio.rssi=(int)reply[2]/2-134;
            if(g_radio.rssi>g_radio.peak) g_radio.peak=g_radio.rssi;
            phase=Running; break;
        case RunState: g_radio.state=reply[0]; phase=Running; break;
        default: break;
        }
    }
    if(phase==Running) {
        if((uint32_t)(now-last_state)>=1000u) { last_state=now; phase=RunState; }
        else if((uint32_t)(now-last_rssi)>=200u) { last_rssi=now; phase=RunRssi; }
        else return;
    }
    switch(phase) {
    case Power: begin(pu,sizeof(pu),0,now); break;
    case Part: begin(part,sizeof(part),8,now); break;
    case Clear: case Status: begin(status,sizeof(status),4,now); break;
    case Config:
        g_radio_fail_offset=(uint16_t)offset;
        if(offset>=g_radio_cfg_len || !g_radio_cfg[offset] || g_radio_cfg[offset]>16 ||
           offset+1u+g_radio_cfg[offset]>g_radio_cfg_len) { fail("CONFIG LENGTH"); return; }
        begin(g_radio_cfg+offset+1,g_radio_cfg[offset],0,now); break;
    case Freq: begin(freq,sizeof(freq),4,now); break;
    case Band: begin(band,sizeof(band),1,now); break;
    case Bcr: begin(bcr,sizeof(bcr),5,now); break;
    case Start: begin(rx,sizeof(rx),0,now); break;
    case Tune: case RunState: begin(state,sizeof(state),2,now); break;
    case RunRssi: begin(rssi,sizeof(rssi),8,now); break;
    default: break;
    }
}

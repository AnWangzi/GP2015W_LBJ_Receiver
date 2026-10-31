#include <string.h>
#include <stdio.h>
#include "history.h"

/* 20 bytes per train instead of storing 100 full decoded messages in 16 KiB.
 * Number and registry are lossless decimal fields; validity uses sentinels. */
typedef struct {
    uint32_t identity, registry, position, id;
    uint16_t speed;
    char prefix[2];
} HistoryRecord;
typedef char HistoryRecordMustBe20Bytes[sizeof(HistoryRecord)==20 ? 1 : -1];
static HistoryRecord records[HISTORY_CAPACITY];
static unsigned count;
static uint32_t next_id;

static int decimal(const char *text,unsigned maximum,uint32_t *value,unsigned *digits)
{
    unsigned n=0;
    uint32_t v=0;
    while (text[n]) {
        if (n>=maximum || text[n]<'0' || text[n]>'9') return 0;
        v=v*10u+(unsigned)(text[n]-'0'); ++n;
    }
    if (!n) return 0;
    *value=v; *digits=n; return 1;
}
void History_Init(void) { count=0; next_id=1; }
unsigned History_Count(void) { return count; }
uint32_t History_Id(unsigned index) { return index<count ? records[index].id : 0; }
unsigned History_FindId(uint32_t id)
{
    unsigned i;
    for (i=0;i<count;++i) if (records[i].id==id) return i;
    return count ? count-1u : 0;
}
int History_Upsert(const LbjMsg *message)
{
    HistoryRecord incoming;
    uint32_t value, code, number;
    unsigned digits, i, found, compatible;
    if (!message || !decimal(message->trainNumber,6,&value,&digits)) return 0;
    memset(&incoming,0,sizeof(incoming));
    incoming.identity=value | (digits<<20);
    incoming.position=0xFFFFFFFFu; incoming.speed=0xFFFFu;
    incoming.prefix[0]=message->prefix[0]; incoming.prefix[1]=message->prefix[1];
    if (decimal(message->speed,3,&value,&digits)) incoming.speed=(uint16_t)value;
    if (strlen(message->position)==6 && message->position[4]=='.') {
        char position[6];
        memcpy(position,message->position,4); position[4]=message->position[5]; position[5]=0;
        if (decimal(position,5,&value,&digits)) incoming.position=value;
    }
    if (message->locoModel[0] && decimal(message->locoCode,3,&code,&digits) && digits==3 &&
        decimal(message->locoNumber,5,&number,&digits) && digits==5)
        incoming.registry=0x80000000u | (code*100000u+number);
    found=compatible=count;
    for (i=0;i<count;++i) if (
        (records[i].identity&0xFFFFFu)==(incoming.identity&0xFFFFFu) &&
        (records[i].identity==incoming.identity || records[i].prefix[0] || incoming.prefix[0])) {
        if (!memcmp(records[i].prefix,incoming.prefix,2)) { found=i; break; }
        if (compatible==count && (!records[i].prefix[0] || !incoming.prefix[0])) compatible=i;
    }
    if (found==count) found=compatible;
    if (found<count) {
        incoming.id=records[found].id;
        /* Preserve details only for basic updates of a compatible train.
         * An accepted extension is a complete new detail snapshot, including
         * empty fields; never resurrect its missing locomotive from history. */
        if (!message->isDetailed &&
            (!incoming.prefix[0] || !records[found].prefix[0] ||
             !memcmp(incoming.prefix,records[found].prefix,2))) {
            if (!incoming.registry) incoming.registry=records[found].registry;
            if (!incoming.prefix[0]) memcpy(incoming.prefix,records[found].prefix,2);
        }
        if (incoming.speed==0xFFFFu) incoming.speed=records[found].speed;
        if (incoming.position==0xFFFFFFFFu) incoming.position=records[found].position;
    } else {
        incoming.id=next_id++;
        if (!next_id) next_id=1;
        if (count<HISTORY_CAPACITY) ++count;
        found=count-1u;
    }
    if (found) memmove(records+1,records,found*sizeof(records[0]));
    records[0]=incoming;
    return 1;
}
int History_Get(unsigned index,LbjMsg *message)
{
    const HistoryRecord *r;
    char registry[9];
    const char *number;
    unsigned digits;
    uint32_t value;
    if (index>=count || !message) return 0;
    r=&records[index]; memset(message,0,sizeof(*message));
    digits=(unsigned)(r->identity>>20); value=r->identity&0xFFFFFu;
    if (!digits || digits>6 || value>999999u) return 0;
    snprintf(message->trainNumber,sizeof(message->trainNumber),"%0*lu",
             (int)digits,(unsigned long)value);
    memcpy(message->prefix,r->prefix,2);
    number=message->trainNumber;
    if (message->prefix[0]) while (number[0]=='0' && number[1]) ++number;
    snprintf(message->train,sizeof(message->train),"%s%s",message->prefix,number);
    if (r->speed!=0xFFFFu) snprintf(message->speed,sizeof(message->speed),"%03u",(unsigned)r->speed);
    if (r->position!=0xFFFFFFFFu)
        snprintf(message->position,sizeof(message->position),"%04lu.%lu",
                 (unsigned long)(r->position/10u),(unsigned long)(r->position%10u));
    if (r->registry & 0x80000000u) {
        value=r->registry&0x7FFFFFFFu;
        if (value>99999999u) return 0;
        snprintf(registry,sizeof(registry),"%08lu",(unsigned long)value);
        Lbj_SetRegistry(registry,message);
    }
    message->isShort=1;
    message->isDetailed=message->prefix[0] || message->locoModel[0];
    return 1;
}

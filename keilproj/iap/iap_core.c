#include <string.h>
#include "iap_layout.h"
static uint32_t page[IAP_PAGE_SIZE / 4];
static uint32_t get32(const uint8_t *p)
{ return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static void put32(uint8_t *p, uint32_t v)
{ unsigned i; for (i=0;i<4;++i) p[i]=(uint8_t)(v>>(8*i)); }
uint32_t Iap_Crc32(const void *data, unsigned length)
{
    const uint8_t *p=(const uint8_t *)data;
    uint32_t crc=0xFFFFFFFFu;
    unsigned i;
    while (length--) {
        crc ^= *p++;
        for (i=0;i<8;++i) crc=(crc>>1) ^ (0xEDB88320u & (0u-(crc&1u)));
    }
    return ~crc;
}
static int image_valid(uint32_t base, uint32_t length, uint32_t crc)
{
    const uint8_t *image=Iap_FlashAt(base);
    uint32_t stack, reset;
    if (length < 210u || length > IAP_IMAGE_MAX) return 0;
    stack=get32(image); reset=get32(image+4);
    if ((stack&7u) || stack<=0x20000000u || stack>IAP_REQUEST_ADDR) return 0;
    if (!(reset&1u) || (reset&~1u)<IAP_APP_BASE+208u ||
        (reset&~1u)>=IAP_APP_BASE+length) return 0;
    if (get32(image+192)!=IAP_APP_TAG || get32(image+196)!=IAP_BOARD_ID ||
        get32(image+200)!=IAP_APP_BASE || get32(image+204)!=IAP_IMAGE_MAX) return 0;
    return Iap_Crc32(image,length)==crc;
}
static int read_record(uint32_t address, IapRecord *record, uint32_t image_base)
{
    memcpy(record,Iap_FlashAt(address),sizeof(*record));
    return record->magic==IAP_META_MAGIC &&
        record->record_crc==Iap_Crc32(record,12) &&
        image_valid(image_base,record->length,record->crc);
}
static int save_record(uint32_t address, uint32_t length, uint32_t crc)
{
    IapRecord record;
    record.magic=IAP_META_MAGIC; record.length=length; record.crc=crc;
    record.record_crc=Iap_Crc32(&record,12);
    return Iap_ErasePage(address) && Iap_Program(address,&record,sizeof(record));
}
void Iap_Init(IapTransfer *transfer) { memset(transfer,0,sizeof(*transfer)); }
int Iap_ActiveValid(void)
{ IapRecord record; return read_record(IAP_ACTIVE_META,&record,IAP_APP_BASE); }
int Iap_Recover(void)
{
    IapRecord pending, active;
    unsigned offset, count;
    if (!read_record(IAP_PENDING_META,&pending,IAP_STAGE_BASE)) return Iap_ActiveValid();
    if (read_record(IAP_ACTIVE_META,&active,IAP_APP_BASE) &&
        active.length==pending.length && active.crc==pending.crc) return 1;
    /* Staging and its committed descriptor stay intact throughout copying.
     * After loss of power, startup retries this installation from staging. */
    for (offset=0;offset<pending.length;offset+=IAP_PAGE_SIZE) {
        count=pending.length-offset;
        if (count>IAP_PAGE_SIZE) count=IAP_PAGE_SIZE;
        memset(page,0xFF,sizeof(page));
        memcpy(page,Iap_FlashAt(IAP_STAGE_BASE+offset),count);
        if (!Iap_ErasePage(IAP_APP_BASE+offset) ||
            !Iap_Program(IAP_APP_BASE+offset,page,IAP_PAGE_SIZE)) return 0;
    }
    if (!image_valid(IAP_APP_BASE,pending.length,pending.crc)) return 0;
    return save_record(IAP_ACTIVE_META,pending.length,pending.crc) && Iap_ActiveValid();
}
unsigned Iap_Command(IapTransfer *t, uint8_t command,
                     const uint8_t *data, unsigned length, uint8_t *reply)
{
    uint32_t size, crc, offset;
    unsigned count, rounded;
    reply[0]=IapOk;
    switch (command) {
    case IapPing:
        if (length) { reply[0]=IapBadLength; break; }
        put32(reply+1,IAP_BOARD_ID); put32(reply+5,IAP_APP_BASE);
        put32(reply+9,IAP_IMAGE_MAX); put32(reply+13,IAP_BAUD);
        return 17;
    case IapBegin:
        if (length!=16) { reply[0]=IapBadLength; break; }
        size=get32(data); crc=get32(data+4);
        if (size<210 || size>IAP_IMAGE_MAX || get32(data+8)!=IAP_APP_BASE ||
            get32(data+12)!=IAP_BOARD_ID) { reply[0]=IapBadImage; break; }
        /* BEGIN retries after a lost reply must not discard received data. */
        if (t->receiving && t->length==size && t->crc==crc) break;
        Iap_Init(t);
        if (!Iap_ErasePage(IAP_PENDING_META)) { reply[0]=IapFlashError; break; }
        t->length=size; t->crc=crc; t->receiving=1;
        break;
    case IapData:
        if (!t->receiving) { reply[0]=IapBadState; break; }
        if (length<=4 || length>4+IAP_PAGE_SIZE) { reply[0]=IapBadLength; break; }
        offset=get32(data); count=length-4;
        if (offset>t->length || count>t->length-offset || (offset%IAP_PAGE_SIZE) ||
            (count!=IAP_PAGE_SIZE && offset+count!=t->length)) { reply[0]=IapBadOffset; break; }
        if (offset<t->received) {
            if (offset+count>t->received || memcmp(Iap_FlashAt(IAP_STAGE_BASE+offset),data+4,count))
                reply[0]=IapBadOffset;
            break;
        }
        if (offset!=t->received) { reply[0]=IapBadOffset; break; }
        memset(page,0xFF,sizeof(page)); memcpy(page,data+4,count);
        rounded=(count+3u)&~3u;
        if (!Iap_ErasePage(IAP_STAGE_BASE+offset) ||
            !Iap_Program(IAP_STAGE_BASE+offset,page,rounded)) { reply[0]=IapFlashError; break; }
        t->received+=count;
        break;
    case IapCommit:
        if (length) { reply[0]=IapBadLength; break; }
        if (!t->receiving || t->received!=t->length) { reply[0]=IapBadState; break; }
        if (!image_valid(IAP_STAGE_BASE,t->length,t->crc)) { reply[0]=IapBadImage; break; }
        if (!save_record(IAP_PENDING_META,t->length,t->crc)) { reply[0]=IapFlashError; break; }
        if (!Iap_Recover()) { reply[0]=IapFlashError; break; }
        t->receiving=0;
        break;
    case IapRun:
        if (length) reply[0]=IapBadLength;
        else if (!Iap_ActiveValid()) reply[0]=IapBadImage;
        break;
    case IapAbort:
        if (length) reply[0]=IapBadLength;
        else Iap_Init(t);
        break;
    default: reply[0]=IapBadCommand;
    }
    return 1;
}

#include <string.h>
#include "iap_wire.h"
static const uint8_t magic[]={'L','B','J','1'};
void IapWire_Init(IapWire *wire) { memset(wire,0,sizeof(*wire)); }
unsigned IapWire_Byte(IapWire *w, uint8_t byte, uint32_t now,
                      uint8_t *reply, int *run)
{
    uint32_t crc, received_crc;
    uint16_t sequence, length;
    unsigned n, i;
    uint8_t command;
    *run=0;
    if (w->used && (uint32_t)(now-w->last_byte)>500u) w->used=0;
    w->last_byte=now;
    if (w->used<4 && byte!=magic[w->used]) { w->used=byte==magic[0] ? 1 : 0; return 0; }
    w->buffer[w->used++]=byte;
    if (w->used==9) {
        length=(uint16_t)(w->buffer[7]|w->buffer[8]<<8);
        if (length>IAP_BODY_MAX) { w->used=0; return 0; }
        w->expected=(uint16_t)(13+length);
    }
    if (w->used<9 || w->used!=w->expected) return 0;
    length=(uint16_t)(w->expected-13);
    crc=Iap_Crc32(w->buffer+4,5+length);
    received_crc=0;
    for (i=0;i<4;++i) received_crc|=(uint32_t)w->buffer[9+length+i]<<(8*i);
    w->used=0;
    if (crc!=received_crc) return 0;
    command=w->buffer[4]; sequence=(uint16_t)(w->buffer[5]|w->buffer[6]<<8);
    if (w->cached && w->cached_crc==crc && w->cached_sequence==sequence && w->cached_command==command) {
        n=w->cached_length;
    } else {
        n=Iap_Command(&w->transfer,command,w->buffer+9,length,w->cached_reply);
        w->cached_crc=crc; w->cached_sequence=sequence; w->cached_command=command;
        w->cached_length=(uint8_t)n; w->cached=1;
    }
    memcpy(reply,magic,4); reply[4]=command|0x80;
    reply[5]=(uint8_t)sequence; reply[6]=(uint8_t)(sequence>>8);
    reply[7]=(uint8_t)n; reply[8]=0;
    memcpy(reply+9,w->cached_reply,n);
    crc=Iap_Crc32(reply+4,5+n);
    for (i=0;i<4;++i) reply[9+n+i]=(uint8_t)(crc>>(8*i));
    *run=command==IapRun && w->cached_reply[0]==IapOk;
    return 13+n;
}

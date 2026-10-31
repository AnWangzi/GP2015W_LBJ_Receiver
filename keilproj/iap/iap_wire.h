#ifndef IAP_WIRE_H
#define IAP_WIRE_H
#include "iap_layout.h"
#define IAP_BODY_MAX (IAP_PAGE_SIZE+4u)
typedef struct {
    uint8_t buffer[4+5+IAP_BODY_MAX+4];
    uint16_t used, expected;
    uint32_t last_byte;
    uint32_t cached_crc;
    uint16_t cached_sequence;
    uint8_t cached_command, cached_length, cached_reply[17], cached;
    IapTransfer transfer;
} IapWire;
void IapWire_Init(IapWire *wire);
/* Returns encoded reply length, or zero until a complete valid request.
 * run is set only after a successful RUN response has been constructed. */
unsigned IapWire_Byte(IapWire *wire, uint8_t byte, uint32_t now,
                      uint8_t *reply, int *run);
#endif

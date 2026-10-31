#ifndef IAP_LAYOUT_H
#define IAP_LAYOUT_H
#include <stdint.h>
#define IAP_APP_BASE       0x00002000u
#define IAP_IMAGE_MAX      0x0000E000u
#define IAP_STAGE_BASE     0x00010000u
#define IAP_ACTIVE_META    0x0001E000u
#define IAP_PENDING_META   0x0001E200u
#define IAP_PAGE_SIZE      512u
#define IAP_REQUEST_ADDR   0x20003FF0u
#define IAP_REQUEST_MAGIC  0x4C424A55u
#define IAP_META_MAGIC     0x314A424Cu
#define IAP_BOARD_ID       0x36373148u /* H176, little endian */
#define IAP_BAUD           38400u /* 4 MHz / 8 / 13 = 38461.5, +0.16%. */
#define IAP_APP_TAG        0x31504149u /* IAP1 */

enum { IapPing, IapBegin, IapData, IapCommit, IapRun, IapAbort };
enum { IapOk, IapBadCommand, IapBadLength, IapBadImage, IapBadOffset,
       IapFlashError, IapBadState };
typedef struct { uint32_t magic, length, crc, record_crc; } IapRecord;
typedef struct { uint32_t length, crc, received; uint8_t receiving; } IapTransfer;

uint32_t Iap_Crc32(const void *data, unsigned length);
void Iap_Init(IapTransfer *transfer);
int Iap_Recover(void);
int Iap_ActiveValid(void);
/* Payload fields are little endian, DATA = offset + up to 512 bytes.
 * Response payload starts with a status byte. */
unsigned Iap_Command(IapTransfer *transfer, uint8_t command,
                     const uint8_t *payload, unsigned length, uint8_t *response);

/* Platform boundary. Writes must reject the boot region and reserved flash. */
const uint8_t *Iap_FlashAt(uint32_t address);
int Iap_ErasePage(uint32_t address);
int Iap_Program(uint32_t address, const void *data, unsigned length);
#endif

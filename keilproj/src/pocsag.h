#ifndef POCSAG_H
#define POCSAG_H
#include <stdint.h>

#define POCSAG_MAX_BCD 160
typedef void (*PocsagMessageFn)(uint32_t address, unsigned function,
                               const char *bcd, unsigned length);
typedef struct {
    uint32_t bits, syncs, words, corrected, bad, messages;
    uint32_t discarded, discard_address;
    uint16_t discard_length;
    uint8_t discard_reason; /* 1=bad word, 2=sync/sample loss, 3=overflow. */
    uint8_t locked, inverted;
} PocsagStats;
extern PocsagStats g_pocsag_stats;
void POCSAG_Init(PocsagMessageFn callback);
/* Discard partial report after sample loss; cumulative counters survive. */
void POCSAG_Reset(void);
void POCSAG_Bit(unsigned bit);
/* Returns number of corrected bits (0..2), or -1. */
int POCSAG_Correct(uint32_t *word);
#endif

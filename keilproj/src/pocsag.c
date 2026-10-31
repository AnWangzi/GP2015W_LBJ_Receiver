/* POCSAG numeric receiver: MSB-first words, both RF polarities,
 * BCH(31,21), even parity and up to two erroneous bits per 32-bit word.
 * Numeric nibbles are sent least-significant bit first.
 */
#include "pocsag.h"
#include <string.h>

#define SYNC 0x7CD215D8u
#define IDLE 0x7A89C197u
PocsagStats g_pocsag_stats;
static uint32_t errors[1024];
static uint32_t shift, address;
static unsigned bit_count, word_index, length, function;
static uint8_t active;
static char message[POCSAG_MAX_BCD + 1];
static PocsagMessageFn deliver;

static unsigned ones(uint32_t x)
{
    x -= (x >> 1) & 0x55555555u;
    x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
    x = (x + (x >> 4)) & 0x0F0F0F0Fu;
    x += x >> 8;
    x += x >> 16;
    return x & 63u;
}

static unsigned syndrome(uint32_t word)
{
    uint32_t v = word >> 1;
    int i;
    for (i = 30; i >= 10; --i)
        if (v & (1u << i)) v ^= 0x769u << (i - 10);
    return (unsigned)v;
}

int POCSAG_Correct(uint32_t *word)
{
    uint32_t mask = errors[syndrome(*word)];
    uint32_t fixed = *word ^ mask;
    unsigned n = ones(mask);
    if (syndrome(fixed)) return -1;
    if (ones(fixed) & 1u) { fixed ^= 1u; ++n; }
    if (n > 2) return -1;
    *word = fixed;
    return (int)n;
}

static void discard(unsigned reason)
{
    if (active && length) {
        ++g_pocsag_stats.discarded;
        g_pocsag_stats.discard_address = address;
        g_pocsag_stats.discard_length = (uint16_t)length;
        g_pocsag_stats.discard_reason = (uint8_t)reason;
    }
    active = 0;
    length = 0;
}

void POCSAG_Reset(void)
{
    discard(2);
    shift = 0;
    bit_count = word_index = length = 0;
    active = 0;
    g_pocsag_stats.locked = g_pocsag_stats.inverted = 0;
}

void POCSAG_Init(PocsagMessageFn callback)
{
    unsigned i, j;
    uint32_t mask;
    memset(errors, 0, sizeof(errors));
    for (i = 1; i < 32; ++i) {
        mask = 1u << i;
        errors[syndrome(mask)] = mask;
        for (j = i + 1; j < 32; ++j) {
            mask = (1u << i) | (1u << j);
            errors[syndrome(mask)] = mask;
        }
    }
    memset(&g_pocsag_stats, 0, sizeof(g_pocsag_stats));
    active = 0; length = 0;
    deliver = callback;
    POCSAG_Reset();
}

static void finish(void)
{
    if (active && length) {
        message[length] = 0;
        ++g_pocsag_stats.messages;
        if (deliver) deliver(address, function, message, length);
    }
    active = 0;
    length = 0;
}

void POCSAG_Bit(unsigned bit)
{
    uint32_t word;
    unsigned i, nibble;
    int corrected;
    static const char digits[] = "0123456789*U -)(";
    ++g_pocsag_stats.bits;
    shift = (shift << 1) | (bit & 1u);
    if (!g_pocsag_stats.locked) {
        if (ones(shift ^ SYNC) <= 2) g_pocsag_stats.inverted = 0;
        else if (ones(~shift ^ SYNC) <= 2) g_pocsag_stats.inverted = 1;
        else return;
        g_pocsag_stats.locked = 1;
        ++g_pocsag_stats.syncs;
        word_index = bit_count = 0;
        return;
    }
    if (++bit_count != 32) return;
    bit_count = 0;
    word = g_pocsag_stats.inverted ? ~shift : shift;
    if (word_index == 16) {
        if (ones(word ^ SYNC) > 2) { POCSAG_Reset(); return; }
        ++g_pocsag_stats.syncs;
        word_index = 0;
        return;
    }
    ++g_pocsag_stats.words;
    corrected = POCSAG_Correct(&word);
    if (corrected < 0) {
        ++g_pocsag_stats.bad;
        /* A corrupt address could change the meaning of subsequent data. */
        discard(1);
    } else {
        g_pocsag_stats.corrected += (unsigned)corrected;
        if (word == IDLE) finish();
        else if (!(word & 0x80000000u)) {
            finish();
            address = ((word >> 13) << 3) | (word_index / 2);
            function = (word >> 11) & 3;
            active = 1;
        } else if (active) {
            if (length + 5 > POCSAG_MAX_BCD) discard(3);
            else for (i = 0; i < 5; ++i) {
                nibble = (word >> (27 - 4 * i)) & 15u;
                nibble = ((nibble & 1) << 3) | ((nibble & 2) << 1) |
                         ((nibble & 4) >> 1) | ((nibble & 8) >> 3);
                message[length++] = digits[nibble];
            }
        }
    }
    ++word_index;
}

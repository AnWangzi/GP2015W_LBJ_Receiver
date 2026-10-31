#ifndef LBJ_STATE_H
#define LBJ_STATE_H
#include <stdint.h>
#include "lbj_parse.h"

#define LBJ_SESSIONS 4
#define LBJ_PAIR_MS 8000u
#define LBJ_CACHE_MS 30000u
typedef struct {
    LbjMsg message;
    uint32_t short_at, detail_at;
    unsigned function;
    uint8_t used, has_detail;
} LbjSession;
typedef struct {
    char number[8];
    uint32_t short_at, conflict_at;
    uint8_t used, conflicted;
} LbjPairTrack;

typedef struct {
    LbjSession sessions[LBJ_SESSIONS];
    LbjPairTrack pair[2]; /* Separate direction streams; not POCSAG addresses. */
    uint32_t ambiguous; /* Anonymous extensions skipped after train switches. */
    uint8_t current, have_current;
} LbjState;

void LbjState_Init(LbjState *state);
/* NULL: no safely associated train to display; the parsed report still counts.
 * Standalone extensions have no train identity: pair only with the latest
 * short report, same direction, <=8 seconds. A recent change between different
 * numbers in that direction blocks anonymous extensions for 8 seconds after
 * the last change. Merged reports carry their own number and bypass this gate.
 * This reduces ambiguity; a wholly missed short report is still undetectable. */
const LbjMsg *LbjState_Update(LbjState *state, const LbjMsg *message,
                              unsigned function, uint32_t now);
#endif

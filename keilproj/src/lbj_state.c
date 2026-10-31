#include <string.h>
#include <stdio.h>
#include "lbj_state.h"

void LbjState_Init(LbjState *state) { memset(state, 0, sizeof(*state)); }

static void details(LbjMsg *to, const LbjMsg *from)
{
    /* One extension is one snapshot. Missing fields must not inherit parts of
     * another extension (for example a new prefix with an old locomotive). */
    strcpy(to->prefix, from->prefix);
    strcpy(to->loco, from->loco);
    strcpy(to->locoModel, from->locoModel);
    strcpy(to->locoNumber, from->locoNumber);
    strcpy(to->locoType, from->locoType);
    strcpy(to->locoCode, from->locoCode);
    strcpy(to->route, from->route);
    to->isDetailed = 1;
}

static void name_train(LbjMsg *message)
{
    unsigned prefix = (unsigned)strlen(message->prefix);
    const char *digits=message->trainNumber;
    unsigned number;
    /* Explicit passenger prefixes override the zero-led numeric identity. */
    if (prefix) while (digits[0]=='0' && digits[1]) ++digits;
    number=(unsigned)strlen(digits);
    memcpy(message->train, message->prefix, prefix);
    memcpy(message->train + prefix, digits, number);
    message->train[prefix + number] = 0;
}

const LbjMsg *LbjState_Update(LbjState *state, const LbjMsg *message,
                              unsigned function, uint32_t now)
{
    unsigned i, slot = 0;
    uint32_t oldest = 0;
    LbjSession *session;
    LbjPairTrack *pair;
    if (function != 1 && function != 3) return 0;
    pair = &state->pair[function == 3];
    if (message->isShort || message->isMerged) {
        if (!message->trainNumber[0]) return 0;
        /* An anonymous extension can belong to either train after a switch.
         * Track switches independently of the four-entry detail cache so
         * eviction cannot accidentally make a busy stream look unambiguous. */
        if (pair->used && strcmp(pair->number, message->trainNumber) &&
            (uint32_t)(now-pair->short_at) <= LBJ_PAIR_MS) {
            pair->conflicted = 1;
            pair->conflict_at = now;
        }
        strcpy(pair->number, message->trainNumber);
        pair->short_at = now; pair->used = 1;
        if (pair->conflicted && (uint32_t)(now-pair->conflict_at) > LBJ_PAIR_MS)
            pair->conflicted = 0;
        for (i = 0; i < LBJ_SESSIONS; ++i) {
            session = &state->sessions[i];
            if (session->used && session->function == function &&
                !strcmp(session->message.trainNumber, message->trainNumber) &&
                (uint32_t)(now - session->short_at) <= LBJ_CACHE_MS) break;
        }
        if (i == LBJ_SESSIONS) {
            for (i = 0; i < LBJ_SESSIONS; ++i) {
                session = &state->sessions[i];
                if (!session->used) { slot = i; break; }
                if ((uint32_t)(now - session->short_at) >= oldest) {
                    oldest = now - session->short_at; slot = i;
                }
            }
            i = slot;
            memset(&state->sessions[i], 0, sizeof(state->sessions[i]));
        }
        session = &state->sessions[i];
        /* Carry recent details across basic speed/mileage updates. */
        if (session->has_detail && (uint32_t)(now-session->detail_at) <= LBJ_CACHE_MS) {
            strcpy(session->message.speed, message->speed);
            strcpy(session->message.position, message->position);
            strcpy(session->message.direction, message->direction);
            session->message.isShort = message->isShort;
            session->message.isMerged = message->isMerged;
            session->message.isStandalone = 0;
        } else {
            session->message = *message;
            session->has_detail = 0;
        }
        if (message->isDetailed) {
            details(&session->message, message);
            session->detail_at = now; session->has_detail = 1;
        }
        session->used = 1; session->function = function; session->short_at = now;
        state->current = (uint8_t)i; state->have_current = 1;
    } else if (message->isStandalone && state->have_current) {
        session = &state->sessions[state->current];
        if (session->function != function ||
            (uint32_t)(now-session->short_at) > LBJ_PAIR_MS) return 0;
        if (pair->conflicted &&
            (uint32_t)(now-pair->conflict_at) <= LBJ_PAIR_MS) {
            ++state->ambiguous;
            return 0;
        }
        details(&session->message, message);
        session->detail_at = now; session->has_detail = 1;
    } else return 0;
    session = &state->sessions[state->current];
    name_train(&session->message);
    return &session->message;
}

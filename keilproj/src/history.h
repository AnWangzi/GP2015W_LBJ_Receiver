#ifndef LBJ_HISTORY_H
#define LBJ_HISTORY_H
#include <stdint.h>
#include "lbj_parse.h"
#define HISTORY_CAPACITY 100u
void History_Init(void);
unsigned History_Count(void);
/* Merge repeated trains and move the latest update to index 0.
 * Prefixes disambiguate known trains; an absent prefix can be filled later. */
int History_Upsert(const LbjMsg *message);
int History_Get(unsigned index, LbjMsg *message);
uint32_t History_Id(unsigned index);
unsigned History_FindId(uint32_t id);
#endif

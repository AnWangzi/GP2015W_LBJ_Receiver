#ifndef LBJ_PARSE_H
#define LBJ_PARSE_H

typedef struct {
    char train[16];
    char speed[8];
    char position[12];
    char loco[24];
    char locoCode[8];
    char route[32]; /* Raw GBK bytes, not UTF-8. */
    char direction[8];
    int isDetailed;
    int isShort, isMerged, isStandalone;
    char trainNumber[8]; /* Numeric identity, without the transmitted prefix. */
    char prefix[3];      /* Transmitted ASCII A-Z / 0-9; no category guessing. */
    char locoModel[24];  /* UTF-8, rendered with the firmware's subset font. */
    char locoNumber[8];  /* All five transmitted registry number digits. */
    char locoType[5];    /* Four-digit lookup prefix; overlaps number digit 1. */
} LbjMsg;

/* 0 = recognized report, -1 = invalid/truncated/unsupported input. */
int parseLbj(const char *bcd, int len, unsigned addr, int func, LbjMsg *out);
/* Set only locomotive fields from exactly eight registry digits. */
int Lbj_SetRegistry(const char *digits, LbjMsg *out);
#endif

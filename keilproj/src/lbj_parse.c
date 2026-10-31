/*
 * lbj_parse.c — LBJ 列车预警报文解析（从 CR_LBJ_Receiver 的 Python 实现移植）
 *
 * 用于验证协议理解是否正确，也是后面往 HC32L176 固件里加的参考实现。
 *
 * 报文结构（POCSAG 1200bps @ 821.2375MHz）:
 *   地址 1233999/1234000  短报文(≥15 BCD) / 合并长报文(≥65 BCD)
 *   地址 1234001/1234002  独立长报文(<65 BCD, 含机车+线路)
 *   功能码 1=下行 3=上行
 *
 * 短报文:  bcd[0:6] 车次 | bcd[6:9] 速度 | bcd[10:15] 公里标
 * 长报文:  取尾部 50 个 BCD，按 *→A U→B 空格→C -→D )→E (→F 转 hex
 *          buf[0:2]  车次前缀(ASCII)
 *          buf[4:7]  机车类型(3位型号)
 *          buf[7:12] 机车登记编号(5位，不能丢首位)
 *          buf[14:30] 线路名(8 字节 GBK)
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "lbj_parse.h"

typedef struct {
    int  code;
    const char *name;
} LocoEntry;

static const LocoEntry LOCO_TABLE[] = {
    {1,"\350\247\243\346\224\276"},{3,"\345\211\215\350\277\233"},{5,"\345\273\272\350\256\276"},{6,"KD7"},{55,"\350\223\235\347\256\255\346\216\247\350\275\246"},
    {81,"\344\270\234\351\243\21621"},{101,"\344\270\234\351\243\216"},{102,"\344\270\234\351\243\2162"},{103,"\344\270\234\351\243\2163"},{104,"\344\270\234\351\243\2164"},
    {105,"\344\270\234\351\243\2164\345\256\242"},{106,"\344\270\234\351\243\2164C"},{107,"\344\270\234\351\243\2165"},{108,"\344\270\234\351\243\2165\345\256\275"},{109,"\344\270\234\351\243\2166"},
    {110,"\344\270\234\351\243\2167"},{111,"\344\270\234\351\243\2168"},{112,"\344\270\234\351\243\2169"},{113,"\344\270\234\351\243\21610"},{114,"\344\270\234\346\226\271\347\272\2421"},
    {115,"\344\270\234\346\226\271\347\272\2422"},{116,"\344\270\234\346\226\271\347\272\2423"},{117,"\344\270\234\346\226\271\347\272\2425"},{118,"\345\214\227\344\272\254"},{119,"\345\214\227\344\272\254\345\256\275"},
    {120,"ND2"},{121,"ND3"},{122,"ND4"},{123,"ND5"},{124,"NY5"},{125,"NY6"},
    {126,"NY7"},{127,"\350\275\273\346\262\271"},{128,"\344\270\234\346\226\271\347\272\24221"},{129,"\344\270\234\351\243\2167B"},{130,"\344\270\234\351\243\2165S"},
    {131,"\344\270\234\351\243\2167C"},{132,"\344\270\234\351\243\2167S"},{133,"\345\267\245\347\237\2771"},{134,"\345\267\245\347\237\2771F"},{135,"\344\270\234\351\243\2164E"},
    {136,"\344\270\234\351\243\2167D"},{137,"\345\267\245\347\237\2771A"},{138,"\344\270\234\351\243\21611"},{139,"\345\244\251\345\256\211"},{140,"\344\270\234\351\243\21610F"},
    {141,"\344\270\234\351\243\2164D"},{142,"\344\270\234\351\243\2168B"},{143,"\344\270\234\351\243\21612"},{144,"\344\270\234\351\243\2167E"},{145,"NYJ1"},
    {146,"NZJ1"},{147,"NZJ2"},{148,"\344\270\234\351\243\2164DJ"},{149,"\346\226\260\346\233\231\345\205\211"},{150,"\347\245\236\345\267\236"},
    {151,"NJ2"},{152,"\344\270\234\351\243\2167G"},{153,"NDJ3"},{157,"FXN3D"},{158,"\344\270\234\351\243\21611G"},
    {160,"HXN3"},{161,"HXN5"},{162,"HXN3B"},{163,"HXN5B"},{167,"FXN3B"},
    {170,"FXN5C"},{171,"FXN3-J"},{201,"8G"},{202,"8K"},{203,"6G"},{204,"6K"},
    {205,"\351\237\266\345\261\2611"},{206,"\351\237\266\345\261\2613"},{207,"\351\237\266\345\261\2614"},{208,"\351\237\266\345\261\2615"},{209,"\351\237\266\345\261\2616"},
    {210,"\351\237\266\345\261\2613B"},{211,"\351\237\266\345\261\2617"},{212,"\351\237\266\345\261\2618"},{213,"\351\237\266\345\261\2617B"},{214,"\351\237\266\345\261\2617C"},
    {215,"\351\237\266\345\261\2616B"},{216,"\351\237\266\345\261\2619"},{217,"\351\237\266\345\261\2617D"},{218,"DJ\347\206\212\347\214\253"},{219,"DJ1"},
    {220,"DJ2"},{221,"DJF"},{222,"\350\223\235\347\256\255\345\212\250\350\275\246"},{223,"\345\205\210\351\224\213\345\217\267"},{224,"\351\237\266\345\261\2617E"},
    {225,"\351\237\266\345\261\2614G"},{226,"\351\237\266\345\261\2613C"},{228,"\345\244\251\346\242\255"},{229,"DJ4\345\222\214\350\260\220"},{230,"KTT"},
    {231,"HXD1"},{232,"HXD2"},{233,"HXD3"},{234,"HXD1B"},{235,"HXD2B"},
    {236,"HXD3B"},{237,"HXD1C"},{238,"HXD2C"},{239,"HXD3C"},{240,"HXD1D"},
    {241,"HXD2D"},{242,"HXD3D"},{243,"FXD1B"},{244,"FXD2B"},{245,"FXD1"},
    {246,"FXD3"},{247,"FXD1-J"},{248,"FXD3-J"},{249,"KZ25TA"},{251,"KZ25TB"},
    {252,"HXD1D-J"},{254,"FXD1H"},{300,"\351\233\252\345\237\237\347\245\236\345\267\236"},{301,"CRH1"},{302,"CRH2"},
    {303,"CRH3"},{305,"CRH5"},{306,"CRH380A"},{307,"CRH380B"},{308,"CRH380C"},
    {309,"CRH380D"},{310,"CRH6A"},{311,"CR400AF"},{312,"CR400BF"},
    {313,"CR300AF"},{314,"CR300BF"},{315,"CRH2E"},{316,"CRH6F"},
    {330,"CJ1"},{331,"CJ2"},{332,"CJ3"},{333,"CJ4"},{334,"CJ5"},{335,"CJ6"},
    /* Specific registry prefixes take priority; digit four is ALSO the first
     * digit of the five-digit registered number, not a disposable subtype.
     * Mapping audit: undef-i/LBJ_Console, e68db29, assets/loco_type_info.csv. */
    {3073,"CRH380BL"},{3075,"CRH380BG"},{3111,"CR400AF-Z"},
    {3112,"CR400AF-S"},{3125,"CR400BF-G"},
};
#define LOCO_TABLE_N (int)(sizeof(LOCO_TABLE)/sizeof(LOCO_TABLE[0]))

/* BCD 字符 → hex 半字节 */
static int bcdToNibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    switch (c) {
    case '*': return 0xA;
    case 'U': return 0xB;
    case ' ': return 0xC;
    case '-': return 0xD;
    case ')': return 0xE;
    case '(': return 0xF;
    default:  return -1;
    }
}

static const char *locoName(int code)
{
    for (int i = 0; i < LOCO_TABLE_N; i++)
        if (LOCO_TABLE[i].code == code)
            return LOCO_TABLE[i].name;
    return "\346\234\252\347\237\245\345\236\213\345\217\267";
}

/* 从 hex 半字节切片取字节流 */
static int hexSliceToBytes(const char *bcd, int from, int to, unsigned char *out, int maxOut)
{
    int n = 0;
    for (int i = from; i + 1 < to; i += 2) {
        int hi = bcdToNibble(bcd[i]), lo = bcdToNibble(bcd[i + 1]);
        if (hi < 0 || lo < 0) return -1;
        if (n < maxOut) out[n++] = (unsigned char)((hi << 4) | lo);
    }
    return n;
}

int Lbj_SetRegistry(const char *digits, LbjMsg *out)
{
    unsigned i;
    char code[4], specific[5];
    const char *model;
    if (!digits || !out) return -1;
    for (i=0;i<8;++i) if (digits[i]<'0' || digits[i]>'9') return -1;
    memcpy(code,digits,3); code[3]=0;
    memcpy(specific,digits,4); specific[4]=0;
    model=locoName(atoi(specific));
    if (!strcmp(model,"\346\234\252\347\237\245\345\236\213\345\217\267")) model=locoName(atoi(code));
    strcpy(out->locoCode,code); strcpy(out->locoType,specific);
    /* Specific model lookup overlaps number[0]; retain all five digits. */
    memcpy(out->locoNumber,digits+3,5); out->locoNumber[5]=0;
    strcpy(out->locoModel,model);
    snprintf(out->loco,sizeof(out->loco),"%.17s-%s",model,
             out->locoNumber+(out->locoNumber[0]=='0'));
    return 0;
}

int parseLbj(const char *bcd, int len, unsigned addr, int func, LbjMsg *out)
{
    if (!out) return -1;
    memset(out, 0, sizeof(*out));
    if (!bcd || len < 0 || (func != 1 && func != 3)) return -1;
    strcpy(out->direction, func == 1 ? "\344\270\213\350\241\214" : func == 3 ? "\344\270\212\350\241\214" : "\346\234\252\347\237\245");

    /* --- 短报文 --- */
    if (((addr == 1233999 || addr == 1234000) && len >= 15) ||
        (addr == 1234002 && len >= 65)) {
        out->isShort = 1;
        int n = len < 6 ? len : 6;
        memcpy(out->train, bcd, n);
        out->train[n] = 0;
        /* Numeric paging uses spaces for padding; never infer a letter from
         * the train number or a timetable numbering rule. */
        {
            int start = 0, end = n, j;
            while (start < end && out->train[start] == ' ') ++start;
            while (end > start && out->train[end - 1] == ' ') --end;
            if (end == start) return -1;
            for (j = start; j < end; ++j)
                if (out->train[j] < '0' || out->train[j] > '9') return -1;
            memmove(out->train, out->train + start, end - start);
            out->train[end - start] = 0;
            strcpy(out->trainNumber, out->train);
            start = 0;
            while (out->trainNumber[start] == '0' && out->trainNumber[start+1]) ++start;
            /* Keep five-digit 001xx..004xx identities from the user's return
             * service table; strip only the sixth transport padding zero. */
            n = (int)strlen(out->trainNumber);
            if (n>=5 && n-start<5 && out->trainNumber[n-5]=='0' &&
                out->trainNumber[n-4]=='0' && out->trainNumber[n-3]>='1' &&
                out->trainNumber[n-3]<='4') start=n-5;
            if (start) memmove(out->trainNumber, out->trainNumber+start,
                               strlen(out->trainNumber+start)+1);
        }

        if (len >= 9) {
            char sp[4] = { bcd[6], bcd[7], bcd[8], 0 };
            for (int i = 0; i < 3; i++)
                if (sp[i] == ' ' || sp[i] == 'U' || sp[i] == '*') sp[i] = '0';
            if (sp[0] >= '0' && sp[0] <= '9') strcpy(out->speed, sp);
        }
        if (len >= 15) {
            char ps[6];
            memcpy(ps, bcd + 10, 5); ps[5] = 0;
            int bad = 0;
            for (int i = 0; i < 5; i++)
                if (ps[i] == '*' || ps[i] == '-' || ps[i] == 'X' || ps[i] == 'U') bad = 1;
            if (!bad) {
                for (int i = 0; i < 5; i++) if (ps[i] == ' ') ps[i] = '0';
                snprintf(out->position, sizeof(out->position), "%.4s.%c", ps, ps[4]);
            }
        }
    }

    /* --- 合并 / 独立长报文 --- */
    if (((addr == 1233999 || addr == 1234000 || addr == 1234002) && len >= 65) ||
        ((addr == 1234001 || addr == 1234002) && len >= 50 && len < 65)) {
        out->isMerged = (len >= 65);
        out->isStandalone = (len < 65);

        const char *buf = (len > 50) ? bcd + (len - 50) : bcd;

        /* 车次前缀: 2 个 ASCII 字节 */
        unsigned char pre[4];
        if (hexSliceToBytes(buf, 0, 4, pre, 2) == 2) {
            for (int i = 0; i < 2; i++)
                if (!((pre[i] >= '0' && pre[i] <= '9') || (pre[i] >= 'A' && pre[i] <= 'Z')))
                    pre[i] = 0;
            /* Preserve transmitted alphanumeric prefixes (including 0K).
             * Do not hardcode today's timetable categories. */
            char tmp[3] = {0}; int k = 0;
            for (int i = 0; i < 2; i++)
                if ((pre[i] >= 'A' && pre[i] <= 'Z') ||
                    (pre[i] >= '0' && pre[i] <= '9')) tmp[k++] = (char)pre[i];
            strcpy(out->prefix, tmp);
            if (k) { memmove(out->train + k, out->train, strlen(out->train) + 1);
                     memcpy(out->train, tmp, k); }
        }

        /* 机车代号 + 编号: buf[4:12] */
        Lbj_SetRegistry(buf+4,out);

        /* 线路名: buf[14:30] → 8 字节 GBK */
        unsigned char rb[10];
        int rn = hexSliceToBytes(buf, 14, 30, rb, 8);
        if (rn > 0) {
            int k = 0;
            for (int i = 0; i < rn && k < (int)sizeof(out->route) - 1; i++)
                if (rb[i] != 0) out->route[k++] = (char)rb[i];
            out->route[k] = 0;
        }
        out->isDetailed = 1;
    }
    return (out->isShort || out->isDetailed) ? 0 : -1;
}

/* ---------------- 测试 ---------------- */
#ifdef LBJ_PARSE_DEMO
static void dump(const char *tag, LbjMsg *m)
{
    printf("[%s]\n", tag);
    printf("  \350\275\246\346\254\241   : %s\n", m->train[0] ? m->train : "(\347\251\272)");
    printf("  \346\226\271\345\220\221   : %s\n", m->direction);
    printf("  \351\200\237\345\272\246   : %s\n", m->speed[0] ? m->speed : "(\347\251\272)");
    printf("  \345\205\254\351\207\214\346\240\207 : %s\n", m->position[0] ? m->position : "(\347\251\272)");
    printf("  \346\234\272\350\275\246   : %s\n", m->loco[0] ? m->loco : "(\347\251\272)");
    printf("  \344\273\243\345\217\267   : %s\n", m->locoCode[0] ? m->locoCode : "(\347\251\272)");
    printf("  \347\272\277\350\267\257   : %s\n", m->route[0] ? m->route : "(\347\251\272)");
    printf("  \347\261\273\345\236\213   : short=%d merged=%d standalone=%d\n\n",
           m->isShort, m->isMerged, m->isStandalone);
}

int main(void)
{
    /* 1) 短报文: 车次 T398, 速度 088, 公里标 0123.4 */
    const char *short1 = "398   088 01234";
    LbjMsg m1;
    parseLbj(short1, strlen(short1), 1233999, 3, &m1);
    dump("\347\237\255\346\212\245\346\226\207 (T398 / 088 / 0123.4)", &m1);

    /* 2) 长报文: 前缀 T / 机车 207(韶山4)-0123 / 线路 京沪线(GBK) */
    /* 京沪线 GBK = BEA9 BBA6 CFDF -> hex "BEA9BBA6CFDF" -> BCD nibble 字符 */
    char longBuf[64];
    memset(longBuf, '-', 50);          /* 全填 '-' (nibble D) */
    longBuf[50] = 0;
    /* buf[0:2] = '5','4' -> ASCII 0x54 = 'T' */
    longBuf[0] = '5'; longBuf[1] = '4';
    /* buf[4:12] = "20701234" */
    memcpy(longBuf + 4, "20701234", 8);
    /* buf[14:30] = hex "BEA9BBA6CFDF" 用 BCD 字符表示 */
    const char *hexToBcd = "0123456789*U -)(";
    const char *routeHex = "BEA9BBA6CFDF";
    for (int i = 0; i < 12; i++) {
        int nib = (routeHex[i] <= '9' && routeHex[i] >= '0')
                  ? routeHex[i] - '0' : routeHex[i] - 'A' + 10;
        longBuf[14 + i] = hexToBcd[nib];
    }

    LbjMsg m2;
    parseLbj(longBuf, 50, 1234001, 1, &m2);
    printf("  \345\216\237\345\247\213\351\225\277\346\212\245\346\226\207 BCD(%d): %s\n\n", 50, longBuf);
    dump("\351\225\277\346\212\245\346\226\207 (T / \351\237\266\345\261\2614-0123 / \344\272\254\346\262\252\347\272\277)", &m2);
    return 0;
}
#endif

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "lbj_state.h"
static LbjState state;
static LbjMsg msg;
static const LbjMsg *short_report(const char *digits, unsigned func, uint32_t now)
{
    char bcd[16];
    snprintf(bcd, sizeof(bcd), "%-6s036 06108", digits);
    assert(parseLbj(bcd, 15, 1234000, func, &msg) == 0);
    return LbjState_Update(&state, &msg, func, now);
}
static const LbjMsg *extension(const char *prefix, const char *registry,
                               unsigned func, uint32_t now)
{
    char bcd[51];
    memset(bcd, '0', 50); bcd[50] = 0;
    memcpy(bcd, prefix, 4); memcpy(bcd+4, registry, 8);
    assert(parseLbj(bcd, 50, 1234002, func, &msg) == 0);
    return LbjState_Update(&state, &msg, func, now);
}
static const LbjMsg *detail(unsigned func, uint32_t now)
{
    return extension("204U", "23900093", func, now);
}
static const LbjMsg *merged(const char *digits, const char *prefix,
                            const char *registry, unsigned func, uint32_t now)
{
    char bcd[66];
    snprintf(bcd,16,"%-6s036 06108",digits);
    memset(bcd+15,'0',50); bcd[65]=0;
    memcpy(bcd+15,prefix,4); memcpy(bcd+19,registry,8);
    assert(!parseLbj(bcd,65,1234000,func,&msg));
    return LbjState_Update(&state,&msg,func,now);
}
int main(void)
{
    const LbjMsg *m;
    uint32_t wrap = 0xFFFFFF00u;
    LbjState_Init(&state);
    assert(!detail(3, 100)); /* no train identity to associate */
    m = short_report("11588", 3, 1000); assert(!strcmp(m->train, "11588") && !m->loco[0]);
    assert(!detail(1, 1200)); /* wrong direction */
    m = detail(3, 1300);
    assert(!strcmp(m->train, "K11588") && !strcmp(m->loco, "HXD3C-0093"));
    m = short_report("11588", 3, 2000); assert(!strcmp(m->train,"K11588") && m->loco[0]);
    m = short_report("8515", 1, 3000); assert(!m->prefix[0] && !m->loco[0]);
    assert(!detail(3, 3100)); /* must not search back into another train */
    m = short_report("11588", 3, 4000); assert(!strcmp(m->train,"K11588"));
    assert(!detail(3, 12001)); /* stale short report */
    m = short_report("11588", 3, 32000); assert(!m->prefix[0] && !m->loco[0]);
    LbjState_Init(&state);
    m = short_report("1216", 3, wrap); assert(m);
    m = detail(3, wrap + 8000u); assert(m && m->prefix[0] == 'K');
    LbjState_Init(&state);
    assert(!detail(3, 10));
    m = short_report("008717",3,20); assert(!strcmp(m->train,"8717"));
    m = detail(3,30); assert(!strcmp(m->train,"K8717"));
    m = short_report("8717",3,40); assert(!strcmp(m->train,"K8717"));

    /* Reproduce the possible field failure: B's short followed by A's detail.
     * The anonymous HXD1B extension must not become 8324's locomotive. */
    LbjState_Init(&state);
    short_report("8325",3,100);
    m=extension("2020","23400001",3,200); assert(!strcmp(m->loco,"HXD1B-0001"));
    m=short_report("8324",3,300); assert(!m->loco[0]);
    assert(!extension("2020","23400001",3,400));
    assert(!state.sessions[state.current].message.loco[0]);
    short_report("8324",3,8300);
    assert(!extension("2020","24000002",3,8300)); /* inclusive conflict boundary */
    m=extension("2020","24000002",3,8301);
    assert(m && !strcmp(m->train,"8324") && !strcmp(m->loco,"HXD1D-0002"));

    /* Continuous interleaving extends the gate; merged messages remain usable. */
    short_report("8325",3,8400);
    short_report("8324",3,8500);
    assert(!detail(3,16400));
    m=merged("8324","204U","24000002",3,16401);
    assert(m && !strcmp(m->train,"K8324") && !strcmp(m->loco,"HXD1D-0002"));
    m=short_report("8324",3,16402); assert(!strcmp(m->loco,"HXD1D-0002"));
    assert(!detail(3,16500)); /* merged success does not cancel the conflict gate */

    /* Fresh details replace all detail fields, even if registry/route absent. */
    m=extension("2047","********",3,16501);
    assert(m && !strcmp(m->train,"G8324") && !m->loco[0] && !m->locoModel[0]);
    assert(!m->locoNumber[0] && !m->locoCode[0] && !m->locoType[0] && !m->route[0]);
    m=short_report("8324",3,16502); assert(!strcmp(m->train,"G8324") && !m->loco[0]);

    /* Direction streams are independent; old, non-overlapping trains are safe. */
    LbjState_Init(&state);
    short_report("1",1,100); short_report("2",3,200);
    assert(detail(3,201));
    short_report("3",1,8201); assert(detail(1,8202));

    /* More trains than cache slots cannot erase the ambiguity observation. */
    LbjState_Init(&state);
    short_report("1",3,100); short_report("2",3,200);
    short_report("3",3,300); short_report("4",3,400);
    short_report("5",3,500); assert(!detail(3,501));

    /* Conflict timeout is also unsigned-wrap safe and refreshed by switches. */
    LbjState_Init(&state);
    short_report("1",3,wrap); short_report("2",3,wrap+100u);
    short_report("2",3,wrap+8100u); assert(!detail(3,wrap+8100u));
    assert(detail(3,wrap+8101u));
    assert(!LbjState_Update(&state,&msg,2,wrap+8102u));
    puts("PASS LBJ association: retention, interleaved trains, atomic details, merged bypass, direction, eviction, stale, wrap");
    return 0;
}

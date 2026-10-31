/* Synthetic payload order through real parser, association and RAM history.
 * These regression fixtures are not captured field messages. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "lbj_state.h"
#include "history.h"
static LbjState state;
static LbjMsg live;
static unsigned updates;
static int receive(const char *bcd,unsigned length,unsigned address,unsigned function,uint32_t now)
{
    LbjMsg parsed;
    const LbjMsg *accepted;
    assert(!parseLbj(bcd,(int)length,address,(int)function,&parsed));
    accepted=LbjState_Update(&state,&parsed,function,now);
    if (!accepted) return 0;
    live=*accepted;
    assert(History_Upsert(accepted)); ++updates;
    return 1;
}
static int basic(const char *number,unsigned function,uint32_t now)
{
    char bcd[16];
    snprintf(bcd,sizeof(bcd),"%-6s036 06108",number);
    return receive(bcd,15,1234000,function,now);
}
static int extension(const char *registry,unsigned function,uint32_t now)
{
    char bcd[51];
    memset(bcd,'0',50); bcd[50]=0;
    memcpy(bcd,"2020",4); memcpy(bcd+4,registry,8);
    return receive(bcd,50,1234002,function,now);
}
int main(void)
{
    LbjMsg saved;
    uint32_t first;
    LbjState_Init(&state); History_Init();
    assert(basic("8325",3,100)); first=History_Id(0);
    assert(extension("23400001",3,200));
    assert(!strcmp(live.locoModel,"HXD1B"));
    assert(basic("8324",3,300));
    assert(!strcmp(live.train,"8324") && !live.loco[0]);
    assert(!extension("23400001",3,400)); /* delayed A after B */
    assert(!extension("24000002",3,500)); /* B is ambiguous as well */
    assert(updates==3 && History_Count()==2 && state.ambiguous==2);
    assert(History_Get(0,&saved) && !strcmp(saved.train,"8324") && !saved.loco[0]);
    assert(History_Get(1,&saved) && !strcmp(saved.train,"8325") &&
           !strcmp(saved.locoModel,"HXD1B") && History_Id(1)==first);
    assert(basic("8324",3,8301));
    assert(extension("24000002",3,8302));
    assert(!strcmp(live.train,"8324") && !strcmp(live.locoModel,"HXD1D"));
    assert(History_Count()==2 && History_Get(0,&saved) &&
           !strcmp(saved.locoModel,"HXD1D") && !strcmp(saved.locoNumber,"00002"));
    assert(History_Get(1,&saved) && !strcmp(saved.locoModel,"HXD1B"));
    puts("PASS parser -> association -> live/history: interleaved details rejected; independent trains retained");
    return 0;
}

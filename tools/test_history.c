#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "history.h"
static LbjMsg message, saved;
static void make(const char *number,const char *prefix,unsigned speed)
{
    char bcd[16];
    snprintf(bcd,sizeof(bcd),"%-6s%03u 01234",number,speed);
    assert(parseLbj(bcd,15,1234000,3,&message)==0);
    strcpy(message.prefix,prefix);
}
int main(void)
{
    unsigned i;
    uint32_t id, selected;
    char number[8];
    History_Init(); assert(!History_Count() && !History_Get(0,&saved));
    memset(&message,0,sizeof(message)); strcpy(message.prefix,"K");
    assert(!History_Upsert(&message)); /* unassociated extension is not a train */
    for (i=10000;i<10105;++i) {
        snprintf(number,sizeof(number),"%u",i); make(number,"",88);
        assert(History_Upsert(&message));
    }
    assert(History_Count()==100);
    assert(History_Get(0,&saved) && !strcmp(saved.train,"10104"));
    assert(History_Get(99,&saved) && !strcmp(saved.train,"10005"));
    assert(!History_Get(100,&saved));
    selected=History_Id(1); id=History_Id(54); /* 10050 */
    make("10050","G",300); strcpy(message.position,"9876.5");
    assert(!Lbj_SetRegistry("30757000",&message));
    assert(History_Upsert(&message) && History_Count()==100);
    assert(History_Id(0)==id && History_FindId(selected)==2);
    assert(History_Get(0,&saved) && !strcmp(saved.train,"G10050"));
    assert(!strcmp(saved.speed,"300") && !strcmp(saved.position,"9876.5"));
    assert(!strcmp(saved.loco,"CRH380BG-57000") && !strcmp(saved.locoNumber,"57000"));
    make("10050","",42); message.position[0]=0;
    History_Upsert(&message); assert(History_Count()==100 && History_Id(0)==id);
    History_Get(0,&saved);
    assert(!strcmp(saved.train,"G10050") && !strcmp(saved.speed,"042"));
    assert(!strcmp(saved.position,"9876.5") && !strcmp(saved.loco,"CRH380BG-57000"));
    make("10050","K",123); History_Upsert(&message);
    assert(History_Count()==100 && History_Id(0)!=id && History_FindId(id)==1);
    History_Get(0,&saved); assert(!strcmp(saved.train,"K10050") && !saved.loco[0]);
    /* A late prefix and differently padded number still identify one train. */
    History_Init(); make("000123","",99); History_Upsert(&message);
    id=History_Id(0); History_Get(0,&saved); assert(!strcmp(saved.train,"00123"));
    strcpy(message.prefix,"K"); History_Upsert(&message);
    make("123","K",100); History_Upsert(&message);
    assert(History_Count()==1 && History_Id(0)==id);
    History_Get(0,&saved); assert(!strcmp(saved.train,"K123") && !strcmp(saved.speed,"100"));
    make("123","K",101); assert(!Lbj_SetRegistry("23900093",&message));
    History_Upsert(&message); History_Get(0,&saved);
    assert(!strcmp(saved.loco,"HXD3C-0093") && !strcmp(saved.locoNumber,"00093"));
    /* A new accepted detail snapshot cannot resurrect the old locomotive. */
    make("123","K",102); message.isDetailed=1;
    History_Upsert(&message); History_Get(0,&saved);
    assert(!strcmp(saved.train,"K123") && !saved.locoModel[0] && !saved.locoNumber[0]);
    /* An explicitly empty prefix in a new detail snapshot is empty, too. */
    make("123","",103); message.isDetailed=1;
    History_Upsert(&message); History_Get(0,&saved);
    assert(!strcmp(saved.train,"123") && !saved.locoModel[0]);
    /* Distinct trains never inherit details from the last record. */
    make("8325","",104); message.isDetailed=1;
    assert(!Lbj_SetRegistry("23400001",&message)); History_Upsert(&message);
    make("8324","",105); History_Upsert(&message); History_Get(0,&saved);
    assert(!strcmp(saved.train,"8324") && !saved.locoModel[0]);
    History_Get(1,&saved); assert(!strcmp(saved.locoModel,"HXD1B"));
    History_Init(); assert(!History_Count() && !History_Id(0) && !History_Get(0,&saved));
    puts("PASS history: 100 unique trains, latest-first dedup, eviction, enrichment, padding, stable selection, reset");
    return 0;
}

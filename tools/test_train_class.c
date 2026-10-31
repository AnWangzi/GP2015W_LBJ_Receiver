#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "train_class.h"
#include "lbj_parse.h"
#include "lbj_state.h"
typedef struct { const char *number; TrainClass kind; } Case;
static const Case numeric[] = {
    {"00100",TrainEmuReturn},{"00123",TrainEmuReturn},{"00299",TrainEmuReturn},
    {"00300",TrainCoachReturn},{"00499",TrainCoachReturn},
    {"10000",TrainDirectFreight},{"19999",TrainDirectFreight},
    {"20000",TrainCrossFreight},{"29999",TrainCrossFreight},
    {"30000",TrainSectionWork},{"39999",TrainSectionWork},
    {"40000",TrainPickup},{"44999",TrainPickup},{"45000",TrainLocalWork},{"49999",TrainLocalWork},
    {"50000",TrainPassengerLoco},{"50999",TrainPassengerLoco},
    {"51000",TrainFreightLoco},{"51999",TrainFreightLoco},
    {"52000",TrainLocalLoco},{"52999",TrainLocalLoco},
    {"53000",TrainBankerReturn},{"53999",TrainBankerReturn},{"54000",TrainBankerReturn},{"54999",TrainBankerReturn},
    {"55100",TrainOrdinaryMixed},{"55200",TrainOrdinaryMixed},{"55399",TrainOrdinaryMixed},
    {"55400",TrainTrial},{"55999",TrainTrial},{"56000",TrainRailcar},{"56999",TrainRailcar},
    {"57000",TrainEngineering},{"57999",TrainEngineering},{"58000",TrainRescue},{"58999",TrainRescue},
    {"60000",TrainPrivate},{"69999",TrainPrivate},{"70000",TrainSpecialFreight},{"79999",TrainSpecialFreight},
    {"80000",TrainFastFreight},{"89999",TrainFastFreight},{"90000",TrainMilitary},{"99999",TrainMilitary},
    {"00099",TrainUnknown},{"00500",TrainUnknown},{"09999",TrainUnknown},
    {"55000",TrainUnknown},{"55099",TrainUnknown},{"59000",TrainUnknown},{"59999",TrainUnknown},
    {"1216",TrainUnknown},{"8515",TrainUnknown},{"123",TrainUnknown},
    {"100000",TrainUnknown},{"",TrainUnknown},{"25-30",TrainUnknown},{"00000",TrainUnknown}
};
static const Case k[] = {
    {"1",TrainKRapidCross},{"3998",TrainKRapidCross},
    {"4001",TrainKTemporaryCross},{"4998",TrainKTemporaryCross},
    {"5001",TrainKTemporaryLocal},{"6998",TrainKTemporaryLocal},
    {"7001",TrainKRapidLocal},{"8515",TrainKRapidLocal},{"9998",TrainKRapidLocal},
    {"3999",TrainRapid},{"4000",TrainRapid},{"4999",TrainRapid},{"5000",TrainRapid},
    {"6999",TrainRapid},{"7000",TrainRapid},{"9999",TrainRapid},{"11588",TrainRapid}
};
int main(void)
{
    unsigned i;
    LbjMsg message;
    LbjState state;
    const LbjMsg *display;
    const char *prefixes[]={"G","D","C","S","Z","T","Y","X"};
    const TrainClass types[]={TrainHighSpeed,TrainEmu,TrainIntercity,TrainSuburban,TrainDirectExpress,TrainExpress,TrainTourist,TrainFastCargo};
    for (i=0;i<sizeof(numeric)/sizeof(numeric[0]);++i)
        assert(Train_Classify("",numeric[i].number)==numeric[i].kind);
    for (i=0;i<sizeof(k)/sizeof(k[0]);++i) assert(Train_Classify("K",k[i].number)==k[i].kind);
    for (i=0;i<sizeof(types)/sizeof(types[0]);++i) {
        assert(Train_Classify(prefixes[i],"8515")==types[i]);
        assert(Train_Classify(prefixes[i],"25230")==types[i]);
    }
    assert(Train_Classify("W","8515")==TrainUnknown);
    assert(Train_Classify("0K","8515")==TrainUnknown);
    assert(Train_Classify("K","")==TrainUnknown);
    assert(Train_Classify(0,"8515")==TrainUnknown);
    assert(Train_Classify("",0)==TrainUnknown);
    assert(parseLbj("000123036 06108",15,1234000,3,&message)==0);
    assert(!strcmp(message.trainNumber,"00123"));
    LbjState_Init(&state);
    display=LbjState_Update(&state,&message,3,0);
    assert(!strcmp(display->train,"00123"));
    assert(Train_Classify(display->prefix,display->trainNumber)==TrainEmuReturn);
    {
        char extension[51];
        memset(extension,'0',50); extension[50]=0;
        memcpy(extension,"204U23900093",12);
        assert(parseLbj(extension,50,1234002,3,&message)==0);
        display=LbjState_Update(&state,&message,3,100);
        assert(!strcmp(display->train,"K123"));
        assert(Train_Classify(display->prefix,display->trainNumber)==TrainKRapidCross);
    }
    assert(parseLbj("000398036 06108",15,1234000,3,&message)==0);
    assert(!strcmp(message.trainNumber,"00398"));
    assert(Train_Classify("",message.trainNumber)==TrainCoachReturn);
    assert(parseLbj("008717036 06108",15,1234000,3,&message)==0);
    assert(!strcmp(message.trainNumber,"8717"));
    puts("PASS user train table: all numeric/K endpoints and gaps, passenger prefix priority, 001xx identity and late extension reclassification");
    return 0;
}

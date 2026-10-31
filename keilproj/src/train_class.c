#include "train_class.h"

typedef struct { unsigned first, last; TrainClass kind; } NumberRule;
static const NumberRule numeric_rules[] = {
    {100,299,TrainEmuReturn}, {300,499,TrainCoachReturn},
    {10000,19999,TrainDirectFreight}, {20000,29999,TrainCrossFreight},
    {30000,39999,TrainSectionWork}, {40000,44999,TrainPickup},
    {45000,49999,TrainLocalWork}, {50000,50999,TrainPassengerLoco},
    {51000,51999,TrainFreightLoco}, {52000,52999,TrainLocalLoco},
    {53000,54999,TrainBankerReturn}, {55100,55399,TrainOrdinaryMixed},
    {55400,55999,TrainTrial}, {56000,56999,TrainRailcar},
    {57000,57999,TrainEngineering}, {58000,58999,TrainRescue},
    {60000,69999,TrainPrivate}, {70000,79999,TrainSpecialFreight},
    {80000,89999,TrainFastFreight}, {90000,99999,TrainMilitary}
};
static const NumberRule k_rules[] = {
    {1,3998,TrainKRapidCross}, {4001,4998,TrainKTemporaryCross},
    {5001,6998,TrainKTemporaryLocal}, {7001,9998,TrainKRapidLocal}
};
static TrainClass match(unsigned number,const NumberRule *rules,unsigned count,TrainClass fallback)
{
    unsigned i;
    for (i=0;i<count;++i)
        if (number>=rules[i].first && number<=rules[i].last) return rules[i].kind;
    return fallback;
}
TrainClass Train_Classify(const char *prefix,const char *number)
{
    unsigned value=0, digits=0;
    if (!number || !prefix) return TrainUnknown;
    while (number[digits]) {
        if (digits>=6 || number[digits]<'0' || number[digits]>'9') return TrainUnknown;
        value=value*10u+(unsigned)(number[digits++]-'0');
    }
    if (!digits || !value) return TrainUnknown;
    if (prefix[0]) {
        if (prefix[1]) return TrainUnknown;
        switch (prefix[0]) {
        case 'G': return TrainHighSpeed;
        case 'D': return TrainEmu;
        case 'C': return TrainIntercity;
        case 'S': return TrainSuburban;
        case 'Z': return TrainDirectExpress;
        case 'T': return TrainExpress;
        case 'Y': return TrainTourist;
        case 'X': return TrainFastCargo;
        case 'K': return match(value,k_rules,sizeof(k_rules)/sizeof(k_rules[0]),TrainRapid);
        default: return TrainUnknown;
        }
    }
    if (digits!=5) return TrainUnknown;
    return match(value,numeric_rules,sizeof(numeric_rules)/sizeof(numeric_rules[0]),TrainUnknown);
}

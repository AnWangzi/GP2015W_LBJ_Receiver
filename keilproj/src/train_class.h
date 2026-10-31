#ifndef TRAIN_CLASS_H
#define TRAIN_CLASS_H
typedef enum {
    TrainUnknown, TrainHighSpeed, TrainEmu, TrainIntercity, TrainSuburban,
    TrainDirectExpress, TrainExpress, TrainRapid, TrainTourist, TrainFastCargo,
    TrainKRapidCross, TrainKTemporaryCross, TrainKTemporaryLocal, TrainKRapidLocal,
    TrainEmuReturn, TrainCoachReturn, TrainDirectFreight, TrainCrossFreight,
    TrainSectionWork, TrainPickup, TrainLocalWork, TrainPassengerLoco,
    TrainFreightLoco, TrainLocalLoco, TrainBankerReturn, TrainOrdinaryMixed,
    TrainTrial, TrainRailcar, TrainEngineering, TrainRescue, TrainPrivate,
    TrainSpecialFreight, TrainFastFreight, TrainMilitary, TrainClassCount
} TrainClass;
/* User-supplied 2026-10-25 table. Letter prefixes take priority; five-digit
 * numeric rules never classify a passenger service with a letter prefix. */
TrainClass Train_Classify(const char *prefix, const char *number);
#endif

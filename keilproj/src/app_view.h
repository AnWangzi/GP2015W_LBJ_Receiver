#ifndef APP_VIEW_H
#define APP_VIEW_H
#include "app_ui.h"
#include "lbj_parse.h"
void AppView_Menu(const AppUi *ui);
/* Four rows: train, speed/mileage, locomotive, category; caller draws battery.
 * elapsed_ms drives nonblocking category scrolling, reset on a new train. */
void AppView_Train(const LbjMsg *message, uint32_t elapsed_ms);
void AppView_History(const LbjMsg *message, unsigned index, unsigned count);
void AppView_Waiting(int ready, int failed);
void AppView_Battery(int valid, unsigned level, int external, int charging, int phase);
#endif

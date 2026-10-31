#ifndef APP_UI_H
#define APP_UI_H
#include <stdint.h>

typedef enum { UiMain, UiDiagnostics, UiBattery, UiMenu, UiUpgrade, UiSelfTest, UiHistory } UiScreen;
typedef enum { MenuLight, MenuSound, MenuHistory, MenuDiagnostics, MenuBattery, MenuSelfTest, MenuUpgrade, MenuPowerOff, MenuBack, MenuCount } UiItem;
enum {
    UiActionLight = 1u, UiActionSound = 2u, UiActionTest = 4u,
    UiActionStopSound = 8u, UiActionPowerOff = 16u, UiActionUpgrade = 32u
};
typedef struct {
    UiScreen screen, parent;
    uint8_t selected, light_mode, sound_enabled, diagnostic_page;
    uint8_t history_index, history_count;
} AppUi;

void AppUi_Init(AppUi *ui);
uint8_t AppUi_Keys(AppUi *ui, uint16_t events);
#endif

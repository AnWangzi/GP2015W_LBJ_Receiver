#include "app_ui.h"
#include "buttons.h"

void AppUi_Init(AppUi *ui)
{
    ui->screen = UiMain;
    ui->parent = UiMain;
    ui->selected = MenuLight;
    ui->light_mode = 0;
    ui->sound_enabled = 1;
    ui->diagnostic_page = 0;
    ui->history_index = ui->history_count = 0;
}

uint8_t AppUi_Keys(AppUi *ui, uint16_t events)
{
    /* Green wins over simultaneous events and never confirms an item. */
    if (events & BUTTON_SHORT(0)) {
        ui->screen = ui->screen == UiMenu ? UiMain : ui->parent;
        ui->parent = UiMain;
        return UiActionStopSound;
    }
    if (ui->screen == UiMain) {
        if (events & BUTTON_SHORT(3)) {
            ui->screen = UiMenu;
        } else if (events & (BUTTON_PRESS(1) | BUTTON_PRESS(2))) {
            ui->parent = UiMain;
            ui->screen = UiHistory;
            ui->history_index = 0;
        }
        return 0;
    }
    if (ui->screen != UiMenu) {
        if (ui->screen == UiHistory &&
            (events & (BUTTON_PRESS(1) | BUTTON_PRESS(2)))) {
            if ((events & (BUTTON_PRESS(1) | BUTTON_PRESS(2))) ==
                (BUTTON_PRESS(1) | BUTTON_PRESS(2))) return 0;
            if ((events & BUTTON_PRESS(1)) && ui->history_index+1u < ui->history_count)
                ++ui->history_index;
            else if ((events & BUTTON_PRESS(2)) && ui->history_index) --ui->history_index;
            return 0;
        }
        if (ui->screen == UiDiagnostics &&
            (events & (BUTTON_PRESS(1) | BUTTON_PRESS(2)))) {
            if ((events & (BUTTON_PRESS(1) | BUTTON_PRESS(2))) !=
                (BUTTON_PRESS(1) | BUTTON_PRESS(2)))
                ui->diagnostic_page = (uint8_t)((ui->diagnostic_page +
                    ((events & BUTTON_PRESS(1)) ? 2 : 1)) % 3);
            return 0;
        }
        /* A child page never treats UP/DOWN as a shortcut to the home page.
         * Red and green both return to its actual parent and keep selection. */
        if (events & BUTTON_SHORT(3)) {
#ifdef LBJ_IAP_APP
            if (ui->screen == UiUpgrade) return UiActionUpgrade;
#endif
            ui->screen = ui->parent;
            ui->parent = UiMain;
        }
        return 0;
    }
    if (events & (BUTTON_PRESS(1) | BUTTON_PRESS(2))) {
        if ((events & (BUTTON_PRESS(1) | BUTTON_PRESS(2))) ==
            (BUTTON_PRESS(1) | BUTTON_PRESS(2))) return 0;
        if (events & BUTTON_PRESS(1))
            ui->selected = (uint8_t)((ui->selected + MenuCount - 1) % MenuCount);
        else ui->selected = (uint8_t)((ui->selected + 1) % MenuCount);
        return 0; /* A direction event cannot also confirm a new selection. */
    }
    if (!(events & BUTTON_SHORT(3))) return 0;
    switch (ui->selected) {
    case MenuLight:
        ui->light_mode = (uint8_t)((ui->light_mode + 1) % 3);
        return UiActionLight;
    case MenuSound:
        ui->sound_enabled ^= 1;
        return UiActionSound;
    case MenuDiagnostics:
        ui->parent = UiMenu;
        ui->screen = UiDiagnostics;
        ui->diagnostic_page = 0;
        return 0;
    case MenuHistory:
        ui->parent = UiMenu;
        ui->screen = UiHistory;
        ui->history_index = 0;
        return 0;
    case MenuBattery:
        ui->parent = UiMenu;
        ui->screen = UiBattery;
        return 0;
    case MenuSelfTest:
        ui->parent = UiMenu;
        ui->screen = UiSelfTest;
        return UiActionTest;
    case MenuUpgrade:
        ui->parent = UiMenu;
        ui->screen = UiUpgrade;
        return 0;
    case MenuPowerOff: return UiActionPowerOff;
    default: ui->screen = UiMain; return 0;
    }
}

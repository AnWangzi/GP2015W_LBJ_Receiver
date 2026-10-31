#include <assert.h>
#include <stdio.h>
#include "buttons.h"
#include "app_ui.h"
#include "battery_model.h"

static void open_item(AppUi *u, unsigned item)
{
    unsigned i;
    AppUi_Init(u);
    assert(!AppUi_Keys(u, BUTTON_SHORT(3)));
    assert(u->screen == UiMenu && u->light_mode == 0);
    for (i = 0; i < item; ++i) assert(!AppUi_Keys(u, BUTTON_PRESS(2)));
    assert(u->selected == item);
}
int main(void)
{
    AppUi u;
    PowerHold h;
    uint32_t start = 0xFFFFFF00u;
    unsigned i;
    const unsigned thresholds[] = {2816, 3008, 3088, 3152, 3264};
    PowerHold_Init(&h);
    assert(!PowerHold_Update(&h, 1, 0));
    assert(!PowerHold_Update(&h, 1, 1499));
    assert(PowerHold_Update(&h, 1, 1500));
    assert(!PowerHold_Update(&h, 1, 6000));
    assert(!PowerHold_Update(&h, 0, 6100));
    assert(!PowerHold_Update(&h, 1, 6200));
    assert(!PowerHold_Update(&h, 0, 7699)); /* early release resets hold */
    assert(!PowerHold_Update(&h, 1, 7700));
    assert(!PowerHold_Update(&h, 1, 9199));
    assert(PowerHold_Update(&h, 1, 9200));
    PowerHold_Init(&h);
    assert(!PowerHold_Update(&h, 1, start));
    assert(!PowerHold_Update(&h, 1, start + 1499));
    assert(PowerHold_Update(&h, 1, start + 1500));

    open_item(&u, MenuLight);
    assert(!AppUi_Keys(&u, BUTTON_SHORT(1)|BUTTON_SHORT(2)) && u.selected == MenuLight);
    for (i = 1; i <= 3; ++i) {
        assert(AppUi_Keys(&u, BUTTON_SHORT(3)) == UiActionLight);
        assert(u.light_mode == i % 3);
    }
    open_item(&u, MenuSound);
    assert(AppUi_Keys(&u, BUTTON_SHORT(3)) == UiActionSound && !u.sound_enabled);
    assert(AppUi_Keys(&u, BUTTON_SHORT(3)) == UiActionSound && u.sound_enabled);
    open_item(&u, MenuDiagnostics);
    assert(!AppUi_Keys(&u, BUTTON_SHORT(3)) && u.screen == UiDiagnostics);
    assert(AppUi_Keys(&u, BUTTON_SHORT(0)) == UiActionStopSound && u.screen == UiMenu);
    assert(u.selected == MenuDiagnostics);
    assert(!AppUi_Keys(&u, BUTTON_SHORT(3)) && u.screen == UiDiagnostics);
    assert(!AppUi_Keys(&u, BUTTON_PRESS(1)) && u.screen == UiDiagnostics);
    assert(u.diagnostic_page == 2);
    assert(!AppUi_Keys(&u, BUTTON_PRESS(2)) && u.diagnostic_page == 0);
    assert(!AppUi_Keys(&u, BUTTON_PRESS(2)) && u.diagnostic_page == 1);
    assert(!AppUi_Keys(&u, BUTTON_PRESS(1)|BUTTON_PRESS(2)) && u.diagnostic_page == 1);
    assert(!AppUi_Keys(&u, BUTTON_SHORT(3)) && u.screen == UiMenu);
    assert(AppUi_Keys(&u, BUTTON_SHORT(0)) == UiActionStopSound && u.screen == UiMain);
    assert(!AppUi_Keys(&u, BUTTON_SHORT(3)) && u.selected == MenuDiagnostics);
    open_item(&u, MenuBattery);
    assert(!AppUi_Keys(&u, BUTTON_SHORT(3)) && u.screen == UiBattery);
    assert(AppUi_Keys(&u, BUTTON_SHORT(0)) == UiActionStopSound && u.screen == UiMenu);
    assert(u.selected == MenuBattery);
    AppUi_Init(&u);
    assert(!AppUi_Keys(&u, BUTTON_PRESS(2)) && u.screen == UiHistory);
    assert(!AppUi_Keys(&u, BUTTON_PRESS(1)) && u.history_index == 0); /* empty */
    u.history_count=100;
    for (i=0;i<120;++i) AppUi_Keys(&u,BUTTON_PRESS(1));
    assert(u.history_index==99);
    assert(!AppUi_Keys(&u,BUTTON_PRESS(2)) && u.history_index==98);
    assert(!AppUi_Keys(&u,BUTTON_PRESS(1)|BUTTON_PRESS(2)) && u.history_index==98);
    assert(AppUi_Keys(&u, BUTTON_SHORT(0)) == UiActionStopSound && u.screen == UiMain);
    open_item(&u,MenuHistory);
    assert(!AppUi_Keys(&u,BUTTON_SHORT(3)) && u.screen==UiHistory && u.history_index==0);
    assert(AppUi_Keys(&u,BUTTON_SHORT(0))==UiActionStopSound && u.screen==UiMenu);
    assert(u.selected==MenuHistory);
    open_item(&u, MenuSelfTest);
    assert(AppUi_Keys(&u, BUTTON_SHORT(3)) == UiActionTest);
    assert(u.screen == UiSelfTest);
    assert(AppUi_Keys(&u, BUTTON_SHORT(0)) == UiActionStopSound && u.screen == UiMenu);
    assert(u.selected == MenuSelfTest);
    open_item(&u, MenuPowerOff);
    assert(!AppUi_Keys(&u, BUTTON_LONG(0) | BUTTON_LONG(3)));
    assert(AppUi_Keys(&u, BUTTON_SHORT(3)) == UiActionPowerOff);
    assert(AppUi_Keys(&u, BUTTON_SHORT(0) | BUTTON_SHORT(3)) == UiActionStopSound);
    assert(u.screen == UiMain); /* green never confirms power-off */
    open_item(&u, MenuLight);
    assert(!AppUi_Keys(&u, BUTTON_PRESS(1)) && u.selected == MenuBack);
    assert(!AppUi_Keys(&u, BUTTON_PRESS(2)) && u.selected == MenuLight);
    assert(!AppUi_Keys(&u, BUTTON_PRESS(1) | BUTTON_PRESS(2)) && u.selected == MenuLight);
    open_item(&u, MenuSelfTest);
    assert(!AppUi_Keys(&u, BUTTON_PRESS(2) | BUTTON_SHORT(3)));
    assert(u.selected == MenuUpgrade); /* navigation does not also confirm */
    assert(!AppUi_Keys(&u, BUTTON_SHORT(3)) && u.screen == UiUpgrade);
#ifdef LBJ_IAP_APP
    assert(AppUi_Keys(&u, BUTTON_SHORT(3)) == UiActionUpgrade);
#else
    assert(!AppUi_Keys(&u, BUTTON_SHORT(3)) && u.screen == UiMenu);
#endif
    open_item(&u, MenuUpgrade);
    assert(!AppUi_Keys(&u, BUTTON_SHORT(3)) && u.screen == UiUpgrade);
    assert(AppUi_Keys(&u, BUTTON_SHORT(0)|BUTTON_SHORT(3)) == UiActionStopSound);
    assert(u.screen == UiMenu && u.selected == MenuUpgrade);
    open_item(&u, MenuBack);
    assert(!AppUi_Keys(&u, BUTTON_SHORT(3)) && u.screen == UiMain);

    for (i = 0; i < 5; ++i) {
        assert(Battery_Level(thresholds[i]) == i);
        assert(Battery_Level(thresholds[i] + 1) == i + 1);
    }
    assert(Battery_Level(0) == 0 && Battery_Level(4095) == 5);
    assert(Battery_Status(0, 1, 1, 5) == BatterySupply);
    assert(Battery_Status(1, 1, 1, 5) == BatteryCharging);
    assert(Battery_Status(1, 0, 1, 5) == BatteryFull);
    assert(Battery_Status(1, 0, 0, 5) == BatteryExternal);
    assert(Battery_Status(1, 0, 1, 4) == BatteryExternal);
    puts("PASS menu navigation/confirmation, power hold/release/wrap, battery thresholds/charge status");
    return 0;
}

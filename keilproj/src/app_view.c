#include <stdio.h>
#include "app_view.h"
#include "lcd_text.h"
#include "lcd_buffer.h"
#include "ui_text.h"
#include "battery_icon.h"
#include "lcd_8080.h"
#include "train_class.h"
#include "train_labels.h"

typedef char TrainLabelCountMustMatchEnum[
    sizeof(text_train_classes)/sizeof(text_train_classes[0]) == TrainClassCount ? 1 : -1];

static void clear(int page) { LCD_DrawAscii8(page, 0, "                "); }
void AppView_Menu(const AppUi *ui)
{
    char index[8];
    unsigned i, first;
    static const uint16_t *const labels[] = {text_light, text_sound, text_history, text_diagnostics,
        text_battery, text_self_test, text_upgrade, text_power_off, text_back};
    clear(0); LCD_DrawStr16(0, 0, text_menu);
    snprintf(index, sizeof(index), "%u/%u", (unsigned)ui->selected + 1, (unsigned)MenuCount);
    LCD_DrawAscii8(0, 72, index);
    first = ui->selected > 0 ? ui->selected - 1 : 0;
    if (first > MenuCount - 3) first = MenuCount - 3;
    for (i = 0; i < 3; ++i) {
        unsigned item = first + i;
        int page = 2 + 2 * i;
        clear(page);
        LCD_DrawStr16(page, 8, labels[item]);
        if (item == MenuLight)
            LCD_DrawStr16(page, 80, ui->light_mode == 0 ? text_auto : ui->light_mode == 1 ? text_always : text_off);
        if (item == MenuSound)
            LCD_DrawStr16(page, 96, ui->sound_enabled ? text_on : text_off);
        if (item == ui->selected) LcdBuffer_InvertRow(page);
    }
}
void AppView_Train(const LbjMsg *message, uint32_t elapsed_ms)
{
    char line[24];
    const uint16_t *label = text_train_classes[Train_Classify(message->prefix,message->trainNumber)];
    unsigned length = 0, offset = 0;
    clear(0); LCD_DrawAscii8(0, 0, message->train);
    snprintf(line, sizeof(line), "V:%.3s K:%.6s", message->speed, message->position);
    clear(2); LCD_DrawAscii8(2, 0, line);
    clear(4);
    if (message->loco[0]) LCD_DrawUtf8(4, 0, message->loco);
    else LCD_DrawStr16(4, 0, text_wait_detail);
    clear(6);
    while (label[length]) ++length;
    if (length > 8) {
        uint32_t phase = elapsed_ms % (4000u + (length-8u)*600u);
        if (phase > 2000u) offset = (unsigned)((phase-2000u)/600u);
        if (offset > length-8u) offset = length-8u;
    }
    LCD_DrawStr16(6,0,label+offset);
}
void AppView_History(const LbjMsg *message, unsigned index, unsigned count)
{
    char position[12];
    if (count) AppView_Train(message, 0);
    else {
        clear(0); clear(2); clear(4);
        LCD_DrawStr16(2, 16, text_no_history);
    }
    clear(6); LCD_DrawStr16(6,0,text_history);
    snprintf(position,sizeof(position),"%03u/%03u",count ? index+1u : 0u,count);
    LCD_DrawAscii8(6,72,position);
}
void AppView_Waiting(int ready, int failed)
{
    clear(0); clear(2); clear(4); clear(6);
    if (failed) {
        LCD_DrawStr16(2,0,text_failed);
        LCD_DrawStr16(4,8,text_see_diagnostics);
    } else if (ready) LCD_DrawStr16(2,8,text_wait);
    else LCD_DrawStr16(2,32,text_boot);
}
void AppView_Battery(int valid, unsigned level, int external, int charging, int phase)
{
    unsigned x, p;
    for (p=0;p<2;++p) {
        LCD_SetPos((int)p,96);
        for (x=0;x<32;++x)
            LCD_WriteData((uint8_t)(BatteryIcon_Column(x,valid,level,external,charging,phase)>>(8*p)));
    }
}

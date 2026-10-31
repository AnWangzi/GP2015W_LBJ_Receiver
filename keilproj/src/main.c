/* Receive-only LBJ: foreground SPI, LCD and decoder; interrupt samples bits. */
#include <stdio.h>
#include <string.h>
#include "hc32l17x.h"
#include "gpio.h"
#include "sysctrl.h"
#include "lcd_8080.h"
#include "lcd_text.h"
#include "si446x.h"
#include "radio_task.h"
#include "pocsag.h"
#include "lbj_parse.h"
#include "lbj_state.h"
#include "buttons.h"
#include "board_io.h"
#include "app_ui.h"
#include "battery.h"
#include "battery_icon.h"
#include "ui_text.h"
#include "app_view.h"
#include "iap_entry.h"
#include "history.h"
#include "power_sleep.h"

#define RING_SIZE 2048u
static volatile uint8_t samples[RING_SIZE], sample_gap, standby = 1;
static volatile uint32_t head, tail;
volatile uint32_t g_rx_edges, g_rx_overruns, g_rx_ms, g_lbj_count;
volatile int g_rx_rssi_dbm = -134, g_rx_peak_dbm = -134;
LbjMsg g_lbj_message;
static AppUi ui;
static LbjState trains;
static uint8_t radio_state, ui_dirty, have_alarm, have_short, test_notice;
static uint32_t last_alarm, notice_since;
static uint32_t category_since;
static unsigned last_function;
static const char *radio_error;
volatile uint32_t g_debug_stage, g_fault_pc, g_fault_lr;
volatile uint32_t g_ui_max_render_ms, g_service_max_gap_ms;
static uint8_t capture_enabled,lcd_ready;
/* RAM-only evidence: no Flash writes or UART polling in the receive loop.
 * Last complete long candidate survives newer short reports until power off. */
struct LbjEvidence {
    uint32_t short_reports, details, paired, unpaired, rejected;
    uint32_t address;
    unsigned length, function;
    char bcd[POCSAG_MAX_BCD + 1];
} g_lbj_evidence;

static void line(int page, const char *text);

void fault_stop(uint32_t *frame)
{
    char buf[24];
    __disable_irq();
    g_fault_pc = frame[6]; g_fault_lr = frame[5];
    M0P_PCA->CCON = 0;
    Gpio_SetAfMode(GpioPortC, GpioPin8, GpioAf0);
    M0P_GPIO->PCBCLR = 1u << 8;
    line(0, "FAULT - SILENT");
    snprintf(buf, sizeof(buf), "PC:%08lX", (unsigned long)g_fault_pc); line(2, buf);
    snprintf(buf, sizeof(buf), "LR:%08lX", (unsigned long)g_fault_lr); line(4, buf);
    snprintf(buf, sizeof(buf), "STAGE:%lu", (unsigned long)g_debug_stage); line(6, buf);
    LCD_FlushAll();
    for (;;) __WFI();
}

__asm void HardFault_Handler(void)
{
    IMPORT fault_stop
    MOV R1, LR
    MOVS R2, #4
    TST R1, R2
    BEQ fault_msp
    MRS R0, PSP
    B fault_enter
fault_msp
    MRS R0, MSP
fault_enter
    LDR R3, =fault_stop
    BX R3
    ALIGN
}

void SysTick_Handler(void) { ++g_rx_ms; }
void PORTB_IRQHandler(void)
{
    uint32_t flags = M0P_GPIO->PB_STAT, pins, next;
    if (flags & (1u << 7)) { M0P_GPIO->PB_ICLR = ~(1u << 7); __SEV(); }
    if (!(flags & (1u << 10))) return;
    pins = M0P_GPIO->PBIN;
    M0P_GPIO->PB_ICLR = ~(1u << 10);
    if (standby) return;
    ++g_rx_edges;
    next = (head + 1) & (RING_SIZE - 1);
    if (next == tail) { ++g_rx_overruns; sample_gap = 1; }
    else { samples[head] = (uint8_t)((pins >> 2) & 1u); head = next; }
}
static void input_pin(en_gpio_pin_t pin)
{
    stc_gpio_cfg_t cfg;
    DDL_ZERO_STRUCT(cfg);
    cfg.enDir = GpioDirIn; cfg.enCtrlMode = GpioFastIO;
    Gpio_Init(GpioPortB, pin, &cfg);
}
/* Software off: DeepSleep with RCH/SysTick stopped until PB07 wakes the CPU.
 * Qualify a continuous 1.5 s green press with 1 ms ticks in ordinary Sleep;
 * an early release resets qualification and returns to DeepSleep. */
static void wait_for_power_on(void)
{
    PowerHold hold;
    uint8_t ticking = 0;
    standby = 1;
    Gpio_DisableIrq(GpioPortB, GpioPin10, GpioIrqFalling);
    Gpio_ClearIrq(GpioPortB, GpioPin10);
    Board_Standby(); SI446X_Shutdown();
    LCD_WriteCmd(0xAE); LCD_WriteCmd(0x28);
    SysTick->CTRL = 0;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
    SCB->SCR &= ~(SCB_SCR_SLEEPDEEP_Msk | SCB_SCR_SLEEPONEXIT_Msk);
    Gpio_SfIrqModeCfg(GpioSfIrqActSlpMode);
    Gpio_ClearIrq(GpioPortB, GpioPin7);
    Gpio_EnableIrq(GpioPortB, GpioPin7, GpioIrqFalling);
    Gpio_EnableIrq(GpioPortB, GpioPin7, GpioIrqRising);
    NVIC_ClearPendingIRQ(PORTB_IRQn);
    PowerHold_Init(&hold);
    for (;;) {
        int pressed = Board_GreenPressed();
        if (pressed && !ticking) {
            SysTick->VAL = 0;
            SysTick->CTRL = SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_CLKSOURCE_Msk;
            ticking = 1;
        }
        if (PowerHold_Update(&hold, pressed, g_rx_ms)) break;
        if (!pressed) {
            ticking = 0;
            PowerSleep_Stop();
        } else __WFI();
    }
    Gpio_DisableIrq(GpioPortB, GpioPin7, GpioIrqFalling);
    Gpio_DisableIrq(GpioPortB, GpioPin7, GpioIrqRising);
    Gpio_ClearIrq(GpioPortB, GpioPin7);
    standby = 0;
}
static void line(int page, const char *text)
{
    char padded[17];
    unsigned n = (unsigned)strlen(text);
    if (n > 16) n = 16;
    memset(padded, ' ', 16); memcpy(padded, text, n); padded[16] = 0;
    LCD_DrawAscii8(page, 0, padded);
}
static void zh(int page, const uint16_t *text)
{
    line(page, ""); LCD_DrawStr16(page, 0, text);
}
static void battery_icon(uint32_t now)
{
    AppView_Battery(g_battery.valid, g_battery.level, g_battery.external,
                    g_battery.charging, (now/500u)&1u);
}
static void render(uint32_t now, uint32_t rate)
{
    char buf[40];
    const uint16_t *status;
    if (test_notice && (uint32_t)(now - notice_since) < 3000u) {
        zh(0, text_test_title); zh(2, text_test_only);
        line(4, "LIGHT + 3 BEEPS"); line(6, "RX ACTIVE"); return;
    }
    test_notice = 0;
    if (ui.screen == UiSelfTest) { ui.screen=UiMenu; ui.parent=UiMain; }
    if (ui.screen == UiMenu) {
        AppView_Menu(&ui);
    } else if (ui.screen == UiHistory) {
        LbjMsg saved;
        unsigned count=History_Count();
        if (ui.history_index>=count) ui.history_index=count ? (uint8_t)(count-1u) : 0;
        History_Get(ui.history_index,&saved);
        AppView_History(&saved,ui.history_index,count);
    } else if (ui.screen == UiUpgrade) {
        zh(0,text_upgrade);
        if (IapEntry_Available()) {
            zh(2,text_upgrade_confirm); line(4,"UART 38400 8N1");
        } else {
            zh(2,text_need_boot); line(4,"SWD INSTALL IAP");
        }
        zh(6,text_upgrade_back);
    } else if (ui.screen == UiBattery) {
        ChargeStatus state = Battery_Status(g_battery.external, g_battery.charging, g_battery.valid, g_battery.level);
        zh(0, text_battery); zh(2, text_level);
        snprintf(buf, sizeof(buf), g_battery.valid ? "%u/5" : "--/5", (unsigned)g_battery.level);
        LCD_DrawAscii8(2, 40, buf);
        status = state == BatteryCharging ? text_charging : state == BatteryFull ? text_full : state == BatteryExternal ? text_external : text_battery_supply;
        zh(4, status);
        snprintf(buf, sizeof(buf), "ADC:%u E:%lu", (unsigned)g_battery.raw, (unsigned long)g_battery.errors); line(6, buf);
    } else if (ui.screen == UiDiagnostics) {
        if (!ui.diagnostic_page && radio_error) {
            zh(0,text_failed);
            snprintf(buf,sizeof(buf),"OFF:%04X ERR:%02X",(unsigned)g_radio_fail_offset,(unsigned)g_radio_cmd_error); line(2,buf);
            snprintf(buf,sizeof(buf),"CMD:%02X",(unsigned)g_radio_cmd_id); line(4,buf);
            line(6,"UP/DN:MORE"); return;
        }
        if (ui.diagnostic_page == 1) {
            snprintf(buf,sizeof(buf),"DT:%lu OK:%lu",(unsigned long)g_lbj_evidence.details,(unsigned long)g_lbj_evidence.paired); line(0,buf);
            snprintf(buf,sizeof(buf),"NO:%lu RJ:%lu",(unsigned long)g_lbj_evidence.unpaired,(unsigned long)g_lbj_evidence.rejected); line(2,buf);
            snprintf(buf,sizeof(buf),"DROP:%lu N:%u",(unsigned long)g_pocsag_stats.discarded,(unsigned)g_pocsag_stats.discard_length); line(4,buf);
            snprintf(buf,sizeof(buf),"A:%lu R:%u",(unsigned long)g_pocsag_stats.discard_address,(unsigned)g_pocsag_stats.discard_reason); line(6,buf);
            return;
        } else if (ui.diagnostic_page == 2) {
            const char *raw = g_lbj_evidence.length >= 50 ?
                g_lbj_evidence.bcd + g_lbj_evidence.length - 50 : "------------";
            snprintf(buf,sizeof(buf),"A:%lu F:%u",(unsigned long)g_lbj_evidence.address,g_lbj_evidence.function); line(0,buf);
            snprintf(buf,sizeof(buf),"N:%u P:%.4s",g_lbj_evidence.length,raw); line(2,buf);
            snprintf(buf,sizeof(buf),"RAW:%.8s",raw+4); line(4,buf);
            snprintf(buf,sizeof(buf),"SH:%lu AM:%lu",(unsigned long)g_lbj_evidence.short_reports,(unsigned long)trains.ambiguous); line(6,buf);
            return;
        }
        snprintf(buf, sizeof(buf), "RX%02X B:%lu", (unsigned)radio_state, (unsigned long)rate); line(0, buf);
        snprintf(buf, sizeof(buf), "R:%d PK:%d", g_rx_rssi_dbm, g_rx_peak_dbm); line(2, buf);
        snprintf(buf, sizeof(buf), "E:%lu O:%lu", (unsigned long)g_pocsag_stats.bad, (unsigned long)g_rx_overruns); line(4, buf);
        snprintf(buf, sizeof(buf), "S:%lu L:%lu", (unsigned long)g_pocsag_stats.syncs, (unsigned long)g_lbj_count); line(6, buf);
        return;
    } else if (radio_error || !g_radio.ready) {
        AppView_Waiting(0,radio_error!=0);
    } else {
        if (have_short) {
            AppView_Train(&g_lbj_message, now-category_since);
        } else {
            AppView_Waiting(1,0);
        }
    }
    battery_icon(now);
}
static void report(uint32_t addr, unsigned func, const char *bcd, unsigned len)
{
    LbjMsg msg;
    const LbjMsg *display;
    if (addr >= 1233999u && addr <= 1234002u && len >= 50u && len <= POCSAG_MAX_BCD) {
        memcpy(g_lbj_evidence.bcd,bcd,len); g_lbj_evidence.bcd[len]=0;
        g_lbj_evidence.address=addr; g_lbj_evidence.function=func;
        g_lbj_evidence.length=len;
    }
    if (parseLbj(bcd, (int)len, addr, (int)func, &msg)) {
        if (addr >= 1233999u && addr <= 1234002u) ++g_lbj_evidence.rejected;
        return;
    }
    if (msg.isShort) ++g_lbj_evidence.short_reports;
    display = LbjState_Update(&trains, &msg, func, g_rx_ms);
    if (msg.isDetailed) {
        ++g_lbj_evidence.details;
        if (display) ++g_lbj_evidence.paired;
        else ++g_lbj_evidence.unpaired;
    }
    if (display) {
        uint32_t held_id=ui.screen==UiHistory ? History_Id(ui.history_index) : 0;
        if (!have_short || strcmp(g_lbj_message.train,display->train) || last_function!=func)
            category_since=g_rx_ms;
        History_Upsert(display); ui.history_count=(uint8_t)History_Count();
        if (held_id) ui.history_index=(uint8_t)History_FindId(held_id);
        g_lbj_message = *display; last_function = func; have_short = 1;
    }
    ++g_lbj_count;
    Board_WakeLight(g_rx_ms); ui.light_mode = (uint8_t)Board_GetLightMode(); ui_dirty = 1;
    if (!have_alarm || (uint32_t)(g_rx_ms - last_alarm) >= 10000u) {
        have_alarm = 1; last_alarm = g_rx_ms;
        if (ui.sound_enabled) Board_Beep(2000, 180, 2, 120, g_rx_ms);
    }
}
int main(void)
{
    uint8_t bit, actions;
    unsigned decoded;
    uint16_t events;
    uint32_t last_ui, last_edges, last_service, elapsed, rate, now, render_since;
    int off_requested;
    __disable_irq();
    LCD_Init(); Board_Init(0);
    SysTick_Config(SystemCoreClock / 1000); SysTick->CTRL = 0;
    NVIC_SetPriority(SysTick_IRQn, 3); NVIC_SetPriority(PORTB_IRQn, 0); NVIC_EnableIRQ(PORTB_IRQn);
    __enable_irq();
    for (;;) {
        wait_for_power_on();
        g_debug_stage = 1;
        SysTick_Config(SystemCoreClock / 1000);
        NVIC_SetPriority(SysTick_IRQn, 3);
        Board_Init(g_rx_ms); LCD_Start(g_rx_ms); lcd_ready=0;
        AppUi_Init(&ui); LbjState_Init(&trains); POCSAG_Init(report);
        History_Init(); category_since=g_rx_ms;
        memset(&g_lbj_evidence,0,sizeof(g_lbj_evidence));
        head = tail = g_rx_edges = g_rx_overruns = g_lbj_count = 0;
        sample_gap = have_alarm = have_short = test_notice = 0;
        g_rx_rssi_dbm = g_rx_peak_dbm = -134; radio_state = 0;
        g_debug_stage = 2;
        radio_error = 0; capture_enabled=0;
        Radio_Begin(g_rx_ms);
        g_debug_stage = 3;
        Battery_Init(g_rx_ms);
        /* Quiet startup while diagnosing the reported foreground freeze. */
        Board_StopBeep(); ui.sound_enabled = 0;
        last_ui = last_service = g_rx_ms; last_edges = rate = 0;
        ui_dirty = 1; off_requested = 0;
        g_ui_max_render_ms = g_service_max_gap_ms = 0;
        while (!off_requested) {
            now = g_rx_ms;
            if (now != last_service) {
                if ((uint32_t)(now-last_service) > g_service_max_gap_ms)
                    g_service_max_gap_ms = now-last_service;
                last_service = now;
                if(LCD_PowerTask(now) && !lcd_ready) { lcd_ready=1; ui_dirty=1; }
                g_debug_stage = 11; Battery_Task(now);
                g_debug_stage = 12; events = Board_Task(now);
                if (events) {
                    if (events & 15u) Board_WakeLight(now);
                    ui.light_mode = (uint8_t)Board_GetLightMode();
                    actions = AppUi_Keys(&ui, events);
                    if (actions & UiActionLight) Board_SetLightMode((LightMode)ui.light_mode, now);
                    if ((actions & UiActionStopSound) || ((actions & UiActionSound) && !ui.sound_enabled)) Board_StopBeep();
                    if ((actions & UiActionSound) && ui.sound_enabled) Board_Beep(2000, 80, 1, 0, now);
                    if (actions & UiActionTest) {
                        Board_TestLight(now, 3000u); Board_Beep(2000, 160, 3, 160, now);
                        test_notice = 1; notice_since = now;
                    } else if (events & 0xF0u) test_notice = 0;
                    if (actions & UiActionPowerOff) off_requested = 1;
                    if (actions & UiActionUpgrade) {
                        Board_Standby(); Battery_Stop();
                        Gpio_DisableIrq(GpioPortB,GpioPin10,GpioIrqFalling);
                        SI446X_Shutdown();
                        LCD_WriteCmd(0xAE); LCD_WriteCmd(0x28);
                        IapEntry_Start();
                    }
                    ui_dirty = 1;
                }
            }
            if (off_requested) break;
            Radio_Task(now);
            radio_error=g_radio.error; radio_state=g_radio.state;
            g_rx_rssi_dbm=g_radio.rssi; g_rx_peak_dbm=g_radio.peak;
            if(g_radio.ready && !capture_enabled) {
                input_pin(GpioPin10); input_pin(GpioPin2);
                Gpio_ClearIrq(GpioPortB,GpioPin10);
                Gpio_EnableIrq(GpioPortB,GpioPin10,GpioIrqFalling);
                capture_enabled=1; ui_dirty=1;
            }
            if (sample_gap) {
                __disable_irq(); tail = head; sample_gap = 0; __enable_irq(); POCSAG_Reset();
            }
            /* Bound each decode batch so display output and keys get service
             * even while radio data arrives continuously. */
            for (decoded = 0; decoded < 32 && tail != head; ++decoded) {
                bit = samples[tail]; tail = (tail + 1) & (RING_SIZE - 1);
                g_debug_stage = 13; POCSAG_Bit(bit);
            }
            LCD_FlushStep(16);
            elapsed = now - last_ui;
            if (elapsed >= 500u || ui_dirty) {
                if (elapsed >= 500u) {
                    rate = (g_rx_edges - last_edges) * 1000 / elapsed;
                    last_edges = g_rx_edges; last_ui = now;
                }
                ui_dirty = 0; g_debug_stage = 14;
                render_since = g_rx_ms;
                render(now, rate);
                elapsed = g_rx_ms-render_since;
                if (elapsed > g_ui_max_render_ms) g_ui_max_render_ms = elapsed;
                g_debug_stage = 15;
            }
        }
        Battery_Stop();
    }
}

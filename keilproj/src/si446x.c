/* ============================================================================
 * si446x.c — Si446x 软 SPI 驱动（最小可用：复位 / POWER_UP / PART_INFO）
 *
 * 引脚全部来自原厂固件反汇编，不是猜的：
 *   SCLK = PB01      函数 0x791E  每个 bit 翻转一次
 *   MOSI = PC05      函数 0x7938  按位输出
 *   MISO = PB00      函数 0x796A  读回
 *   /CS  = PC04      函数 0x7868  包住整次传输
 *   SDN  = PA06      函数 0x4A50  高 100ms -> 低 20ms
 *
 * 原厂驱动的调用约定：
 *   0x7868(len, buf)  发命令     0x788A(len, buf)  读响应
 *   CTS 检测：发 0x44 后读 1 字节，等于 0xFF 即就绪
 * ==========================================================================*/

#include "hc32l17x.h"
#include "gpio.h"
#include "sysctrl.h"
#include "si446x.h"

#define CS_PORT    GpioPortC
#define CS_PIN     GpioPin4      /* PC04 */

#define SCLK_PORT  GpioPortB
#define SCLK_PIN   GpioPin1      /* PB01 */

#define MOSI_PORT  GpioPortC
#define MOSI_PIN   GpioPin5      /* PC05 */

#define MISO_PORT  GpioPortB
#define MISO_PIN   GpioPin0      /* PB00 */

#define SDN_PORT   GpioPortA
#define SDN_PIN    GpioPin6      /* PA06 */

/* 原厂上电函数 0x04A24 开头 CLR PA02 —— 射频电源使能（低有效） */
#define PWR_PORT   GpioPortA
#define PWR_PIN    GpioPin2      /* PA02 */

#define AUX_PORT   GpioPortA
#define AUX_PIN    GpioPin3      /* PA03 */

#define CMD_READ_CMD_BUFF  0x44

static uint32_t async_since;
static unsigned async_reply_len;

static void pin_out(en_gpio_port_t port, en_gpio_pin_t pin, int init_high)
{
    stc_gpio_cfg_t cfg;
    DDL_ZERO_STRUCT(cfg);
    cfg.bOutputVal = init_high ? TRUE : FALSE;
    cfg.enDir      = GpioDirOut;
    cfg.enDrv      = GpioDrvH;
    cfg.enPu       = GpioPuDisable;
    cfg.enPd       = GpioPdDisable;
    cfg.enOD       = GpioOdDisable;
    cfg.enCtrlMode = GpioFastIO;
    Gpio_Init(port, pin, &cfg);
}

static void pin_in_pullup(en_gpio_port_t port, en_gpio_pin_t pin)
{
    stc_gpio_cfg_t cfg;
    DDL_ZERO_STRUCT(cfg);
    cfg.bOutputVal = TRUE;
    cfg.enDir      = GpioDirIn;
    cfg.enDrv      = GpioDrvH;
    cfg.enPu       = GpioPuEnable;
    cfg.enPd       = GpioPdDisable;
    cfg.enOD       = GpioOdDisable;
    cfg.enCtrlMode = GpioFastIO;
    Gpio_Init(port, pin, &cfg);
}

void SI446X_InitPins(void)
{
    Sysctrl_SetPeripheralGate(SysctrlPeripheralGpio, TRUE);

    pin_out(SCLK_PORT, SCLK_PIN, 0);
    pin_out(MOSI_PORT, MOSI_PIN, 0);
    pin_out(CS_PORT,   CS_PIN,   1);    /* CS 空闲高 */
    pin_out(SDN_PORT,  SDN_PIN,  1);    /* SDN 高 = shutdown */
    pin_in_pullup(MISO_PORT, MISO_PIN);

    /* 严格照抄原厂 0x4A24 的动作，一根不多：
     *   (A,2) PA02 ← 低   射频电源使能（低有效）
     *   (B,1) PB01 ← 低   SCLK 空闲
     *   (C,5) PC05 ← 低   MOSI 空闲
     *   (C,4) PC04 ← 高   /CS 空闲
     * 此处仅复现0x4A24的上电段。后续原厂RX函数0x6CD4另有
     * PA01低/PA03高，休眠0x6D1C两者均高；实际电路作用待量。
     * 当前未驱动PA01/PA03，不能声称原厂完全不使用这两脚。 */
    pin_out(PWR_PORT, PWR_PIN, 0);
}

/* --------------------------- SPI 位操作 -------------------------------- */
static uint8_t spi_xfer(uint8_t b)
{
    uint8_t in = 0;
    uint8_t mask = 0x80;
    int i;

    for (i = 0; i < 8; i++) {
        Gpio_ClrIO(SCLK_PORT, SCLK_PIN);          /* SCLK 低 */

        if (b & mask) {
            Gpio_SetIO(MOSI_PORT, MOSI_PIN);
        } else {
            Gpio_ClrIO(MOSI_PORT, MOSI_PIN);
        }

        __NOP(); __NOP(); __NOP(); __NOP();
        Gpio_SetIO(SCLK_PORT, SCLK_PIN);          /* 上升沿：主机输出有效 */

        __NOP(); __NOP(); __NOP(); __NOP();
        if (Gpio_GetInputIO(MISO_PORT, MISO_PIN)) {
            in |= mask;                            /* 上升沿后采样 MISO */
        }

        Gpio_ClrIO(SCLK_PORT, SCLK_PIN);
        mask >>= 1;
    }
    return in;
}

/* --------------------------- 命令收发 ---------------------------------- */
void SI446X_Send(const uint8_t *buf, int len)
{
    int i;
    Gpio_ClrIO(CS_PORT, CS_PIN);
    for (i = 0; i < len; i++) {
        spi_xfer(buf[i]);
    }
    Gpio_SetIO(CS_PORT, CS_PIN);
}

void SI446X_Read(uint8_t *buf, int len)
{
    int i;
    Gpio_ClrIO(CS_PORT, CS_PIN);
    for (i = 0; i < len; i++) {
        buf[i] = spi_xfer(0xFF);
    }
    Gpio_SetIO(CS_PORT, CS_PIN);
}

int SI446X_MisoLevel(void)
{
    Gpio_SetIO(CS_PORT, CS_PIN);        /* CS 高 = 芯片不驱动 SDO */
    __NOP(); __NOP(); __NOP(); __NOP();
    return Gpio_GetInputIO(MISO_PORT, MISO_PIN) ? 1 : 0;
}

int SI446X_WaitCTS(void)
{
    int tries;

    for (tries = 0; tries < 200; tries++) {
        uint8_t b = 0;

        Gpio_ClrIO(CS_PORT, CS_PIN);
        spi_xfer(CMD_READ_CMD_BUFF);
        b = spi_xfer(0xFF);
        Gpio_SetIO(CS_PORT, CS_PIN);

        if (b == 0xFF) {
            return 1;
        }
    }
    return 0;
}

/* --------------------------- 复位 / 上电 ------------------------------- */
void SI446X_SetShutdown(int high)
{
    if (high) Gpio_SetIO(SDN_PORT, SDN_PIN);
    else Gpio_ClrIO(SDN_PORT, SDN_PIN);
}

void SI446X_AsyncBegin(const uint8_t *cmd,unsigned len,unsigned reply_len,uint32_t now)
{
    async_since=now; async_reply_len=reply_len;
    SI446X_Send(cmd,(int)len);
}
int SI446X_AsyncPoll(uint8_t *reply,uint32_t now)
{
    unsigned i;
    uint8_t cts;
    Gpio_ClrIO(CS_PORT,CS_PIN);
    spi_xfer(CMD_READ_CMD_BUFF); cts=spi_xfer(0xFF);
    if(cts==0xFF) {
        for(i=0;i<async_reply_len;++i) reply[i]=spi_xfer(0xFF);
        Gpio_SetIO(CS_PORT,CS_PIN);
        return 1;
    }
    Gpio_SetIO(CS_PORT,CS_PIN);
    return (uint32_t)(now-async_since)>=300u ? -1 : 0;
}

void SI446X_Shutdown(void)
{
    /* Also valid before the first POWER_UP. Use only confirmed radio pins. */
    Sysctrl_SetPeripheralGate(SysctrlPeripheralGpio, TRUE);
    pin_out(CS_PORT, CS_PIN, 1);
    pin_out(SCLK_PORT, SCLK_PIN, 0);
    pin_out(MOSI_PORT, MOSI_PIN, 0);
    pin_out(SDN_PORT, SDN_PIN, 1);
    pin_out(PWR_PORT, PWR_PIN, 1);
}

void SI446X_SetPowerPins(int pa02, int pa03)
{
    if (pa02) {
        Gpio_SetIO(PWR_PORT, PWR_PIN);
    } else {
        Gpio_ClrIO(PWR_PORT, PWR_PIN);
    }
    if (pa03) {
        Gpio_SetIO(AUX_PORT, AUX_PIN);
    } else {
        Gpio_ClrIO(AUX_PORT, AUX_PIN);
    }
}

int SI446X_PowerUpArgs(uint8_t boot, uint8_t xtal)
{
    uint8_t pu[7];
    pu[0] = 0x02;
    pu[1] = boot;
    pu[2] = xtal;
    pu[3] = 0x01;
    pu[4] = 0x8C;
    pu[5] = 0xBA;
    pu[6] = 0x80;
    SI446X_Send(pu, 7);
    return SI446X_WaitCTS();
}

int SI446X_PowerUp(void)
{
    /* CMD=02, BOOT_OPTIONS=01, XTAL_OPTIONS=01 (external clock),
     * XO_FREQ=018CBA80 (26 MHz). Matches original 0x4A7E..0x4A96.
     * External clock selection is XTAL_OPTIONS, not BOOT_OPTIONS. */
    return SI446X_PowerUpArgs(0x01, 0x01);
}

/* ---------------------- 通用命令 + 读响应 ------------------------------ */
int SI446X_CmdResp(const uint8_t *cmd, int clen, uint8_t *resp, int *rlen)
{
    *rlen = 0;
    if (!SI446X_CmdRespN(cmd, clen, resp, 16)) return 0;
    *rlen = 16;
    return 1;
}

int SI446X_CmdRespN(const uint8_t *cmd, int clen, uint8_t *resp, int len)
{
    uint8_t cts;
    int i;
    if (len < 0 || len > 16 || clen < 1 || !SI446X_WaitCTS()) return 0;

    SI446X_Send(cmd, clen);
    if (!SI446X_WaitCTS()) {
        return 0;
    }

    /* 一次 CS 窗口内：0x44 -> CTS -> 响应 */
    Gpio_ClrIO(CS_PORT, CS_PIN);
    spi_xfer(CMD_READ_CMD_BUFF);
    cts = spi_xfer(0xFF);
    if (cts != 0xFF) {
        Gpio_SetIO(CS_PORT, CS_PIN);
        return 0;
    }
    /* CTS is already consumed: resp[0] is the first actual reply field. */
    for (i = 0; i < len; i++) {
        resp[i] = spi_xfer(0xFF);
    }
    Gpio_SetIO(CS_PORT, CS_PIN);
    return 1;
}

int SI446X_GetProperty(uint8_t group, uint8_t start, uint8_t *out, int len)
{
    uint8_t cmd[4];
    if (len < 1 || len > 16) return 0;
    cmd[0] = 0x12; cmd[1] = group; cmd[2] = (uint8_t)len; cmd[3] = start;
    return SI446X_CmdRespN(cmd, 4, out, len);
}

/* --------------------------- 配置 / 收 --------------------------------- */
extern const uint8_t  g_radio_cfg[];
extern const uint16_t g_radio_cfg_len;
volatile uint16_t g_radio_fail_offset = 0xFFFF;
volatile uint8_t g_radio_cmd_error, g_radio_cmd_id;

int SI446X_ApplyRange(uint16_t from, uint16_t to)
{
    int i = (int)from;
    static const uint8_t status_cmd[2] = {0x23, 0x00};
    uint8_t status[4];
    g_radio_fail_offset = from;
    if (from > to || to > g_radio_cfg_len) return 0;
    /* CTS says command completed; it does not say command was valid. */
    if (!SI446X_CmdRespN(status_cmd, 2, status, 4)) return 0;
    g_radio_cmd_error = g_radio_cmd_id = 0;

    while (i < (int)to) {
        int n = g_radio_cfg[i];
        g_radio_fail_offset = (uint16_t)i;
        if (n == 0 || n > 16 || i + 1 + n > (int)to) {
            return 0;
        }
        SI446X_Send(&g_radio_cfg[i + 1], n);
        if (!SI446X_WaitCTS()) {
            return 0;
        }
        if (!SI446X_CmdRespN(status_cmd, 2, status, 4)) return 0;
        if ((status[0] | status[1]) & 0x08) {
            g_radio_cmd_error = status[2];
            g_radio_cmd_id = status[3];
            return 0;
        }
        i += 1 + n;
    }
    g_radio_fail_offset = 0xFFFF;
    return 1;
}

int SI446X_ApplyConfig(void)
{
    return SI446X_ApplyRange(0, g_radio_cfg_len);
}

int SI446X_StartRX(void)
{
    /* START_RX (0x32) 标准 7 个参数（AN625）：
     *   CHANNEL(1) CONDITION(1) RX_LEN[2] NEXT_STATE1..3(3)
     *
     * NEXT_STATE 必须填 0x00 = "不改变状态"（留在 RX）。
     * 之前填 0x03(READY) → 芯片一判定无效包就退回 READY，
     * 状态永远是 03，这就是"进不了接收"的真正原因。
     * CMD + 7 arguments = 8 bytes total. */
    static const uint8_t rx[8] = {
        0x32,
        0x00,              /* CHANNEL = 0 */
        0x00,              /* CONDITION：立即开始 */
        0x00, 0x00,        /* RX_LEN = 0（不限长） */
        0x00, 0x00, 0x00   /* NEXT_STATE1..3 = 0 保持 RX */
    };

    SI446X_Send(rx, 8);
    return SI446X_WaitCTS();
}

int SI446X_PartInfo(uint8_t *out, int *outlen)
{
    static const uint8_t cmd = 0x01;
    if (!SI446X_CmdRespN(&cmd, 1, out, 8)) {
        *outlen = 0;
        return 0;
    }
    *outlen = 8;  /* CHIPREV, PART[2], PBUILD, ID[2], CUSTOMER, ROMID. */
    return 1;
}

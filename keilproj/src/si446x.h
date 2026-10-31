#ifndef __SI446X_H__
#define __SI446X_H__

#include <stdint.h>

/* 引脚（来自原厂固件反汇编）：
 *   SCLK = PB01    MOSI = PC05    MISO = PB00
 *   /CS  = PC04    SDN  = PA06
 *   GPIO0 = PB10 (RX clock), GPIO1 = PB02 (RX data)
 *   GPIO2 = PA07 (user continuity measurement, 2026-10-25)
 *   GPIO3 remains unmapped. GPIO2 function/configuration is unchanged.
 */
void SI446X_InitPins(void);
void SI446X_SetShutdown(int high);
void SI446X_AsyncBegin(const uint8_t *cmd, unsigned len, unsigned reply_len, uint32_t now);
int SI446X_AsyncPoll(uint8_t *reply, uint32_t now); /* 0 pending, 1 done, -1 timeout */
void SI446X_Shutdown(void);
int  SI446X_WaitCTS(void);                   /* 1 = 就绪，0 = 超时 */
void SI446X_Send(const uint8_t *buf, int len);
void SI446X_Read(uint8_t *buf, int len);
int  SI446X_PowerUp(void);                   /* 1 = OK */
int  SI446X_PowerUpArgs(uint8_t boot, uint8_t xtal);
void SI446X_SetPowerPins(int pa02, int pa03);
int  SI446X_PartInfo(uint8_t *out, int *outlen);

/* 通用：发命令（cmd[0..clen-1]），然后把 16 字节响应读进 resp */
int  SI446X_CmdResp(const uint8_t *cmd, int clen, uint8_t *resp, int *rlen);
int  SI446X_CmdRespN(const uint8_t *cmd, int clen, uint8_t *resp, int len);
int  SI446X_GetProperty(uint8_t group, uint8_t start, uint8_t *out, int len);
extern volatile uint16_t g_radio_fail_offset;
extern volatile uint8_t g_radio_cmd_error, g_radio_cmd_id;
int  SI446X_ApplyConfig(void);     /* 灌入 g_radio_cfg */
int  SI446X_ApplyRange(uint16_t from, uint16_t to);   /* 只灌一段，用于分段诊断 */
int  SI446X_StartRX(void);

/* 诊断：CS 高时 MISO 的原始电平（1 = 上拉到高，0 = 被拉低） */
int  SI446X_MisoLevel(void);

#endif

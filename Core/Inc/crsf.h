#ifndef CRSF_H
#define CRSF_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void CRSF_Init(void);

/* 设置 CRSF 通道值。value 为 11-bit 原始值（0..2047），超限自动截断。 */
bool CRSF_SetChannel(uint8_t channel, uint16_t value);

/* 按当前通道值组帧并通过 UART4 发送。当前实现为阻塞发送。 */
bool CRSF_Transmit(void);

uint8_t CRSF_ComputeCrc8(const uint8_t *data, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* CRSF_H */

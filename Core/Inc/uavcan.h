#ifndef UAVCAN_H
#define UAVCAN_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UAVCAN_PWM_CHANNEL_COUNT 4U

void Uavcan_Init(void);

/* 周期性调用：处理 libcanard 接收队列、超时和状态机。 */
void Uavcan_Update(void);

/* 发布 4 路 PWM（单位 us）到 CAN/UAVCAN(DroneCAN)。 */
bool Uavcan_PublishPwm(const uint16_t pwm_us[UAVCAN_PWM_CHANNEL_COUNT]);

/* 底层 CAN 发送接口。libcanard 集成完成后应通过它发送 canard 帧。 */
bool Uavcan_TransmitRaw(uint32_t ext_id, const uint8_t *data, uint8_t dlc);

#ifdef __cplusplus
}
#endif

#endif /* UAVCAN_H */

/**
  ******************************************************************************
  * @file    uavcan.h
  * @brief   DroneCAN（UAVCAN v0）节点：把 4 路 PWM 以标准消息发到 CAN1。
  *
  *          节点行为：
  *            - 上电固定节点 ID（APP_UAVCAN_NODE_ID），1 Hz 广播 NodeStatus；
  *            - 周期广播 uavcan.equipment.esc.RawCommand（默认 20 ms）；
  *            - 响应 uavcan.protocol.GetNodeInfo 请求（总线工具/接收端可读节点信息）。
  ******************************************************************************
  */
#ifndef UAVCAN_H
#define UAVCAN_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UAVCAN_PWM_CHANNEL_COUNT 4U

/**
  * @brief 节点运行状态，供调试日志使用
  */
typedef struct
{
    bool     ready;          /**< CAN 与 canard 是否初始化成功 */
    uint32_t tx_frames;      /**< 已发出的 CAN 帧数 */
    uint32_t rx_frames;      /**< 已接收的 CAN 帧数 */
    uint8_t  pwm_valid_mask; /**< 最近一次发布时有效的 PWM 通道位图 */
} UavcanStats;

/** 初始化 CAN1 过滤器、启动控制器并初始化 libcanard 实例（失败不致命） */
void Uavcan_Init(void);

/** 周期调用（主循环）：收帧、发帧、心跳与超时清理 */
void Uavcan_Update(void);

/**
  * @brief 发布 uavcan.equipment.esc.RawCommand
  * @param pwm_us     4 路脉宽（µs）；无效通道的数值会被忽略
  * @param valid_mask 位 0..3 对应 CH1..CH4，1 表示该路信号有效
  * @retval true 已入队（不代表已上总线）
  */
bool Uavcan_PublishPwm(const uint16_t pwm_us[UAVCAN_PWM_CHANNEL_COUNT], uint8_t valid_mask);

/** 读取运行统计（可为 NULL） */
void Uavcan_GetStats(UavcanStats *out);

#ifdef __cplusplus
}
#endif

#endif /* UAVCAN_H */

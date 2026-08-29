#include "uavcan.h"
#include "app_config.h"
#include "main.h"

extern CAN_HandleTypeDef hcan1;

void Uavcan_Init(void)
{
    if (HAL_CAN_Start(&hcan1) != HAL_OK)
    {
        Error_Handler();
    }
}

void Uavcan_Update(void)
{
    /* TODO：接入 libcanard 后，在此调用 canardRxAccept()、
     * 维护传输超时、发送心跳和节点状态等。 */
}

bool Uavcan_TransmitRaw(uint32_t ext_id, const uint8_t *data, uint8_t dlc)
{
    CAN_TxHeaderTypeDef tx_header = {0};
    uint32_t mailbox = 0U;

    if ((data == 0) || (dlc > 8U))
    {
        return false;
    }

    tx_header.ExtId = ext_id;
    tx_header.IDE   = CAN_ID_EXT;
    tx_header.RTR   = CAN_RTR_DATA;
    tx_header.DLC   = dlc;
    tx_header.TransmitGlobalTime = DISABLE;

    return (HAL_CAN_AddTxMessage(&hcan1, &tx_header, (uint8_t *)data, &mailbox) == HAL_OK);
}

bool Uavcan_PublishPwm(const uint16_t pwm_us[UAVCAN_PWM_CHANNEL_COUNT])
{
#if APP_UAVCAN_USE_RAW_FALLBACK
    /* 临时裸 CAN 调试帧，不满足 UAVCAN/DroneCAN 规范。
     * 仅在 APP_UAVCAN_USE_RAW_FALLBACK=1 时启用。 */
    uint8_t data[8] = {0};
    for (uint8_t i = 0U; i < UAVCAN_PWM_CHANNEL_COUNT; i++)
    {
        data[i * 2U]      = (uint8_t)(pwm_us[i] & 0xFFU);
        data[i * 2U + 1U] = (uint8_t)(pwm_us[i] >> 8U);
    }
    return Uavcan_TransmitRaw(APP_UAVCAN_RAW_FALLBACK_CAN_ID, data, sizeof(data));
#else
    (void)pwm_us;

    /* TODO：接入 libcanard 后，在这里调用正式的 UAVCAN/DroneCAN 发布函数。
     * 建议流程：
     *   1. 根据 DSDL 生成 4 路 PWM 消息类型与 DataType ID；
     *   2. 填充 transfer payload；
     *   3. 调用 canardTxPush()，然后在 Uavcan_Update() 中取出并发送。
     * 当前未接入 libcanard，因此返回 false，不会发送非协议数据。 */
    return false;
#endif
}

/**
  ******************************************************************************
  * @file    uavcan.c
  * @brief   DroneCAN（UAVCAN v0）节点实现：CAN1 收发 + libcanard 传输层。
  *
  *          帧格式由 libcanard 负责（29 位扩展 ID、多帧拆分、传输 CRC-16/CCITT、
  *          tail byte），这里只做三件事：
  *            1. 把 canard 的 TX 队列搬进 CAN 发送邮箱；
  *            2. 把 CAN 接收 FIFO 里的帧喂给 canardHandleRxFrame()；
  *            3. 组织标准消息（见 dronecan_msgs.c）。
  ******************************************************************************
  */
#include "uavcan.h"
#include "dronecan_msgs.h"
#include "app_config.h"
#include "canard.h"
#include "main.h"

#include <string.h>

extern CAN_HandleTypeDef hcan1;

/* STM32F4 的 96 位唯一 ID */
#define UAVCAN_UID_BASE_ADDR 0x1FFF7A10UL

static CanardInstance s_canard;
static uint8_t        s_canard_pool[APP_UAVCAN_MEM_POOL_BYTES];
static UavcanStats    s_stats;
static uint8_t        s_unique_id[DRONECAN_UNIQUE_ID_LEN];

static uint8_t  s_tid_raw_command;
static uint8_t  s_tid_node_status;
static uint8_t  s_pwm_valid_mask;
static uint32_t s_node_start_ms;
static uint32_t s_last_node_status_ms;
static uint32_t s_last_cleanup_ms;
static uint32_t s_last_rx_poll_ms;

static uint64_t Uavcan_NowUsec(void)
{
    return (uint64_t)HAL_GetTick() * 1000ULL;
}

static void Uavcan_ReadUniqueId(void)
{
    const volatile uint32_t *uid = (const volatile uint32_t *)UAVCAN_UID_BASE_ADDR;

    memset(s_unique_id, 0, sizeof(s_unique_id));
    for (uint8_t word = 0U; word < 3U; word++)
    {
        uint32_t value = uid[word];

        s_unique_id[word * 4U + 0U] = (uint8_t)(value & 0xFFU);
        s_unique_id[word * 4U + 1U] = (uint8_t)((value >> 8U) & 0xFFU);
        s_unique_id[word * 4U + 2U] = (uint8_t)((value >> 16U) & 0xFFU);
        s_unique_id[word * 4U + 3U] = (uint8_t)((value >> 24U) & 0xFFU);
    }
}

/* 无信号持续 2 s 后报 WARNING，便于总线工具/接收端发现传感器异常 */
static uint8_t Uavcan_Health(uint32_t now_ms)
{
    if ((s_pwm_valid_mask == 0U) && ((now_ms - s_node_start_ms) > 2000U))
    {
        return DRONECAN_HEALTH_WARNING;
    }
    return DRONECAN_HEALTH_OK;
}

static void Uavcan_FillNodeInfo(DronecanNodeInfo *info, uint32_t now_ms)
{
    info->uptime_sec     = now_ms / 1000U;
    info->health         = Uavcan_Health(now_ms);
    info->mode           = DRONECAN_MODE_OPERATIONAL;
    info->vendor_status  = (uint16_t)s_pwm_valid_mask;
    info->software_major = APP_UAVCAN_SW_VERSION_MAJOR;
    info->software_minor = APP_UAVCAN_SW_VERSION_MINOR;
    info->hardware_major = APP_UAVCAN_HW_VERSION_MAJOR;
    info->hardware_minor = APP_UAVCAN_HW_VERSION_MINOR;
    info->unique_id      = s_unique_id;
    info->name           = APP_UAVCAN_NODE_NAME;
}

/* -------------------------------------------------------------------------- */
/* CAN1 发送 / 接收                                                            */
/* -------------------------------------------------------------------------- */

static bool Uavcan_SendCanFrame(const CanardCANFrame *frame)
{
    CAN_TxHeaderTypeDef header = {0};
    uint32_t mailbox = 0U;

    /* libcanard 的 frame->id 带 CANARD_CAN_FRAME_EFF(bit31) 标志位，
     * HAL 只接受 29 位扩展 ID，必须先把标志位清掉。 */
    header.ExtId = frame->id & CANARD_CAN_EXT_ID_MASK;
    header.IDE   = CAN_ID_EXT;
    header.RTR   = CAN_RTR_DATA;
    header.DLC   = frame->data_len;
    header.TransmitGlobalTime = DISABLE;

    if (HAL_CAN_AddTxMessage(&hcan1, &header, (uint8_t *)frame->data, &mailbox) != HAL_OK)
    {
        return false;
    }

    s_stats.tx_frames++;
    return true;
}

static void Uavcan_PumpTx(void)
{
    for (;;)
    {
        CanardCANFrame *frame = canardPeekTxQueue(&s_canard);

        if (frame == NULL)
        {
            return;
        }

        /* 邮箱全忙时就停在这里，下一轮主循环继续；不能丢帧，否则对端会收到半截传输 */
        if (!Uavcan_SendCanFrame(frame))
        {
            return;
        }

        canardPopTxQueue(&s_canard);
    }
}

static void Uavcan_PollRx(void)
{
    CAN_RxHeaderTypeDef header;
    uint8_t data[CANARD_CAN_FRAME_MAX_DATA_LEN];

    while (HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &header, data) == HAL_OK)
    {
        CanardCANFrame frame;

        if (header.IDE != CAN_ID_EXT)
        {
            continue;   /* DroneCAN 只用 29 位扩展帧 */
        }

        /* canardHandleRxFrame() 要求扩展帧标志位（bit31）已置位 */
        frame.id       = header.ExtId | CANARD_CAN_FRAME_EFF;
        frame.iface_id = 0U;
        frame.data_len = (uint8_t)header.DLC;
        memcpy(frame.data, data, frame.data_len);

        s_stats.rx_frames++;
        (void)canardHandleRxFrame(&s_canard, &frame, Uavcan_NowUsec());
    }
}

/* -------------------------------------------------------------------------- */
/* libcanard 回调                                                              */
/* -------------------------------------------------------------------------- */

static bool Uavcan_ShouldAccept(const CanardInstance *ins, uint64_t *out_signature,
                                uint16_t data_type_id, CanardTransferType transfer_type,
                                uint8_t source_node_id)
{
    (void)ins;
    (void)source_node_id;

    /* 只接服务端需要处理的东西：GetNodeInfo 请求。
     * 注意签名必须与 DSDL 完全一致，否则传输 CRC 校验不过。 */
    if ((transfer_type == CanardTransferTypeRequest) &&
        (data_type_id == DRONECAN_DTID_GET_NODE_INFO))
    {
        *out_signature = DRONECAN_SIGNATURE_GET_NODE_INFO;
        return true;
    }

    return false;
}

static void Uavcan_SendNodeInfoResponse(const CanardRxTransfer *transfer)
{
    uint8_t          payload[80];
    uint16_t         length;
    DronecanNodeInfo info;

    Uavcan_FillNodeInfo(&info, HAL_GetTick());

    length = DronecanMsgs_EncodeGetNodeInfoResponse(payload, sizeof(payload), &info);
    if (length == 0U)
    {
        return;
    }

    /* 响应必须沿用请求的 transfer ID */
    (void)canardRequestOrRespond(&s_canard, transfer->source_node_id,
                                 DRONECAN_SIGNATURE_GET_NODE_INFO,
                                 DRONECAN_DTID_GET_NODE_INFO,
                                 (uint8_t *)&transfer->transfer_id,
                                 CANARD_TRANSFER_PRIORITY_LOW,
                                 CanardResponse, payload, length);
}

static void Uavcan_OnTransferReceived(CanardInstance *ins, CanardRxTransfer *transfer)
{
    if ((transfer->transfer_type == CanardTransferTypeRequest) &&
        (transfer->data_type_id == DRONECAN_DTID_GET_NODE_INFO))
    {
        /* 请求本身是空载荷，先释放接收缓冲，给响应腾出内存池 */
        canardReleaseRxTransferPayload(ins, transfer);
        Uavcan_SendNodeInfoResponse(transfer);
        Uavcan_PumpTx();
    }
}

/* -------------------------------------------------------------------------- */
/* 周期任务                                                                    */
/* -------------------------------------------------------------------------- */

static void Uavcan_SendNodeStatus(uint32_t now_ms)
{
    uint8_t  payload[7];
    uint16_t length;

    length = DronecanMsgs_EncodeNodeStatus(payload, sizeof(payload), now_ms / 1000U,
                                           Uavcan_Health(now_ms), DRONECAN_MODE_OPERATIONAL,
                                           (uint16_t)s_pwm_valid_mask);
    if (length == 0U)
    {
        return;
    }

    (void)canardBroadcast(&s_canard, DRONECAN_SIGNATURE_NODE_STATUS, DRONECAN_DTID_NODE_STATUS,
                          &s_tid_node_status, CANARD_TRANSFER_PRIORITY_LOW, payload, length);
}

/* 脉宽 -> ESC 归一化命令：1500 µs -> 0，500 µs -> -8191，2500 µs -> +8191 */
static int16_t Uavcan_PwmToRawCommand(uint16_t pwm_us)
{
    int32_t centered = (int32_t)pwm_us - (int32_t)APP_UAVCAN_CMD_CENTER_US;
    int32_t scaled   = (centered * (int32_t)DRONECAN_RAW_COMMAND_MAX) / (int32_t)APP_UAVCAN_CMD_HALF_SPAN_US;

    if (scaled < (int32_t)DRONECAN_RAW_COMMAND_MIN)
    {
        scaled = (int32_t)DRONECAN_RAW_COMMAND_MIN;
    }
    else if (scaled > (int32_t)DRONECAN_RAW_COMMAND_MAX)
    {
        scaled = (int32_t)DRONECAN_RAW_COMMAND_MAX;
    }

    return (int16_t)scaled;
}

/* -------------------------------------------------------------------------- */
/* 对外接口                                                                    */
/* -------------------------------------------------------------------------- */

void Uavcan_Init(void)
{
    CAN_FilterTypeDef filter = {0};

    s_stats.ready          = false;
    s_stats.tx_frames      = 0U;
    s_stats.rx_frames      = 0U;
    s_stats.pwm_valid_mask = 0U;
    s_pwm_valid_mask       = 0U;

    Uavcan_ReadUniqueId();

    canardInit(&s_canard, s_canard_pool, sizeof(s_canard_pool),
               Uavcan_OnTransferReceived, Uavcan_ShouldAccept, NULL);
    canardSetLocalNodeID(&s_canard, APP_UAVCAN_NODE_ID);

    /* 接收所有扩展帧：总线上只有本节点与接收板，流量很小，主循环轮询足够 */
    filter.FilterBank           = 0U;
    filter.FilterMode           = CAN_FILTERMODE_IDMASK;
    filter.FilterScale          = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh         = 0U;
    filter.FilterIdLow          = 0U;
    filter.FilterMaskIdHigh     = 0U;
    filter.FilterMaskIdLow      = 0U;
    filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter.FilterActivation     = ENABLE;
    filter.SlaveStartFilterBank = 14U;

    if (HAL_CAN_ConfigFilter(&hcan1, &filter) != HAL_OK)
    {
        return;   /* 过滤器配置失败：不启动 CAN，避免收到无法解析的帧 */
    }

    if (HAL_CAN_Start(&hcan1) != HAL_OK)
    {
        return;
    }

    s_node_start_ms       = HAL_GetTick();
    s_last_node_status_ms = s_node_start_ms;
    s_last_cleanup_ms     = s_node_start_ms;
    s_last_rx_poll_ms     = s_node_start_ms;
    s_stats.ready         = true;
}

void Uavcan_Update(void)
{
    uint32_t now_ms;

    if (!s_stats.ready)
    {
        return;
    }

    now_ms = HAL_GetTick();

    /* 主循环空转频率很高，没必要每个循环都去读 CAN 接收 FIFO；
     * 2 ms 轮询一次（500 Hz）足以覆盖 GetNodeInfo 请求，也不会让 3 级 FIFO 溢出。 */
    if ((now_ms - s_last_rx_poll_ms) >= APP_UAVCAN_RX_POLL_PERIOD_MS)
    {
        s_last_rx_poll_ms = now_ms;
        Uavcan_PollRx();
    }

    if ((now_ms - s_last_node_status_ms) >= APP_UAVCAN_NODE_STATUS_PERIOD_MS)
    {
        s_last_node_status_ms = now_ms;
        Uavcan_SendNodeStatus(now_ms);
    }

    if ((now_ms - s_last_cleanup_ms) >= APP_UAVCAN_NODE_STATUS_PERIOD_MS)
    {
        s_last_cleanup_ms = now_ms;
        canardCleanupStaleTransfers(&s_canard, Uavcan_NowUsec());
    }

    Uavcan_PumpTx();
}

bool Uavcan_PublishPwm(const uint16_t pwm_us[UAVCAN_PWM_CHANNEL_COUNT], uint8_t valid_mask)
{
    uint8_t  payload[8];
    int16_t  commands[UAVCAN_PWM_CHANNEL_COUNT];
    uint16_t length;
    int16_t  queued;

    if (!s_stats.ready || (pwm_us == NULL))
    {
        return false;
    }

    s_pwm_valid_mask       = (uint8_t)(valid_mask & 0x0FU);
    s_stats.pwm_valid_mask = s_pwm_valid_mask;

    for (uint8_t i = 0U; i < UAVCAN_PWM_CHANNEL_COUNT; i++)
    {
        /* 无效通道（断线/超时）按 DroneCAN 约定填 0：0 = 不输出 */
        commands[i] = ((valid_mask & (1U << i)) != 0U) ? Uavcan_PwmToRawCommand(pwm_us[i]) : 0;
    }

    length = DronecanMsgs_EncodeRawCommand(payload, sizeof(payload), commands,
                                           (uint8_t)UAVCAN_PWM_CHANNEL_COUNT);
    if (length == 0U)
    {
        return false;
    }

    queued = canardBroadcast(&s_canard, DRONECAN_SIGNATURE_ESC_RAW_COMMAND,
                             DRONECAN_DTID_ESC_RAW_COMMAND, &s_tid_raw_command,
                             CANARD_TRANSFER_PRIORITY_MEDIUM, payload, length);
    if (queued < 0)
    {
        return false;   /* 内存池耗尽：本轮丢弃，下一次再发 */
    }

    Uavcan_PumpTx();
    return true;
}

void Uavcan_GetStats(UavcanStats *out)
{
    if (out != NULL)
    {
        *out = s_stats;
    }
}

/**
  ******************************************************************************
  * @file    test_transfer.c
  * @brief   Host 侧传输层自测：用 libcanard 编码 RawCommand（2 帧），
  *          再喂给另一个 libcanard 实例解码，验证签名 / DTID / 多帧拆分。
  *
  *          这一步不碰 CAN 硬件，但覆盖了 NodeStatus/RawCommand 的完整
  *          打包路径（含传输 CRC 以数据签名作种子这条 DroneCAN 规则）。
  ******************************************************************************
  */
#include "dronecan_msgs.h"
#include "canard.h"

#include <stdio.h>
#include <string.h>

/* 与 Core/Inc/app_config.h 保持一致（该头文件依赖 STM32 HAL，不能在 PC 上包含） */
#define TEST_NODE_ID   42U
#define TEST_POOL_SIZE 2048U

static uint8_t s_tx_pool[TEST_POOL_SIZE];
static uint8_t s_rx_pool[TEST_POOL_SIZE];

static CanardInstance s_tx;
static CanardInstance s_rx;

static int16_t s_decoded[4];
static uint8_t s_decoded_count;
static uint16_t s_decoded_len;
static uint8_t s_rx_payload[64];

static bool tx_should_accept(const CanardInstance *ins, uint64_t *signature, uint16_t dtid,
                             CanardTransferType transfer_type, uint8_t source_node_id)
{
    (void)ins;
    (void)transfer_type;
    (void)source_node_id;

    if (dtid == DRONECAN_DTID_ESC_RAW_COMMAND)
    {
        *signature = DRONECAN_SIGNATURE_ESC_RAW_COMMAND;
        return true;
    }
    return false;
}

static void tx_on_reception(CanardInstance *ins, CanardRxTransfer *transfer)
{
    (void)ins;
    (void)transfer;
}

static void rx_on_reception(CanardInstance *ins, CanardRxTransfer *transfer)
{
    (void)ins;

    if ((transfer->data_type_id != DRONECAN_DTID_ESC_RAW_COMMAND) ||
        (transfer->transfer_type != CanardTransferTypeBroadcast))
    {
        return;
    }

    uint8_t count = 0U;

    (void)canardDecodeScalar(transfer, 0U, 5U, false, &count);
    s_decoded_count = count;
    s_decoded_len   = transfer->payload_len;

    memcpy(s_rx_payload, transfer->payload_head,
           (transfer->payload_len < sizeof(s_rx_payload)) ? transfer->payload_len : sizeof(s_rx_payload));

    for (uint8_t i = 0U; (i < count) && (i < 4U); i++)
    {
        int16_t value = 0;

        (void)canardDecodeScalar(transfer, 5U + (14U * i), 14U, true, &value);
        s_decoded[i] = value;
    }
}

int main(void)
{
    const int16_t commands[4] = { -8192, -1000, 2500, 8191 };
    uint8_t  payload[8];
    uint16_t payload_len;
    uint8_t  tid = 0U;
    int      failures = 0;

    canardInit(&s_tx, s_tx_pool, sizeof(s_tx_pool), tx_on_reception, tx_should_accept, NULL);
    canardSetLocalNodeID(&s_tx, TEST_NODE_ID);

    canardInit(&s_rx, s_rx_pool, sizeof(s_rx_pool), rx_on_reception, tx_should_accept, NULL);
    canardSetLocalNodeID(&s_rx, 1U);

    payload_len = DronecanMsgs_EncodeRawCommand(payload, sizeof(payload), commands, 4U);
    if (payload_len == 0U)
    {
        printf("FAIL encode\n");
        return 1;
    }

    if (canardBroadcast(&s_tx, DRONECAN_SIGNATURE_ESC_RAW_COMMAND, DRONECAN_DTID_ESC_RAW_COMMAND,
                        &tid, CANARD_TRANSFER_PRIORITY_MEDIUM, payload, payload_len) <= 0)
    {
        printf("FAIL broadcast\n");
        return 1;
    }

    uint64_t now_us = 1000U;
    int      frames = 0;

    for (;;)
    {
        CanardCANFrame *frame = canardPeekTxQueue(&s_tx);

        if (frame == NULL)
        {
            break;
        }

        printf("FRAME id=0x%08lX dlc=%u\n", (unsigned long)frame->id, (unsigned)frame->data_len);
        if (canardHandleRxFrame(&s_rx, frame, now_us) < 0)
        {
            printf("FAIL handle frame\n");
            failures++;
        }
        now_us += 200U;
        frames++;
        canardPopTxQueue(&s_tx);
    }

    printf("FRAMES %d\n", frames);
    printf("DECODED count=%u len=%u values=%d,%d,%d,%d\n", (unsigned)s_decoded_count,
           (unsigned)s_decoded_len, (int)s_decoded[0], (int)s_decoded[1],
           (int)s_decoded[2], (int)s_decoded[3]);

    if (frames != 2)
    {
        printf("FAIL expected 2 frames (8 byte payload + CRC + tail)\n");
        failures++;
    }
    if (s_decoded_count != 4U ||
        s_decoded[0] != commands[0] || s_decoded[1] != commands[1] ||
        s_decoded[2] != commands[2] || s_decoded[3] != commands[3])
    {
        printf("FAIL round-trip mismatch\n");
        failures++;
    }
    if (s_decoded_len != payload_len)
    {
        printf("FAIL payload length %u != %u\n", (unsigned)s_decoded_len, (unsigned)payload_len);
        failures++;
    }

    /* DroneCAN 29 位 ID 布局：priority(bits 28..24) | dtid(bits 23..8) | SOT(bit7) | src(bits 6..0)，
     * libcanard 的 frame->id 还额外带 CANARD_CAN_FRAME_EFF(bit31) 标志。 */
    const uint32_t expected_id = CANARD_CAN_FRAME_EFF |
                                 ((uint32_t)CANARD_TRANSFER_PRIORITY_MEDIUM << 24) |
                                 ((uint32_t)DRONECAN_DTID_ESC_RAW_COMMAND << 8) |
                                 (uint32_t)TEST_NODE_ID;
    printf("EXPECT_ID 0x%08lX\n", (unsigned long)expected_id);
    printf("EXPECT_HAL_ID 0x%08lX\n", (unsigned long)(expected_id & CANARD_CAN_EXT_ID_MASK));

    printf("%s\n", failures ? "TRANSFER TEST FAILED" : "TRANSFER TEST OK");
    return failures ? 1 : 0;
}

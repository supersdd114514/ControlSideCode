/**
  ******************************************************************************
  * @file    dronecan_msgs.c
  * @brief   DroneCAN 标准消息的字节级序列化（UAVCAN v0 DSDL 规则）。
  *
  *          DSDL 序列化要点（本文件只涉及定长/变长整数与数组）：
  *            - 所有字段按 DSDL 声明顺序紧密位打包，低位在前（little endian 位序）；
  *            - 变长数组前缀一个"能表示 容量+1 的最小位宽"的无符号长度字段，
  *              例如 [<=20] -> 5 bit，[<=80] -> 7 bit，[<=255] -> 8 bit；
  *            - 整个结构补零到字节边界，载荷长度 = ceil(总位数 / 8)。
  *
  *          这些数值在 tests/host/ 下用 Python 参考实现逐字节比对过。
  ******************************************************************************
  */
#include "dronecan_msgs.h"
#include "canard.h"

#include <string.h>

/* 固定字段的位偏移（DDL 声明顺序求和得到） */
#define NS_OFF_UPTIME        0U
#define NS_OFF_HEALTH       32U
#define NS_OFF_MODE         34U
#define NS_OFF_SUB_MODE     37U
#define NS_OFF_VENDOR       40U

#define SW_OFF_MAJOR        56U
#define SW_OFF_MINOR        64U
#define SW_OFF_FLAGS        72U
#define SW_OFF_VCS_COMMIT   80U
#define SW_OFF_IMAGE_CRC   112U

#define HW_OFF_MAJOR       176U
#define HW_OFF_MINOR       184U
#define HW_OFF_UNIQUE_ID   192U
#define HW_OFF_COA_LEN     320U

#define NAME_OFF_LEN       328U
#define NAME_OFF_DATA      335U   /* 328 + 7（[<=80] 的长度字段位宽） */

#define BIT_LEN(x)         ((uint32_t)(x))

/* 把值写入 buffer 的指定位；返回 false 表示会越界 */
static bool dronecan_put(uint8_t *buffer, uint16_t capacity, uint32_t bit_offset,
                         uint8_t bit_length, const void *value)
{
    uint32_t last_byte = (bit_offset + BIT_LEN(bit_length) + 7U) / 8U;

    if (last_byte > (uint32_t)capacity)
    {
        return false;
    }
    canardEncodeScalar(buffer, bit_offset, bit_length, value);
    return true;
}

static void dronecan_put_u8(uint8_t *buffer, uint16_t capacity, uint32_t bit_offset,
                            uint8_t bit_length, uint8_t value)
{
    (void)dronecan_put(buffer, capacity, bit_offset, bit_length, &value);
}

/* 写入 NodeStatus 的 56 bit（供 NodeStatus 本体和 GetNodeInfo.Response 复用） */
static void dronecan_put_node_status(uint8_t *buffer, uint16_t capacity, uint32_t base_bit,
                                     uint32_t uptime_sec, uint8_t health, uint8_t mode,
                                     uint8_t sub_mode, uint16_t vendor_status)
{
    (void)dronecan_put(buffer, capacity, base_bit + NS_OFF_UPTIME, 32U, &uptime_sec);
    dronecan_put_u8(buffer, capacity, base_bit + NS_OFF_HEALTH, 2U, (uint8_t)(health & 0x03U));
    dronecan_put_u8(buffer, capacity, base_bit + NS_OFF_MODE, 3U, (uint8_t)(mode & 0x07U));
    dronecan_put_u8(buffer, capacity, base_bit + NS_OFF_SUB_MODE, 3U, (uint8_t)(sub_mode & 0x07U));
    (void)dronecan_put(buffer, capacity, base_bit + NS_OFF_VENDOR, 16U, &vendor_status);
}

uint16_t DronecanMsgs_EncodeNodeStatus(uint8_t *buffer, uint16_t capacity,
                                       uint32_t uptime_sec, uint8_t health, uint8_t mode,
                                       uint16_t vendor_status)
{
    const uint16_t length = 7U;   /* 56 bit */

    if ((buffer == NULL) || (capacity < length))
    {
        return 0U;
    }

    memset(buffer, 0, length);
    dronecan_put_node_status(buffer, capacity, 0U, uptime_sec, health, mode, 0U, vendor_status);

    return length;
}

uint16_t DronecanMsgs_EncodeRawCommand(uint8_t *buffer, uint16_t capacity,
                                       const int16_t *cmd, uint8_t count)
{
    uint16_t length;
    uint32_t bit_offset = 5U;   /* [<=20] 的长度字段占 5 bit */
    uint8_t  length_field;

    if ((buffer == NULL) || (cmd == NULL) ||
        (count == 0U) || (count > DRONECAN_RAW_COMMAND_CAPACITY))
    {
        return 0U;
    }

    length = (uint16_t)((5U + (14U * (uint32_t)count) + 7U) / 8U);
    if (capacity < length)
    {
        return 0U;
    }

    memset(buffer, 0, length);

    length_field = count;
    dronecan_put_u8(buffer, capacity, 0U, 5U, length_field);

    for (uint8_t i = 0U; i < count; i++)
    {
        int16_t value = cmd[i];

        if (value < DRONECAN_RAW_COMMAND_MIN)
        {
            value = DRONECAN_RAW_COMMAND_MIN;
        }
        else if (value > DRONECAN_RAW_COMMAND_MAX)
        {
            value = DRONECAN_RAW_COMMAND_MAX;
        }

        (void)dronecan_put(buffer, capacity, bit_offset, 14U, &value);
        bit_offset += 14U;
    }

    return length;
}

uint16_t DronecanMsgs_EncodeGetNodeInfoResponse(uint8_t *buffer, uint16_t capacity,
                                                const DronecanNodeInfo *info)
{
    uint16_t length;
    uint32_t name_length = 0U;
    uint32_t total_bits;
    uint8_t  flags = 0U;
    uint32_t vcs_commit = 0U;
    uint64_t image_crc = 0U;
    uint8_t  coa_length = 0U;

    if ((buffer == NULL) || (info == NULL) || (info->unique_id == NULL) || (info->name == NULL))
    {
        return 0U;
    }

    name_length = (uint32_t)strlen(info->name);
    if ((name_length == 0U) || (name_length > DRONECAN_NODE_NAME_MAX))
    {
        return 0U;
    }

    /* status(56) + software_version(120) + hardware_version(152 + COA) + name(7 + 8N) */
    total_bits = 328U + 7U + (8U * name_length);
    length = (uint16_t)((total_bits + 7U) / 8U);
    if (capacity < length)
    {
        return 0U;
    }

    memset(buffer, 0, length);

    /* --- status: uavcan.protocol.NodeStatus --- */
    dronecan_put_node_status(buffer, capacity, 0U, info->uptime_sec, info->health, info->mode, 0U,
                             info->vendor_status);

    /* --- software_version: uavcan.protocol.SoftwareVersion --- */
    dronecan_put_u8(buffer, capacity, SW_OFF_MAJOR, 8U, info->software_major);
    dronecan_put_u8(buffer, capacity, SW_OFF_MINOR, 8U, info->software_minor);
    dronecan_put_u8(buffer, capacity, SW_OFF_FLAGS, 8U, flags);        /* 无可选字段 */
    (void)dronecan_put(buffer, capacity, SW_OFF_VCS_COMMIT, 32U, &vcs_commit);
    (void)dronecan_put(buffer, capacity, SW_OFF_IMAGE_CRC, 64U, &image_crc);

    /* --- hardware_version: uavcan.protocol.HardwareVersion --- */
    dronecan_put_u8(buffer, capacity, HW_OFF_MAJOR, 8U, info->hardware_major);
    dronecan_put_u8(buffer, capacity, HW_OFF_MINOR, 8U, info->hardware_minor);
    for (uint8_t i = 0U; i < DRONECAN_UNIQUE_ID_LEN; i++)
    {
        dronecan_put_u8(buffer, capacity, HW_OFF_UNIQUE_ID + (8U * i), 8U, info->unique_id[i]);
    }
    dronecan_put_u8(buffer, capacity, HW_OFF_COA_LEN, 8U, coa_length);   /* 无证书 */

    /* --- name: uint8[<=80]，7 bit 长度 + ASCII --- */
    dronecan_put_u8(buffer, capacity, NAME_OFF_LEN, 7U, (uint8_t)name_length);
    for (uint32_t i = 0U; i < name_length; i++)
    {
        dronecan_put_u8(buffer, capacity, NAME_OFF_DATA + (8U * i), 8U, (uint8_t)info->name[i]);
    }

    return length;
}

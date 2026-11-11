/**
  ******************************************************************************
  * @file    dronecan_msgs.h
  * @brief   DroneCAN（UAVCAN v0）标准消息的纯序列化层。
  *
  *          这里只做 "C 结构体 <-> 传输载荷字节流" 的转换，不碰 CAN 硬件，
  *          因此可以在 PC 上单独编译做字节级测试（见 tests/host/）。
  *
  *          本工程用到三个标准数据类型：
  *            uavcan.protocol.NodeStatus          dtid=341   心跳（1 Hz）
  *            uavcan.protocol.GetNodeInfo         dtid=1     服务：节点信息
  *            uavcan.equipment.esc.RawCommand     dtid=1030  4 路 PWM 命令
  *
  *          DTID 取自 dronecan/dsdl 仓库的文件名前缀（如 341.NodeStatus.uavcan），
  *          注意 DroneCAN 与 uavcan v0 的部分 DTID 不同：RawCommand 在
  *          DroneCAN 中是 1030（uavcan v0 的 103 已作废）。
  *
  *          签名（data type signature）由 DSDL 规范化文本经 CRC-64-WE 得到，
  *          用 dronecan.dsdl 解析器对 dronecan/dsdl 仓库计算获得。
  *          libcanard 的传输 CRC 以签名为种子，签名填错会让对端整帧丢弃，
  *          因此修改消息定义时必须同步更新这里的常量。
  ******************************************************************************
  */
#ifndef DRONECAN_MSGS_H
#define DRONECAN_MSGS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* 标准数据类型 ID / 签名                                                      */
/* -------------------------------------------------------------------------- */
#define DRONECAN_DTID_NODE_STATUS          341U
#define DRONECAN_DTID_GET_NODE_INFO          1U
#define DRONECAN_DTID_ESC_RAW_COMMAND     1030U

#define DRONECAN_SIGNATURE_NODE_STATUS      0x0F0868D0C1A7C6F1ULL
#define DRONECAN_SIGNATURE_GET_NODE_INFO    0xEE468A8121C46A9EULL
#define DRONECAN_SIGNATURE_ESC_RAW_COMMAND  0x217F5C87D7EC951DULL

/* uavcan.protocol.NodeStatus 的枚举（health / mode） */
#define DRONECAN_HEALTH_OK                  0U
#define DRONECAN_HEALTH_WARNING             1U
#define DRONECAN_HEALTH_ERROR               2U
#define DRONECAN_HEALTH_CRITICAL            3U

#define DRONECAN_MODE_OPERATIONAL           0U
#define DRONECAN_MODE_INITIALIZATION        1U
#define DRONECAN_MODE_MAINTENANCE           2U
#define DRONECAN_MODE_SOFTWARE_UPDATE       3U
#define DRONECAN_MODE_OFFLINE               7U

/* uavcan.equipment.esc.RawCommand 的归一化范围 */
#define DRONECAN_RAW_COMMAND_MIN          (-8192)
#define DRONECAN_RAW_COMMAND_MAX          8191
#define DRONECAN_RAW_COMMAND_CAPACITY     20U

/* GetNodeInfo.Response.name 的长度上限 */
#define DRONECAN_NODE_NAME_MAX            80U
#define DRONECAN_UNIQUE_ID_LEN            16U

/* -------------------------------------------------------------------------- */
/* 序列化所需的最小数据结构                                                    */
/* -------------------------------------------------------------------------- */

/**
  * @brief GetNodeInfo.Response 需要填写的字段
  */
typedef struct
{
    uint32_t      uptime_sec;                  /**< 运行秒数，不能溢出 */
    uint8_t       health;                      /**< DRONECAN_HEALTH_* */
    uint8_t       mode;                        /**< DRONECAN_MODE_* */
    uint16_t      vendor_status;               /**< 厂商自定义状态位 */
    uint8_t       software_major;              /**< 固件主版本 */
    uint8_t       software_minor;              /**< 固件次版本 */
    uint8_t       hardware_major;              /**< 硬件主版本 */
    uint8_t       hardware_minor;              /**< 硬件次版本 */
    const uint8_t *unique_id;                  /**< 16 字节硬件唯一 ID，不能全 0 */
    const char    *name;                       /**< 节点名，reversed domain，<= 80 字符 */
} DronecanNodeInfo;

/* -------------------------------------------------------------------------- */
/* 编码函数：返回写入 buffer 的字节数，0 表示失败（容量不足或参数非法）         */
/* -------------------------------------------------------------------------- */

/** 编码 uavcan.protocol.NodeStatus，固定 7 字节 */
uint16_t DronecanMsgs_EncodeNodeStatus(uint8_t *buffer, uint16_t capacity,
                                       uint32_t uptime_sec, uint8_t health, uint8_t mode,
                                       uint16_t vendor_status);

/** 编码 uavcan.equipment.esc.RawCommand：uint8 长度前缀(5 bit) + count × int14 */
uint16_t DronecanMsgs_EncodeRawCommand(uint8_t *buffer, uint16_t capacity,
                                       const int16_t *cmd, uint8_t count);

/** 编码 uavcan.protocol.GetNodeInfo.Response（NodeStatus + 版本 + 唯一 ID + 名称） */
uint16_t DronecanMsgs_EncodeGetNodeInfoResponse(uint8_t *buffer, uint16_t capacity,
                                                const DronecanNodeInfo *info);

#ifdef __cplusplus
}
#endif

#endif /* DRONECAN_MSGS_H */

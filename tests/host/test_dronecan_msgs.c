/**
  ******************************************************************************
  * @file    test_dronecan_msgs.c
  * @brief   Host 侧字节级测试：打印三条标准消息的编码结果。
  *
  *          与 tests/host/check_msgs.py 的参考实现逐字节比对，
  *          用来保证 DSDL 位打包/长度前缀/签名没有写错。
  *
  *          构建（PC，不需要交叉编译器）：
  *            gcc -I../../Core/Inc -I../../ThirdParty/libcanard \
  *                test_dronecan_msgs.c ../../Core/Src/dronecan_msgs.c \
  *                ../../ThirdParty/libcanard/canard.c -o test_dronecan_msgs
  ******************************************************************************
  */
#include "dronecan_msgs.h"

#include <stdio.h>
#include <string.h>

static void dump(const char *name, const uint8_t *buffer, uint16_t length)
{
    printf("%s ", name);
    for (uint16_t i = 0U; i < length; i++)
    {
        printf("%02x", buffer[i]);
    }
    printf("\n");
}

int main(void)
{
    uint8_t buffer[128];
    uint16_t length;

    /* 1) NodeStatus：uptime=1234, health=2, mode=0, vendor=0x1234 */
    length = DronecanMsgs_EncodeNodeStatus(buffer, sizeof(buffer), 1234U, 2U, 0U, 0x1234U);
    dump("NodeStatus", buffer, length);

    /* 2) RawCommand：4 路，覆盖最小值/负值/零/最大值 */
    {
        const int16_t cmd[4] = { -8192, -1, 0, 8191 };

        length = DronecanMsgs_EncodeRawCommand(buffer, sizeof(buffer), cmd, 4U);
        dump("RawCommand4", buffer, length);
    }

    /* 3) RawCommand：单通道与满 20 通道（验证 5 bit 长度前缀与边界） */
    {
        const int16_t cmd[20] = { 777, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                  0, 0, 0, 0, 0, 0, 0, 0, 0, -777 };

        length = DronecanMsgs_EncodeRawCommand(buffer, sizeof(buffer), cmd, 1U);
        dump("RawCommand1", buffer, length);

        length = DronecanMsgs_EncodeRawCommand(buffer, sizeof(buffer), cmd, 20U);
        dump("RawCommand20", buffer, length);
    }

    /* 4) GetNodeInfo.Response：名称 18 字符，唯一 ID 00..0f */
    {
        static const uint8_t uid[DRONECAN_UNIQUE_ID_LEN] = {
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
        };
        DronecanNodeInfo info;

        memset(&info, 0, sizeof(info));
        info.uptime_sec     = 1234U;
        info.health         = 1U;
        info.mode           = 0U;
        info.vendor_status  = 0x0005U;
        info.software_major = 1U;
        info.software_minor = 2U;
        info.hardware_major = 3U;
        info.hardware_minor = 4U;
        info.unique_id      = uid;
        info.name           = "com.hpaics.control";

        length = DronecanMsgs_EncodeGetNodeInfoResponse(buffer, sizeof(buffer), &info);
        dump("GetNodeInfo", buffer, length);
    }

    /* 5) 非法参数应返回 0 */
    printf("RejectNull %u\n", DronecanMsgs_EncodeRawCommand(buffer, sizeof(buffer), NULL, 4U));
    printf("RejectEmpty %u\n", DronecanMsgs_EncodeRawCommand(buffer, sizeof(buffer), NULL, 0U));

    return 0;
}

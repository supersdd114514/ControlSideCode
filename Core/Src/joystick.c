#include "joystick.h"
#include "app_config.h"
#include "main.h"

extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c2;

static I2C_HandleTypeDef *const s_joy_i2c[JOYSTICK_COUNT] = { &hi2c1, &hi2c2 };
static const uint16_t s_joy_addr[JOYSTICK_COUNT] = { APP_JOY1_I2C_ADDR, APP_JOY2_I2C_ADDR };

void Joystick_Init(void)
{
    /* 预留：可在此做摇杆上电初始化、校准等操作。 */
}

JoystickData Joystick_Get(JoystickId id)
{
    JoystickData data = { 0, 0, false };

    if (id >= JOYSTICK_COUNT)
    {
        return data;
    }

    data.valid = Joystick_ReadRawCustom(id, &data.x, &data.y);
    return data;
}

/* 默认弱实现：未配置实际摇杆协议时返回 false。
 * 实现时建议参考以下 I2C 读取模板（按实际寄存器修改）：
 *
 * bool Joystick_ReadRawCustom(JoystickId id, int16_t *x, int16_t *y)
 * {
 *     uint8_t buf[4] = {0};
 *     uint16_t addr = (id == JOYSTICK_1) ? APP_JOY1_I2C_ADDR : APP_JOY2_I2C_ADDR;
 *     if (addr == 0) return false;
 *
 *     // 示例：从寄存器 0x00 连续读 4 字节，低 2 字节为 X，高 2 字节为 Y。
 *     if (HAL_I2C_Mem_Read(s_joy_i2c[id], (uint16_t)(addr << 1), 0x00,
 *                          I2C_MEMADD_SIZE_8BIT, buf, sizeof(buf),
 *                          APP_JOY_I2C_TIMEOUT_MS) != HAL_OK)
 *     {
 *         return false;
 *     }
 *     *x = (int16_t)((buf[0] << 8) | buf[1]);
 *     *y = (int16_t)((buf[2] << 8) | buf[3]);
 *     return true;
 * }
 */
__weak bool Joystick_ReadRawCustom(JoystickId id, int16_t *x, int16_t *y)
{
    (void)id;
    (void)x;
    (void)y;
    (void)s_joy_i2c;
    (void)s_joy_addr;
    return false;
}

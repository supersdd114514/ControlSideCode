#include "app_user.h"
#include "app_config.h"
#include "main.h"

extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c2;

/* ========================================================================== */
/* 1. 摇杆读取：请根据实际摇杆模块协议修改                                      */
/* ========================================================================== */
bool Joystick_ReadRawCustom(JoystickId id, int16_t *x, int16_t *y)
{
    if ((x == 0) || (y == 0) || (id >= JOYSTICK_COUNT))
    {
        return false;
    }

    *x = 0;
    *y = 0;

    /* TODO：按实际模块实现。下面是一个 I2C 读取模板：
     *
     * uint8_t buf[4] = {0};
     * uint16_t addr = (id == JOYSTICK_1) ? APP_JOY1_I2C_ADDR : APP_JOY2_I2C_ADDR;
     * uint8_t reg = 0x00;   // 按实际寄存器修改
     * I2C_HandleTypeDef *hi2c = (id == JOYSTICK_1) ? &hi2c1 : &hi2c2;
     *
     * if (addr == 0U) return false;
     * if (HAL_I2C_Mem_Read(hi2c, (uint16_t)(addr << 1), reg, I2C_MEMADD_SIZE_8BIT,
     *                      buf, sizeof(buf), APP_JOY_I2C_TIMEOUT_MS) != HAL_OK)
     * {
     *     return false;
     * }
     * *x = (int16_t)((buf[0] << 8) | buf[1]);
     * *y = (int16_t)((buf[2] << 8) | buf[3]);
     * return true;
     */

    (void)hi2c1;
    (void)hi2c2;
    return false;
}

/* ========================================================================== */
/* 2. 摇杆 -> CRSF 通道映射                                                    */
/* ========================================================================== */
uint16_t App_MapJoystickToCrsf(int16_t raw, bool valid)
{
    int32_t span;

    if (!valid)
    {
        return (uint16_t)APP_CRSF_CHANNEL_VALUE_NEUTRAL;
    }

    span = (int32_t)APP_JOY_RAW_MAX - (int32_t)APP_JOY_RAW_MIN;
    if (span <= 0)
    {
        return (uint16_t)APP_CRSF_CHANNEL_VALUE_NEUTRAL;
    }

    int32_t value = (int32_t)raw - (int32_t)APP_JOY_RAW_MIN;
    value = (value * (int32_t)APP_CRSF_CHANNEL_VALUE_MAX) / span;

    if (value < 0) value = 0;
    if (value > (int32_t)APP_CRSF_CHANNEL_VALUE_MAX) value = (int32_t)APP_CRSF_CHANNEL_VALUE_MAX;
    return (uint16_t)value;
}

/* ========================================================================== */
/* 3. 开关 -> CRSF 通道映射                                                    */
/* ========================================================================== */
uint16_t App_MapSwitchToCrsf(SwitchPosition position)
{
    switch (position)
    {
        case SWITCH_POS_A:
            return 0U;
        case SWITCH_POS_B:
            return (uint16_t)APP_CRSF_CHANNEL_VALUE_MAX;
        default:
            return (uint16_t)APP_CRSF_CHANNEL_VALUE_NEUTRAL;
    }
}

/* ========================================================================== */
/* 4. 按钮 -> CRSF 通道映射                                                    */
/* ========================================================================== */
uint16_t App_MapButtonToCrsf(bool pressed)
{
    return pressed ? (uint16_t)APP_CRSF_CHANNEL_VALUE_MAX : 0U;
}

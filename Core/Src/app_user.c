#include "app_user.h"
#include "app_config.h"
#include "main.h"
#include "m5_unit_joystick2.h"

extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c2;

/* ========================================================================== */
/* 1. 摇杆读取：M5Stack Unit Joystick2（I2C1 = 摇杆 1，I2C2 = 摇杆 2）        */
/* ========================================================================== */
/* 模块固定 7-bit 地址 0x63，寄存器映射见 m5_unit_joystick2.h。
 * 16-bit 原始值寄存器：0x00/0x01 = X，0x02/0x03 = Y（小端）。
 * 模块出厂 trim 不在量程正中间（X_AXIS_TRIM / Y_AXIS_TRIM），
 * 所以必须按 trim 分段映射，直接把 0..65535 线性拉伸会让回中位置偏移。 */

typedef struct
{
    m5_joystick2_t dev;           /* 驱动实例（含 hi2c / addr / timeout） */
    bool           ready;         /* 当前是否已握手成功 */
    uint32_t       last_probe_ms; /* 上次探测时刻，用于限制重探测频率 */
} JoyContext;

static JoyContext s_joy[JOYSTICK_COUNT];

static I2C_HandleTypeDef *Joy_GetI2c(JoystickId id)
{
    return (id == JOYSTICK_1) ? &hi2c1 : &hi2c2;
}

static uint8_t Joy_GetAddr(JoystickId id)
{
    return (id == JOYSTICK_1) ? APP_JOY1_I2C_ADDR : APP_JOY2_I2C_ADDR;
}

/**
  * @brief 把模块的 16-bit ADC 原始值映射成以中点为 0 的 int16
  * @param raw      原始 ADC 值
  * @param raw_min  该轴机械下限
  * @param raw_trim 该轴出厂中点（回中值）
  * @param raw_max  该轴机械上限
  * @retval -32768..32767，0 表示回中
  */
static int16_t Joy_RawToCentered(uint16_t raw, uint16_t raw_min, uint16_t raw_trim, uint16_t raw_max)
{
    uint32_t span;

    if (raw <= raw_min)
    {
        return -32768;
    }
    if (raw >= raw_max)
    {
        return 32767;
    }

    /* trim 参数异常时退化为整段线性映射，避免除零 */
    if ((raw_trim <= raw_min) || (raw_trim >= raw_max))
    {
        span = (uint32_t)(raw_max - raw_min);
        return (int16_t)((int32_t)(((uint32_t)(raw - raw_min) * 65536U) / span) - 32768);
    }

    if (raw <= raw_trim)
    {
        /* 下段：[raw_min, raw_trim] -> [-32768, 0] */
        span = (uint32_t)(raw_trim - raw_min);
        return (int16_t)((int32_t)(((uint32_t)(raw - raw_min) * 32768U) / span) - 32768);
    }

    /* 上段：(raw_trim, raw_max] -> (0, 32767] */
    span = (uint32_t)(raw_max - raw_trim);
    return (int16_t)((int32_t)(((uint32_t)(raw - raw_trim) * 32767U) / span));
}

/* 强符号：覆盖 joystick.c 中的空实现 */
void Joystick_Init(void)
{
    for (uint8_t i = 0U; i < (uint8_t)JOYSTICK_COUNT; i++)
    {
        JoystickId id = (JoystickId)i;

        s_joy[i].ready         = false;
        s_joy[i].last_probe_ms = HAL_GetTick();

        /* begin() 即使模块不在线也会填好 hi2c/addr/timeout，
         * 因此之后仍可用 m5_joystick2_probe() 做热插拔重连。
         * 模块缺失不是致命错误：对应通道保持中立值。 */
        s_joy[i].ready = m5_joystick2_begin(&s_joy[i].dev, Joy_GetI2c(id),
                                            Joy_GetAddr(id), APP_JOY_I2C_TIMEOUT_MS);
    }
}

bool Joystick_ReadRawCustom(JoystickId id, int16_t *x, int16_t *y)
{
    uint16_t raw_x = 0U;
    uint16_t raw_y = 0U;

    if ((x == NULL) || (y == NULL) || (id >= JOYSTICK_COUNT))
    {
        return false;
    }

    *x = 0;
    *y = 0;

    if (!s_joy[id].ready)
    {
        uint32_t now = HAL_GetTick();

        /* 模块不在线时不要每个周期都探测：I2C 超时会拖慢 50 Hz 的 CRSF 发送 */
        if ((now - s_joy[id].last_probe_ms) < APP_JOY_REPROBE_PERIOD_MS)
        {
            return false;
        }
        s_joy[id].last_probe_ms = now;

        if (m5_joystick2_probe(&s_joy[id].dev, 1U) != HAL_OK)
        {
            return false;
        }
        s_joy[id].ready = true;
    }

    if (m5_joystick2_read_joy_xy(&s_joy[id].dev, &raw_x, &raw_y) != HAL_OK)
    {
        /* 掉线：标记失效，下一周期按 APP_JOY_REPROBE_PERIOD_MS 重新握手 */
        s_joy[id].ready         = false;
        s_joy[id].last_probe_ms = HAL_GetTick();
        return false;
    }

    *x = Joy_RawToCentered(raw_x, (uint16_t)X_AXIS_MIN, (uint16_t)X_AXIS_TRIM, (uint16_t)X_AXIS_MAX);
    *y = Joy_RawToCentered(raw_y, (uint16_t)Y_AXIS_MIN, (uint16_t)Y_AXIS_TRIM, (uint16_t)Y_AXIS_MAX);

    return true;
}

/* ========================================================================== */
/* 2. 摇杆 -> CRSF 通道映射                                                    */
/* ========================================================================== */
/* 输入 raw 来自 Joy_RawToCentered()，范围 -32768..32767，中点 0。
 * 输出为 CRSF 合法量程 172..1811（中点 992）。 */
uint16_t App_MapJoystickToCrsf(int16_t raw, bool valid)
{
    const int32_t span = (int32_t)APP_CRSF_CHANNEL_VALUE_MAX - (int32_t)APP_CRSF_CHANNEL_VALUE_NEUTRAL;
    int32_t       out;

    if (!valid)
    {
        return (uint16_t)APP_CRSF_CHANNEL_VALUE_NEUTRAL;
    }

    out = (int32_t)APP_CRSF_CHANNEL_VALUE_NEUTRAL + ((int32_t)raw * span) / 32768;

    if (out < (int32_t)APP_CRSF_CHANNEL_VALUE_MIN)
    {
        out = (int32_t)APP_CRSF_CHANNEL_VALUE_MIN;
    }
    if (out > (int32_t)APP_CRSF_CHANNEL_VALUE_MAX)
    {
        out = (int32_t)APP_CRSF_CHANNEL_VALUE_MAX;
    }
    return (uint16_t)out;
}

/* ========================================================================== */
/* 3. 开关 -> CRSF 通道映射                                                    */
/* ========================================================================== */
/* 172 -> 988 us，1811 -> 2012 us，正好覆盖 ArduPilot / CRSF 的标准行程。 */
uint16_t App_MapSwitchToCrsf(SwitchPosition position)
{
    switch (position)
    {
        case SWITCH_POS_A:
            return (uint16_t)APP_CRSF_CHANNEL_VALUE_MIN;
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
    return pressed ? (uint16_t)APP_CRSF_CHANNEL_VALUE_MAX : (uint16_t)APP_CRSF_CHANNEL_VALUE_MIN;
}

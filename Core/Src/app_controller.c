#include "app_controller.h"
#include "app_config.h"
#include "joystick.h"
#include "switch_input.h"
#include "pwm_input.h"
#include "crsf.h"
#include "uavcan.h"
#include "main.h"

static uint32_t s_last_crsf_tick = 0U;
static uint32_t s_last_uavcan_tick = 0U;

/* ========================================================================== */
/* 以下映射函数均定义为 weak，便于用户在单独文件中覆盖实现。                  */
/* ========================================================================== */

/* TODO：根据实际摇杆量程和方向调整 CRSF 映射。 */
__weak uint16_t App_MapJoystickToCrsf(int16_t raw, bool valid)
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

    if (value < 0)
    {
        value = 0;
    }
    if (value > (int32_t)APP_CRSF_CHANNEL_VALUE_MAX)
    {
        value = (int32_t)APP_CRSF_CHANNEL_VALUE_MAX;
    }
    return (uint16_t)value;
}

/* TODO：单刀双掷开关在 CRSF 通道中的值，可按飞控端习惯调整。 */
__weak uint16_t App_MapSwitchToCrsf(SwitchPosition position)
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

/* TODO：按钮按下/松开在 CRSF 通道中的值。 */
__weak uint16_t App_MapButtonToCrsf(bool pressed)
{
    return pressed ? (uint16_t)APP_CRSF_CHANNEL_VALUE_MAX : 0U;
}

static void App_BuildCrsfChannels(void)
{
    JoystickData joy1 = Joystick_Get(JOYSTICK_1);
    JoystickData joy2 = Joystick_Get(JOYSTICK_2);
    SwitchPosition sw = SwitchInput_GetPosition();
    bool button = SwitchInput_GetButton();

    (void)CRSF_SetChannel(APP_CRSF_CH_JOY1_X, App_MapJoystickToCrsf(joy1.x, joy1.valid));
    (void)CRSF_SetChannel(APP_CRSF_CH_JOY1_Y, App_MapJoystickToCrsf(joy1.y, joy1.valid));
    (void)CRSF_SetChannel(APP_CRSF_CH_JOY2_X, App_MapJoystickToCrsf(joy2.x, joy2.valid));
    (void)CRSF_SetChannel(APP_CRSF_CH_JOY2_Y, App_MapJoystickToCrsf(joy2.y, joy2.valid));
    (void)CRSF_SetChannel(APP_CRSF_CH_SWITCH, App_MapSwitchToCrsf(sw));
    (void)CRSF_SetChannel(APP_CRSF_CH_BUTTON, App_MapButtonToCrsf(button));
}

void App_Init(void)
{
    Joystick_Init();
    SwitchInput_Init();
    PWMInput_Init();
    CRSF_Init();
    Uavcan_Init();

    s_last_crsf_tick   = HAL_GetTick();
    s_last_uavcan_tick = HAL_GetTick();
}

void App_Update(void)
{
    uint32_t now = HAL_GetTick();

    SwitchInput_Update();
    Uavcan_Update();

    if ((now - s_last_crsf_tick) >= (1000U / APP_CRSF_TX_RATE_HZ))
    {
        s_last_crsf_tick = now;
        App_BuildCrsfChannels();
        (void)CRSF_Transmit();
    }

    if ((now - s_last_uavcan_tick) >= APP_UAVCAN_PUBLISH_PERIOD_MS)
    {
        s_last_uavcan_tick = now;

        uint16_t pwm_us[UAVCAN_PWM_CHANNEL_COUNT] = {0};
        PWMInput_GetAllPulseUs(pwm_us);
        (void)Uavcan_PublishPwm(pwm_us);
    }
}

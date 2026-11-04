#include "app_controller.h"
#include "app_config.h"
#include "app_user.h"
#include "joystick.h"
#include "switch_input.h"
#include "pwm_input.h"
#include "crsf.h"
#include "uavcan.h"
#include "main.h"

static uint32_t s_last_crsf_tick = 0U;
static uint32_t s_last_uavcan_tick = 0U;

/* 摇杆/开关/按钮 -> CRSF 通道值的映射实现在 Core/Src/app_user.c
 * （App_MapJoystickToCrsf / App_MapSwitchToCrsf / App_MapButtonToCrsf），
 * 这里只做调度，不再保留重复的 weak 副本，避免两处行为不一致。 */

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

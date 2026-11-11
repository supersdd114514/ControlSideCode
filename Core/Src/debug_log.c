/**
  ******************************************************************************
  * @file    debug_log.c
  * @brief   USART1 调试输出实现。
  ******************************************************************************
  */
#include "debug_log.h"
#include "app_config.h"
#include "main.h"
#include "joystick.h"
#include "switch_input.h"
#include "pwm_input.h"
#include "uavcan.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

extern UART_HandleTypeDef huart1;

#define DEBUG_LOG_LINE_MAX   128U
#define DEBUG_LOG_TX_TIMEOUT 20U    /* ms；单行 @115200 约 11 ms */

static uint32_t s_last_status_ms;

void DebugLog_Printf(const char *format, ...)
{
#if APP_DEBUG_LOG_ENABLE
    char    line[DEBUG_LOG_LINE_MAX];
    va_list args;
    int     written;

    va_start(args, format);
    written = vsnprintf(line, sizeof(line) - 2U, format, args);
    va_end(args);

    if (written <= 0)
    {
        return;
    }

    /* vsnprintf 返回"想写的长度"，被截断时按缓冲区尾部处理 */
    if ((uint32_t)written > (sizeof(line) - 3U))
    {
        written = (int)(sizeof(line) - 3U);
    }

    line[written]     = '\r';
    line[written + 1] = '\n';
    (void)HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)(written + 2), DEBUG_LOG_TX_TIMEOUT);
#else
    (void)format;
#endif
}

void DebugLog_Init(void)
{
#if APP_DEBUG_LOG_ENABLE
    JoystickData joy1 = Joystick_Get(JOYSTICK_1);
    JoystickData joy2 = Joystick_Get(JOYSTICK_2);
    UavcanStats  can;

    Uavcan_GetStats(&can);

    DebugLog_Printf("ControlSideCode %u.%u  SYSCLK=%lu Hz  CAN node=%u%s",
                    (unsigned)APP_UAVCAN_SW_VERSION_MAJOR, (unsigned)APP_UAVCAN_SW_VERSION_MINOR,
                    (unsigned long)HAL_RCC_GetHCLKFreq(),
                    (unsigned)APP_UAVCAN_NODE_ID, can.ready ? " (ready)" : " (NOT READY)");
    DebugLog_Printf("joystick1=%s joystick2=%s  crsf=%lu baud  can=%lu kbit",
                    joy1.valid ? "ok" : "absent", joy2.valid ? "ok" : "absent",
                    (unsigned long)APP_CRSF_BAUDRATE, (unsigned long)(APP_CAN_NOMINAL_BITRATE / 1000U));

    s_last_status_ms = HAL_GetTick();
#endif
}

void DebugLog_Update(void)
{
#if APP_DEBUG_LOG_ENABLE
    uint32_t now = HAL_GetTick();

    if ((now - s_last_status_ms) < 1000U)
    {
        return;
    }
    s_last_status_ms = now;

    JoystickData   joy1 = Joystick_Get(JOYSTICK_1);
    JoystickData   joy2 = Joystick_Get(JOYSTICK_2);
    uint16_t       pwm[PWM_INPUT_CHANNEL_COUNT];
    SwitchPosition sw = SwitchInput_GetPosition();
    UavcanStats    can;

    PWMInput_GetAllPulseUs(pwm);
    Uavcan_GetStats(&can);

    DebugLog_Printf("t=%lus pwm=%u/%u/%u/%u v=%u joy1=%d,%d joy2=%d,%d sw=%s btn=%u can=%u/%u",
                    (unsigned long)(now / 1000U),
                    (unsigned)pwm[0], (unsigned)pwm[1], (unsigned)pwm[2], (unsigned)pwm[3],
                    (unsigned)PWMInput_GetValidMask(),
                    (int)joy1.x, (int)joy1.y, (int)joy2.x, (int)joy2.y,
                    (sw == SWITCH_POS_A) ? "A" : ((sw == SWITCH_POS_B) ? "B" : "?"),
                    (unsigned)SwitchInput_GetButton(),
                    (unsigned)can.tx_frames, (unsigned)can.rx_frames);
#endif
}

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/* ========================================================================== */
/* 1. CRSF / UART4 配置                                                       */
/* ========================================================================== */
#define APP_CRSF_BAUDRATE               420000U  /* CRSF 常用波特率 */
#define APP_CRSF_UART_TIMEOUT_MS        10U
#define APP_CRSF_TX_RATE_HZ             50U      /* CRSF 发送频率 */
#define APP_CRSF_CHANNEL_COUNT          16U
#define APP_CRSF_CHANNEL_VALUE_MAX      2047U
#define APP_CRSF_CHANNEL_VALUE_NEUTRAL  1024U

/* CRSF 通道映射：按实际摇杆/开关定义调整 */
#define APP_CRSF_CH_JOY1_X              0U
#define APP_CRSF_CH_JOY1_Y              1U
#define APP_CRSF_CH_JOY2_X              2U
#define APP_CRSF_CH_JOY2_Y              3U
#define APP_CRSF_CH_SWITCH              4U
#define APP_CRSF_CH_BUTTON              5U

/* ========================================================================== */
/* 2. I2C 摇杆配置（TODO：按实际模块填写）                                     */
/* ========================================================================== */
/* 两个摇杆的 7-bit I2C 地址。0 表示“未配置”，此时 Joystick_ReadRawCustom()
 * 返回 false，输出将保持中立值。请根据实际模块修改，或直接实现
 * Joystick_ReadRawCustom() 强符号版本。 */
#define APP_JOY1_I2C_ADDR               0x00U
#define APP_JOY2_I2C_ADDR               0x00U
#define APP_JOY_I2C_TIMEOUT_MS          20U

/* 摇杆原始值量程，用于 CRSF 线性映射。请按实际传感器量程修改。 */
#define APP_JOY_RAW_MIN                 0
#define APP_JOY_RAW_MAX                 4095

/* ========================================================================== */
/* 3. 单刀双掷开关 / 按钮配置                                                 */
/* ========================================================================== */
/* PA4/PA5/PA6 均配置了内部上拉，因此按下/接通到 GND 时为低电平。 */
#define APP_SWITCH_DEBOUNCE_MS          20U
#define APP_BUTTON_DEBOUNCE_MS          20U
#define APP_SWITCH_ACTIVE_LEVEL         GPIO_PIN_RESET
#define APP_BUTTON_ACTIVE_LEVEL         GPIO_PIN_RESET

/* ========================================================================== */
/* 4. PWM 输入捕获配置                                                        */
/* ========================================================================== */
/* 目标：让每个定时器计数频率为 1 MHz，即计数值 = 微秒。
 * TIM1 挂在 APB2 定时器时钟 168 MHz；
 * TIM2/3/4 挂在 APB1 定时器时钟 84 MHz。 */
#define APP_PWM_TIM1_PRESCALER          167U   /* 168 MHz / (167+1) = 1 MHz */
#define APP_PWM_TIM234_PRESCALER        83U    /*  84 MHz / (83+1)  = 1 MHz */
#define APP_PWM_TIM_PERIOD              0xFFFFU
#define APP_PWM_MIN_VALID_US            500U
#define APP_PWM_MAX_VALID_US            2500U

/* ========================================================================== */
/* 5. CAN / UAVCAN（DroneCAN）配置                                            */
/* ========================================================================== */
#define APP_CAN_NOMINAL_BITRATE         1000000U
#define APP_UAVCAN_NODE_ID              42U     /* TODO：与总线分配一致 */
#define APP_UAVCAN_PUBLISH_PERIOD_MS    20U
#define APP_UAVCAN_PWM_DATA_TYPE_ID     2000U   /* TODO：改为 DSDL 生成的实际 DataType ID */
#define APP_UAVCAN_USE_RAW_FALLBACK     0       /* 0=严格 UAVCAN/DroneCAN；1=临时裸 CAN 调试 */
#define APP_UAVCAN_RAW_FALLBACK_CAN_ID  0x1A000000U /* 仅当 RAW_FALLBACK=1 时使用 */

#ifdef __cplusplus
}
#endif

#endif /* APP_CONFIG_H */

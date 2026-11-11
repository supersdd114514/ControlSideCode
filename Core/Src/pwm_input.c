#include "pwm_input.h"
#include "app_config.h"
#include "main.h"

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;

typedef struct
{
    volatile uint32_t period_ticks;
    volatile uint32_t pulse_ticks;
    volatile uint32_t last_capture_ms;   /* 最近一次捕获到的时刻，用于断线检测 */
    volatile bool     valid;             /* 脉宽是否落在有效量程内 */
} PwmChannel;

static PwmChannel s_pwm[PWM_INPUT_CHANNEL_COUNT] = {0};

static TIM_HandleTypeDef *const s_tim[PWM_INPUT_CHANNEL_COUNT] =
{
    &htim1, /* PE9  TIM1_CH1 */
    &htim2, /* PA15 TIM2_CH1_ETR */
    &htim3, /* PC6  TIM3_CH1 */
    &htim4  /* PD12 TIM4_CH1 */
};

static const uint32_t s_prescaler[PWM_INPUT_CHANNEL_COUNT] =
{
    APP_PWM_TIM1_PRESCALER,
    APP_PWM_TIM234_PRESCALER,
    APP_PWM_TIM234_PRESCALER,
    APP_PWM_TIM234_PRESCALER
};

static int8_t PWMInput_GetIndex(const TIM_HandleTypeDef *htim)
{
    for (uint8_t i = 0U; i < PWM_INPUT_CHANNEL_COUNT; i++)
    {
        if (s_tim[i]->Instance == htim->Instance)
        {
            return (int8_t)i;
        }
    }
    return -1;
}

void PWMInput_Init(void)
{
    for (uint8_t i = 0U; i < PWM_INPUT_CHANNEL_COUNT; i++)
    {
        TIM_HandleTypeDef *htim = s_tim[i];
        s_pwm[i].period_ticks = 0U;
        s_pwm[i].pulse_ticks  = 0U;
        s_pwm[i].valid        = false;

        /* CubeMX 生成的定时器预分频不一定适合 PWM 输入，这里统一改为 1 MHz 计数。
         * 先写 PSC/ARR，再产生一次更新事件，确保新值立即生效。 */
        __HAL_TIM_SET_PRESCALER(htim, s_prescaler[i]);
        __HAL_TIM_SET_AUTORELOAD(htim, APP_PWM_TIM_PERIOD);
        htim->Instance->EGR = TIM_EGR_UG;
        __HAL_TIM_CLEAR_FLAG(htim, TIM_FLAG_UPDATE);

        if (HAL_TIM_IC_Start_IT(htim, TIM_CHANNEL_1) != HAL_OK)
        {
            Error_Handler();
        }
        if (HAL_TIM_IC_Start_IT(htim, TIM_CHANNEL_2) != HAL_OK)
        {
            Error_Handler();
        }
    }
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    int8_t idx = PWMInput_GetIndex(htim);
    if (idx < 0)
    {
        return;
    }

    if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1)
    {
        s_pwm[idx].period_ticks = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
    }
    else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2)
    {
        uint32_t pulse = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);

        s_pwm[idx].pulse_ticks      = pulse;
        s_pwm[idx].valid            = (pulse >= APP_PWM_MIN_VALID_US) && (pulse <= APP_PWM_MAX_VALID_US);
        s_pwm[idx].last_capture_ms  = HAL_GetTick();
    }
}

bool PWMInput_IsValid(uint8_t channel)
{
    if (channel >= PWM_INPUT_CHANNEL_COUNT)
    {
        return false;
    }

    /* 除了脉宽量程，还要判断"最近一次捕获是不是够新"：
     * 飞控断电或线松之后，pulse_ticks 会一直停在最后一个值上。 */
    if (!s_pwm[channel].valid)
    {
        return false;
    }

    return ((HAL_GetTick() - s_pwm[channel].last_capture_ms) <= APP_PWM_SIGNAL_TIMEOUT_MS);
}

uint8_t PWMInput_GetValidMask(void)
{
    uint8_t mask = 0U;

    for (uint8_t i = 0U; i < PWM_INPUT_CHANNEL_COUNT; i++)
    {
        if (PWMInput_IsValid(i))
        {
            mask |= (uint8_t)(1U << i);
        }
    }

    return mask;
}

uint32_t PWMInput_GetPeriodUs(uint8_t channel)
{
    if (channel >= PWM_INPUT_CHANNEL_COUNT)
    {
        return 0U;
    }
    return s_pwm[channel].period_ticks;
}

uint32_t PWMInput_GetPulseUs(uint8_t channel)
{
    if (channel >= PWM_INPUT_CHANNEL_COUNT)
    {
        return 0U;
    }
    return s_pwm[channel].pulse_ticks;
}

void PWMInput_GetAllPulseUs(uint16_t out[PWM_INPUT_CHANNEL_COUNT])
{
    for (uint8_t i = 0U; i < PWM_INPUT_CHANNEL_COUNT; i++)
    {
        uint32_t pulse = s_pwm[i].pulse_ticks;
        out[i] = (pulse > 0xFFFFU) ? 0xFFFFU : (uint16_t)pulse;
    }
}

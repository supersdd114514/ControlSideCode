#include "switch_input.h"
#include "app_config.h"
#include "main.h"

static SwitchPosition s_switch_position = SWITCH_POS_UNKNOWN;
static bool           s_button_pressed = false;

static SwitchPosition s_switch_candidate = SWITCH_POS_UNKNOWN;
static uint32_t       s_switch_candidate_ms = 0U;

static bool     s_button_candidate = false;
static uint32_t s_button_candidate_ms = 0U;

void SwitchInput_Init(void)
{
    s_switch_position  = SWITCH_POS_UNKNOWN;
    s_button_pressed   = false;
    s_switch_candidate = SWITCH_POS_UNKNOWN;
    s_button_candidate = false;
}

void SwitchInput_Update(void)
{
    GPIO_PinState a = HAL_GPIO_ReadPin(SwitchPin1_GPIO_Port, SwitchPin1_Pin);
    GPIO_PinState b = HAL_GPIO_ReadPin(SwitchPin2_GPIO_Port, SwitchPin2_Pin);

    /* 单刀双掷：公共端接地时，被选中的触点读到低电平。 */
    SwitchPosition raw_pos = SWITCH_POS_UNKNOWN;
    if ((a == APP_SWITCH_ACTIVE_LEVEL) && (b != APP_SWITCH_ACTIVE_LEVEL))
    {
        raw_pos = SWITCH_POS_A;
    }
    else if ((b == APP_SWITCH_ACTIVE_LEVEL) && (a != APP_SWITCH_ACTIVE_LEVEL))
    {
        raw_pos = SWITCH_POS_B;
    }

    uint32_t now = HAL_GetTick();

    if (raw_pos != s_switch_candidate)
    {
        s_switch_candidate = raw_pos;
        s_switch_candidate_ms = now;
    }
    else if ((raw_pos != s_switch_position) &&
             ((now - s_switch_candidate_ms) >= APP_SWITCH_DEBOUNCE_MS))
    {
        s_switch_position = raw_pos;
    }

    bool raw_button = (HAL_GPIO_ReadPin(Brake_GPIO_Port, Brake_Pin) == APP_BUTTON_ACTIVE_LEVEL);
    if (raw_button != s_button_candidate)
    {
        s_button_candidate = raw_button;
        s_button_candidate_ms = now;
    }
    else if ((raw_button != s_button_pressed) &&
             ((now - s_button_candidate_ms) >= APP_BUTTON_DEBOUNCE_MS))
    {
        s_button_pressed = raw_button;
    }
}

SwitchPosition SwitchInput_GetPosition(void)
{
    return s_switch_position;
}

bool SwitchInput_GetButton(void)
{
    return s_button_pressed;
}

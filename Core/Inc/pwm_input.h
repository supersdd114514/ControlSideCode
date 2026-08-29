#ifndef PWM_INPUT_H
#define PWM_INPUT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PWM_INPUT_CHANNEL_COUNT 4U

/* 初始化并启动 4 路 PWM 输入捕获。计数频率统一配置为 1 MHz。 */
void PWMInput_Init(void);

bool PWMInput_IsValid(uint8_t channel);
uint32_t PWMInput_GetPeriodUs(uint8_t channel);
uint32_t PWMInput_GetPulseUs(uint8_t channel);

/* 将 4 路脉宽(us)一次性写入 out[0..3]。 */
void PWMInput_GetAllPulseUs(uint16_t out[PWM_INPUT_CHANNEL_COUNT]);

#ifdef __cplusplus
}
#endif

#endif /* PWM_INPUT_H */

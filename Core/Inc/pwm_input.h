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

/* 4 路有效性的位图：bit0..bit3 对应 CH1..CH4，1 = 该路有最新且在量程内的信号。
 * 判据是"脉宽在 500..2500 µs 且最近 APP_PWM_SIGNAL_TIMEOUT_MS 内有捕获"。 */
uint8_t PWMInput_GetValidMask(void);

/* 将 4 路脉宽(us)一次性写入 out[0..3]。 */
void PWMInput_GetAllPulseUs(uint16_t out[PWM_INPUT_CHANNEL_COUNT]);

#ifdef __cplusplus
}
#endif

#endif /* PWM_INPUT_H */

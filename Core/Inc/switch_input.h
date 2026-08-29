#ifndef SWITCH_INPUT_H
#define SWITCH_INPUT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    SWITCH_POS_A = 0,
    SWITCH_POS_B = 1,
    SWITCH_POS_UNKNOWN = 2
} SwitchPosition;

void SwitchInput_Init(void);

/* 在 App_Update() 中周期性调用，内部完成 GPIO 读取与去抖。 */
void SwitchInput_Update(void);

SwitchPosition SwitchInput_GetPosition(void);
bool SwitchInput_GetButton(void);

#ifdef __cplusplus
}
#endif

#endif /* SWITCH_INPUT_H */

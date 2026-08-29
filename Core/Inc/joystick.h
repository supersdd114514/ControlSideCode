#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    JOYSTICK_1 = 0,
    JOYSTICK_2,
    JOYSTICK_COUNT
} JoystickId;

typedef struct
{
    int16_t x;
    int16_t y;
    bool    valid;
} JoystickData;

void Joystick_Init(void);

/* 读取指定摇杆。若底层读取失败，valid=false，x/y 保持 0。 */
JoystickData Joystick_Get(JoystickId id);

/* TODO：实际摇杆读取接口。
 * 请在单独的用户文件中提供该函数的强符号实现；或直接在本仓库中
 * 实现 I2C 读取逻辑。默认弱实现返回 false。 */
bool Joystick_ReadRawCustom(JoystickId id, int16_t *x, int16_t *y);

#ifdef __cplusplus
}
#endif

#endif /* JOYSTICK_H */

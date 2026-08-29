#ifndef APP_USER_H
#define APP_USER_H

#include <stdint.h>
#include <stdbool.h>
#include "joystick.h"
#include "switch_input.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* 用户自定义接口文件                                                          */
/* 请在 Core/Src/app_user.c 中实现这些函数，集中处理所有 TODO 业务。            */
/* ========================================================================== */

/* 实际摇杆读取：返回 true 表示数据有效。 */
bool Joystick_ReadRawCustom(JoystickId id, int16_t *x, int16_t *y);

/* 摇杆原始值 -> CRSF 11-bit 通道值映射。 */
uint16_t App_MapJoystickToCrsf(int16_t raw, bool valid);

/* 单刀双掷开关 -> CRSF 通道值映射。 */
uint16_t App_MapSwitchToCrsf(SwitchPosition position);

/* 按钮状态 -> CRSF 通道值映射。 */
uint16_t App_MapButtonToCrsf(bool pressed);

#ifdef __cplusplus
}
#endif

#endif /* APP_USER_H */

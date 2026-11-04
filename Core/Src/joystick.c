#include "joystick.h"
#include "app_config.h"
#include "main.h"

extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c2;

static I2C_HandleTypeDef *const s_joy_i2c[JOYSTICK_COUNT] = { &hi2c1, &hi2c2 };
static const uint16_t s_joy_addr[JOYSTICK_COUNT] = { APP_JOY1_I2C_ADDR, APP_JOY2_I2C_ADDR };

__weak void Joystick_Init(void)
{
    /* 预留：可在此做摇杆上电初始化、校准等操作。
     * 实际实现（M5Stack Unit Joystick2 的 I2C 握手）在 Core/Src/app_user.c 中，
     * 那里提供同名强符号，会覆盖本空实现。 */
}

JoystickData Joystick_Get(JoystickId id)
{
    JoystickData data = { 0, 0, false };

    if (id >= JOYSTICK_COUNT)
    {
        return data;
    }

    data.valid = Joystick_ReadRawCustom(id, &data.x, &data.y);
    return data;
}

/* 默认弱实现：用户没有提供摇杆读取实现时返回 false，上层保持中立值。
 * 本工程的真实实现位于 Core/Src/app_user.c（M5Stack Unit Joystick2）。 */
__weak bool Joystick_ReadRawCustom(JoystickId id, int16_t *x, int16_t *y)
{
    (void)id;
    (void)x;
    (void)y;
    (void)s_joy_i2c;
    (void)s_joy_addr;
    return false;
}

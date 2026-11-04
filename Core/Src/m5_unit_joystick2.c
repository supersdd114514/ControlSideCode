/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/**
  ******************************************************************************
  * @file    m5_unit_joystick2.c
  * @brief   STM32 HAL port of the M5Stack Unit Joystick2 Arduino library.
  *
  *          Every register access of the original Wire based driver maps to a
  *          single HAL_I2C_Mem_Read() / HAL_I2C_Mem_Write() call with an 8-bit
  *          memory (register) address and the 7-bit device address shifted
  *          left by one as required by the HAL.
  ******************************************************************************
  */
#include "m5_unit_joystick2.h"

/* Private macro -------------------------------------------------------------*/
/* The HAL expects the 8-bit (already left shifted) device address, so the
   7-bit datasheet address held in the instance has to be shifted by one. */
#define JOYSTICK2_HAL_ADDR(addr) ((uint16_t)((uint16_t)(addr) << 1U))

/* Private function prototypes -----------------------------------------------*/

static HAL_StatusTypeDef m5_joystick2_write_bytes(m5_joystick2_t *dev, uint8_t reg, const uint8_t *buffer,
                                                  uint16_t length);
static HAL_StatusTypeDef m5_joystick2_read_bytes(m5_joystick2_t *dev, uint8_t reg, uint8_t *buffer, uint16_t length);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  I2C write
  * @param  dev    device instance
  * @param  reg    I2C reg address
  * @param  buffer pointer of write buffer
  * @param  length write length
  * @retval HAL status
  */
static HAL_StatusTypeDef m5_joystick2_write_bytes(m5_joystick2_t *dev, uint8_t reg, const uint8_t *buffer,
                                                  uint16_t length)
{
    return HAL_I2C_Mem_Write(dev->hi2c, JOYSTICK2_HAL_ADDR(dev->addr), reg, I2C_MEMADD_SIZE_8BIT,
                             (uint8_t *)buffer, length, dev->timeout);
}

/**
  * @brief  I2C read
  * @param  dev    device instance
  * @param  reg    I2C reg address
  * @param  buffer pointer of read buffer
  * @param  length read length
  * @retval HAL status
  */
static HAL_StatusTypeDef m5_joystick2_read_bytes(m5_joystick2_t *dev, uint8_t reg, uint8_t *buffer, uint16_t length)
{
    return HAL_I2C_Mem_Read(dev->hi2c, JOYSTICK2_HAL_ADDR(dev->addr), reg, I2C_MEMADD_SIZE_8BIT, buffer, length,
                            dev->timeout);
}

/* Exported functions --------------------------------------------------------*/

HAL_StatusTypeDef m5_joystick2_probe(m5_joystick2_t *dev, uint32_t trials)
{
    if ((dev == NULL) || (dev->hi2c == NULL) || (trials == 0U))
    {
        return HAL_ERROR;
    }

    return HAL_I2C_IsDeviceReady(dev->hi2c, JOYSTICK2_HAL_ADDR(dev->addr), trials, dev->timeout);
}

bool m5_joystick2_begin(m5_joystick2_t *dev, I2C_HandleTypeDef *hi2c, uint8_t addr, uint32_t timeout)
{
    uint8_t attempt;

    if ((dev == NULL) || (hi2c == NULL))
    {
        return false;
    }

    dev->hi2c    = hi2c;
    dev->addr    = addr;
    dev->timeout = timeout;

    HAL_Delay(10);

    for (attempt = 0; attempt < 3U; attempt++)
    {
        if (m5_joystick2_probe(dev, 3U) == HAL_OK)
        {
            return true;
        }
        HAL_Delay(10);
    }

    return false;
}

uint8_t m5_joystick2_set_i2c_address(m5_joystick2_t *dev, uint8_t addr)
{
    if (m5_joystick2_write_bytes(dev, JOYSTICK2_I2C_ADDRESS_REG, &addr, 1) != HAL_OK)
    {
        return 0;
    }

    dev->addr = addr;

    return 1;
}

uint8_t m5_joystick2_get_i2c_address(m5_joystick2_t *dev)
{
    uint8_t addr = 0;

    m5_joystick2_read_bytes(dev, JOYSTICK2_I2C_ADDRESS_REG, &addr, 1);

    return addr;
}

uint8_t m5_joystick2_get_firmware_version(m5_joystick2_t *dev)
{
    uint8_t version = 0;

    m5_joystick2_read_bytes(dev, JOYSTICK2_FIRMWARE_VERSION_REG, &version, 1);

    return version;
}

uint8_t m5_joystick2_get_bootloader_version(m5_joystick2_t *dev)
{
    uint8_t version = 0;

    m5_joystick2_read_bytes(dev, JOYSTICK2_BOOTLOADER_VERSION_REG, &version, 1);

    return version;
}

uint16_t m5_joystick2_get_joy_adc_value_x(m5_joystick2_t *dev, adc_mode_t adc_bits)
{
    uint8_t data[2] = {0, 0};
    uint16_t value  = 0;

    if (adc_bits == ADC_16BIT_RESULT)
    {
        if (m5_joystick2_read_bytes(dev, JOYSTICK2_ADC_VALUE_12BITS_REG, data, 2) == HAL_OK)
        {
            value = (uint16_t)(data[0] | (data[1] << 8));
        }
    }
    else
    {
        if (m5_joystick2_read_bytes(dev, JOYSTICK2_ADC_VALUE_8BITS_REG, data, 1) == HAL_OK)
        {
            value = data[0];
        }
    }

    return value;
}

uint16_t m5_joystick2_get_joy_adc_value_y(m5_joystick2_t *dev, adc_mode_t adc_bits)
{
    uint8_t data[2] = {0, 0};
    uint16_t value  = 0;

    if (adc_bits == ADC_16BIT_RESULT)
    {
        if (m5_joystick2_read_bytes(dev, JOYSTICK2_ADC_VALUE_12BITS_REG + 2, data, 2) == HAL_OK)
        {
            value = (uint16_t)(data[0] | (data[1] << 8));
        }
    }
    else
    {
        if (m5_joystick2_read_bytes(dev, JOYSTICK2_ADC_VALUE_8BITS_REG + 1, data, 1) == HAL_OK)
        {
            value = data[0];
        }
    }

    return value;
}

HAL_StatusTypeDef m5_joystick2_read_joy_xy(m5_joystick2_t *dev, uint16_t *adc_x, uint16_t *adc_y)
{
    uint8_t data[4] = {0, 0, 0, 0};

    if ((dev == NULL) || (adc_x == NULL) || (adc_y == NULL))
    {
        return HAL_ERROR;
    }

    /* 0x00/0x01 = X，0x02/0x03 = Y，小端；一次连续读 4 字节 */
    if (m5_joystick2_read_bytes(dev, JOYSTICK2_ADC_VALUE_12BITS_REG, data, sizeof(data)) != HAL_OK)
    {
        return HAL_ERROR;
    }

    *adc_x = (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
    *adc_y = (uint16_t)((uint16_t)data[2] | ((uint16_t)data[3] << 8));

    return HAL_OK;
}

void m5_joystick2_get_joy_adc_16bits_value_xy(m5_joystick2_t *dev, uint16_t *adc_x, uint16_t *adc_y)
{
    if ((adc_x == NULL) || (adc_y == NULL))
    {
        return;
    }

    if (m5_joystick2_read_joy_xy(dev, adc_x, adc_y) != HAL_OK)
    {
        *adc_x = 0;
        *adc_y = 0;
    }
}

void m5_joystick2_get_joy_adc_8bits_value_xy(m5_joystick2_t *dev, uint8_t *adc_x, uint8_t *adc_y)
{
    uint8_t data[2] = {0, 0};

    if ((adc_x == NULL) || (adc_y == NULL))
    {
        return;
    }

    if (m5_joystick2_read_bytes(dev, JOYSTICK2_ADC_VALUE_8BITS_REG, data, 2) != HAL_OK)
    {
        *adc_x = 0;
        *adc_y = 0;
        return;
    }

    *adc_x = data[0];
    *adc_y = data[1];
}

int16_t m5_joystick2_get_joy_adc_12bits_offset_value_x(m5_joystick2_t *dev)
{
    uint8_t data[2] = {0, 0};

    if (m5_joystick2_read_bytes(dev, JOYSTICK2_OFFSET_ADC_VALUE_12BITS_REG, data, 2) != HAL_OK)
    {
        return 0;
    }

    return (int16_t)(data[0] | (data[1] << 8));
}

int16_t m5_joystick2_get_joy_adc_12bits_offset_value_y(m5_joystick2_t *dev)
{
    uint8_t data[2] = {0, 0};

    if (m5_joystick2_read_bytes(dev, JOYSTICK2_OFFSET_ADC_VALUE_12BITS_REG + 2, data, 2) != HAL_OK)
    {
        return 0;
    }

    return (int16_t)(data[0] | (data[1] << 8));
}

int8_t m5_joystick2_get_joy_adc_8bits_offset_value_x(m5_joystick2_t *dev)
{
    uint8_t data = 0;

    if (m5_joystick2_read_bytes(dev, JOYSTICK2_OFFSET_ADC_VALUE_8BITS_REG, &data, 1) != HAL_OK)
    {
        return 0;
    }

    return (int8_t)data;
}

int8_t m5_joystick2_get_joy_adc_8bits_offset_value_y(m5_joystick2_t *dev)
{
    uint8_t data = 0;

    if (m5_joystick2_read_bytes(dev, JOYSTICK2_OFFSET_ADC_VALUE_8BITS_REG + 1, &data, 1) != HAL_OK)
    {
        return 0;
    }

    return (int8_t)data;
}

void m5_joystick2_set_joy_adc_value_cal(m5_joystick2_t *dev, uint16_t x_neg_min, uint16_t x_neg_max,
                                        uint16_t x_pos_min, uint16_t x_pos_max, uint16_t y_neg_min,
                                        uint16_t y_neg_max, uint16_t y_pos_min, uint16_t y_pos_max)
{
    const uint16_t cal[8] = {x_neg_min, x_neg_max, x_pos_min, x_pos_max,
                             y_neg_min, y_neg_max, y_pos_min, y_pos_max};
    uint8_t data[16];
    uint8_t i;

    for (i = 0; i < 8; i++)
    {
        data[i * 2]     = (uint8_t)(cal[i] & 0xFF);
        data[i * 2 + 1] = (uint8_t)(cal[i] >> 8);
    }

    m5_joystick2_write_bytes(dev, JOYSTICK2_ADC_VALUE_CAL_REG, data, sizeof(data));
}

void m5_joystick2_get_joy_adc_value_cal(m5_joystick2_t *dev, uint16_t *x_neg_min, uint16_t *x_neg_max,
                                        uint16_t *x_pos_min, uint16_t *x_pos_max, uint16_t *y_neg_min,
                                        uint16_t *y_neg_max, uint16_t *y_pos_min, uint16_t *y_pos_max)
{
    uint16_t *cal[8] = {x_neg_min, x_neg_max, x_pos_min, x_pos_max, y_neg_min, y_neg_max, y_pos_min, y_pos_max};
    uint8_t data[16] = {0};
    uint8_t i;

    for (i = 0; i < 8; i++)
    {
        if (cal[i] == NULL)
        {
            return;
        }
    }

    if (m5_joystick2_read_bytes(dev, JOYSTICK2_ADC_VALUE_CAL_REG, data, sizeof(data)) != HAL_OK)
    {
        return;
    }

    for (i = 0; i < 8; i++)
    {
        *cal[i] = (uint16_t)(data[i * 2] | (data[i * 2 + 1] << 8));
    }
}

uint8_t m5_joystick2_get_button_value(m5_joystick2_t *dev)
{
    uint8_t data = 0;

    m5_joystick2_read_bytes(dev, JOYSTICK2_BUTTON_REG, &data, 1);

    return data;
}

void m5_joystick2_set_rgb_color(m5_joystick2_t *dev, uint32_t color)
{
    uint8_t data[4];

    data[0] = (uint8_t)(color & 0xFF);
    data[1] = (uint8_t)((color >> 8) & 0xFF);
    data[2] = (uint8_t)((color >> 16) & 0xFF);
    data[3] = (uint8_t)((color >> 24) & 0xFF);

    m5_joystick2_write_bytes(dev, JOYSTICK2_RGB_REG, data, sizeof(data));
}

uint32_t m5_joystick2_get_rgb_color(m5_joystick2_t *dev)
{
    uint8_t data[4] = {0, 0, 0, 0};

    if (m5_joystick2_read_bytes(dev, JOYSTICK2_RGB_REG, data, sizeof(data)) != HAL_OK)
    {
        return 0;
    }

    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static inline uint16_t joystick2_map_axis(uint16_t raw,
                                          uint16_t raw_min,
                                          uint16_t raw_trim,
                                          uint16_t raw_max)
{
    uint32_t span;
    uint32_t num;

    if (raw <= raw_min) return 0u;
    if (raw >= raw_max) return 0xFFFFu;

    /* trim 校准异常：退化为整段线性映射，避免除零 */
    if (raw_trim <= raw_min || raw_trim >= raw_max) {
        span = (uint32_t)(raw_max - raw_min);
        num  = (uint32_t)(raw - raw_min) * 0xFFFFu;
        return (uint16_t)((num + span / 2u) / span);
    }

    if (raw <= raw_trim) {
        /* 下段：[raw_min, raw_trim] -> [0, 0x8000] */
        span = (uint32_t)(raw_trim - raw_min);
        num  = (uint32_t)(raw - raw_min) * 0x8000u;
        return (uint16_t)((num + span / 2u) / span);
    } else {
        /* 上段：[raw_trim, raw_max] -> [0x8000, 0xFFFF] */
        span = (uint32_t)(raw_max - raw_trim);
        num  = (uint32_t)(raw - raw_trim) * 0x7FFFu;
        return (uint16_t)(0x8000u + (num + span / 2u) / span);
    }
}

void m5_joystick2_data_adjust(uint32_t *buf, const uint16_t *joy_x, const uint16_t *joy_y)
{
    uint16_t x = joystick2_map_axis(*joy_x, X_AXIS_MIN, X_AXIS_TRIM, X_AXIS_MAX);
    uint16_t y = joystick2_map_axis(*joy_y, Y_AXIS_MIN, Y_AXIS_TRIM, Y_AXIS_MAX);

    /* x 放到高 16 位，y 放到低 16 位（与 M5Stack 官方 Arduino 库一致）。
     * 注意：目标必须是 uint32_t，用 uint16_t 会把这 32 位结果截断成 y。 */
    *buf = ((uint32_t)x << 16) | (uint32_t)y;
}
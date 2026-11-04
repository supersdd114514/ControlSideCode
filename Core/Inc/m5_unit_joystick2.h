/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/**
  ******************************************************************************
  * @file    m5_unit_joystick2.h
  * @brief   STM32 HAL port of the M5Stack Unit Joystick2 Arduino library.
  *
  *          The original driver talks to the unit through the Arduino Wire
  *          library. This port keeps the same register map and the same
  *          blocking API, but uses the STM32 HAL I2C memory access functions
  *          (HAL_I2C_Mem_Read / HAL_I2C_Mem_Write) instead.
  *
  *          The I2C peripheral (pins, clock speed, DMA/interrupts) must be
  *          configured by CubeMX, e.g. MX_I2C1_Init(), before calling
  *          m5_joystick2_begin(). The Arduino "Wire.begin(sda, scl)" and
  *          "Wire.setClock(speed)" calls have no counterpart here and are not
  *          needed.
  ******************************************************************************
  */
#ifndef __M5_UNIT_JOYSTICK2_H__
#define __M5_UNIT_JOYSTICK2_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

#include <stdbool.h>
#include <stdint.h>

/* Exported constants --------------------------------------------------------*/

/* Unit Joystick2 register map, identical to the Arduino library */
#define JOYSTICK2_ADDR                        0x63
#define JOYSTICK2_ADC_VALUE_12BITS_REG        0x00
#define JOYSTICK2_ADC_VALUE_8BITS_REG         0x10
#define JOYSTICK2_BUTTON_REG                  0x20
#define JOYSTICK2_RGB_REG                     0x30
#define JOYSTICK2_ADC_VALUE_CAL_REG           0x40
#define JOYSTICK2_OFFSET_ADC_VALUE_12BITS_REG 0x50
#define JOYSTICK2_OFFSET_ADC_VALUE_8BITS_REG  0x60
#define JOYSTICK2_FIRMWARE_VERSION_REG        0xFE
#define JOYSTICK2_BOOTLOADER_VERSION_REG      0xFC
#define JOYSTICK2_I2C_ADDRESS_REG             0xFF

/* Default HAL timeout used when the caller does not care about it (in ms) */
#define JOYSTICK2_DEFAULT_TIMEOUT             100U

/* Configuration of data adjusting*/
#define X_AXIS_TRIM 35650U
#define X_AXIS_MIN  0U
#define X_AXIS_MAX  65396U
#define Y_AXIS_TRIM 31965U
#define Y_AXIS_MIN  0U
#define Y_AXIS_MAX  65444U

/* Exported types ------------------------------------------------------------*/

/**
  * @brief ADC resolution of the raw value getters
  */
typedef enum { ADC_8BIT_RESULT = 0, ADC_16BIT_RESULT } adc_mode_t;

/**
  * @brief Unit Joystick2 device instance
  */
typedef struct {
    I2C_HandleTypeDef *hi2c; /**< I2C bus the unit is connected to */
    uint8_t addr;            /**< 7-bit I2C address, e.g. 0x63 (the driver left shifts it for the HAL) */
    uint32_t timeout;        /**< HAL transaction timeout in ms */
} m5_joystick2_t;

/* Exported functions prototypes ---------------------------------------------*/

/**
  * @brief  Unit Joystick2 init
  * @param  dev     pointer of a caller allocated device instance
  * @param  hi2c    pointer of the I2C handle the unit is connected to
  * @param  addr    7-bit I2C address (JOYSTICK2_ADDR)
  * @param  timeout HAL transaction timeout in ms
  * @retval true success, false device not answering
  */
bool m5_joystick2_begin(m5_joystick2_t *dev, I2C_HandleTypeDef *hi2c, uint8_t addr, uint32_t timeout);

/**
  * @brief  probe the unit once, useful to report why m5_joystick2_begin() failed
  * @param  dev    device instance filled by m5_joystick2_begin()
  * @param  trials number of address transmission attempts
  * @retval HAL_OK when the unit acknowledges its address, HAL_ERROR when it does
  *         not, HAL_TIMEOUT when the bus is held low, HAL_BUSY when the bus is busy
  */
HAL_StatusTypeDef m5_joystick2_probe(m5_joystick2_t *dev, uint32_t trials);

/**
  * @brief  set Unit Joystick2 I2C address
  * @param  dev  device instance
  * @param  addr new I2C address
  * @retval 1 success, 0 false
  */
uint8_t m5_joystick2_set_i2c_address(m5_joystick2_t *dev, uint8_t addr);

/**
  * @brief  get Unit Joystick2 I2C address
  * @param  dev device instance
  * @retval I2C address
  */
uint8_t m5_joystick2_get_i2c_address(m5_joystick2_t *dev);

/**
  * @brief  get Unit Joystick2 firmware version
  * @param  dev device instance
  * @retval firmware version
  */
uint8_t m5_joystick2_get_firmware_version(m5_joystick2_t *dev);

/**
  * @brief  get Unit Joystick2 bootloader version
  * @param  dev device instance
  * @retval bootloader version
  */
uint8_t m5_joystick2_get_bootloader_version(m5_joystick2_t *dev);

/**
  * @brief  get Unit Joystick2 x-axis ADC value
  * @param  dev      device instance
  * @param  adc_bits ADC_8BIT_RESULT or ADC_16BIT_RESULT
  * @retval x-axis ADC value
  */
uint16_t m5_joystick2_get_joy_adc_value_x(m5_joystick2_t *dev, adc_mode_t adc_bits);

/**
  * @brief  get Unit Joystick2 y-axis ADC value
  * @param  dev      device instance
  * @param  adc_bits ADC_8BIT_RESULT or ADC_16BIT_RESULT
  * @retval y-axis ADC value
  */
uint16_t m5_joystick2_get_joy_adc_value_y(m5_joystick2_t *dev, adc_mode_t adc_bits);

/**
  * @brief  get Unit Joystick2 button value
  * @param  dev device instance
  * @retval 0 press, 1 no press
  */
uint8_t m5_joystick2_get_button_value(m5_joystick2_t *dev);

/**
  * @brief  set Unit Joystick2 rgb color
  * @param  dev   device instance
  * @param  color rgb color, 0x00RRGGBB
  */
void m5_joystick2_set_rgb_color(m5_joystick2_t *dev, uint32_t color);

/**
  * @brief  get Unit Joystick2 rgb color
  * @param  dev device instance
  * @retval rgb color, 0x00RRGGBB
  */
uint32_t m5_joystick2_get_rgb_color(m5_joystick2_t *dev);

/**
  * @brief  get Unit Joystick2 mapped cal value
  * @param  dev       device instance
  * @param  x_neg_min pointer of x-axis negative minimum value
  * @param  x_neg_max pointer of x-axis negative maximum value
  * @param  x_pos_min pointer of x-axis positive minimum value
  * @param  x_pos_max pointer of x-axis positive maximum value
  * @param  y_neg_min pointer of y-axis negative minimum value
  * @param  y_neg_max pointer of y-axis negative maximum value
  * @param  y_pos_min pointer of y-axis positive minimum value
  * @param  y_pos_max pointer of y-axis positive maximum value
  */
void m5_joystick2_get_joy_adc_value_cal(m5_joystick2_t *dev, uint16_t *x_neg_min, uint16_t *x_neg_max,
                                        uint16_t *x_pos_min, uint16_t *x_pos_max, uint16_t *y_neg_min,
                                        uint16_t *y_neg_max, uint16_t *y_pos_min, uint16_t *y_pos_max);

/**
  * @brief  set Unit Joystick2 mapped cal value
  * @param  dev       device instance
  * @param  x_neg_min x-axis negative minimum value
  * @param  x_neg_max x-axis negative maximum value
  * @param  x_pos_min x-axis positive minimum value
  * @param  x_pos_max x-axis positive maximum value
  * @param  y_neg_min y-axis negative minimum value
  * @param  y_neg_max y-axis negative maximum value
  * @param  y_pos_min y-axis positive minimum value
  * @param  y_pos_max y-axis positive maximum value
  */
void m5_joystick2_set_joy_adc_value_cal(m5_joystick2_t *dev, uint16_t x_neg_min, uint16_t x_neg_max,
                                        uint16_t x_pos_min, uint16_t x_pos_max, uint16_t y_neg_min,
                                        uint16_t y_neg_max, uint16_t y_pos_min, uint16_t y_pos_max);

/**
  * @brief  get Unit Joystick2 x-axis 12bits mapped value
  * @param  dev device instance
  * @retval x-axis 12bits mapped value
  */
int16_t m5_joystick2_get_joy_adc_12bits_offset_value_x(m5_joystick2_t *dev);

/**
  * @brief  get Unit Joystick2 y-axis 12bits mapped value
  * @param  dev device instance
  * @retval y-axis 12bits mapped value
  */
int16_t m5_joystick2_get_joy_adc_12bits_offset_value_y(m5_joystick2_t *dev);

/**
  * @brief  get Unit Joystick2 x-axis 8bits mapped value
  * @param  dev device instance
  * @retval x-axis 8bits mapped value
  */
int8_t m5_joystick2_get_joy_adc_8bits_offset_value_x(m5_joystick2_t *dev);

/**
  * @brief  get Unit Joystick2 y-axis 8bits mapped value
  * @param  dev device instance
  * @retval y-axis 8bits mapped value
  */
int8_t m5_joystick2_get_joy_adc_8bits_offset_value_y(m5_joystick2_t *dev);

/**
  * @brief  get Unit Joystick2 x-axis and y-axis 16bits ADC value
  * @param  dev   device instance
  * @param  adc_x pointer of x-axis ADC value
  * @param  adc_y pointer of y-axis ADC value
  */
void m5_joystick2_get_joy_adc_16bits_value_xy(m5_joystick2_t *dev, uint16_t *adc_x, uint16_t *adc_y);

/**
  * @brief  get Unit Joystick2 x/y 16bits ADC value, with transfer status
  * @param  dev   device instance
  * @param  adc_x pointer of x-axis ADC value
  * @param  adc_y pointer of y-axis ADC value
  * @retval HAL_OK on success, other value when the unit did not answer
  *
  * @note 相比 m5_joystick2_get_joy_adc_16bits_value_xy()，本函数会把 I2C 读取
  *       结果返回给调用者。上层（app_user.c）据此判断摇杆是否还在线，
  *       避免把一次失败的读取误当成"摇杆打到极值"。
  */
HAL_StatusTypeDef m5_joystick2_read_joy_xy(m5_joystick2_t *dev, uint16_t *adc_x, uint16_t *adc_y);

/**
  * @brief  get Unit Joystick2 x-axis and y-axis 8bits ADC value
  * @param  dev   device instance
  * @param  adc_x pointer of x-axis ADC value
  * @param  adc_y pointer of y-axis ADC value
  */
void m5_joystick2_get_joy_adc_8bits_value_xy(m5_joystick2_t *dev, uint8_t *adc_x, uint8_t *adc_y);

/**
  * @brief  transmit row 16bits joystick data into adjusted version
  * @param  buf   buffer of an uint32_t variable (x 在高 16 位，y 在低 16 位)
  * @param  joy_x x-axis row value
  * @param  joy_y y-axis row value
  */
void m5_joystick2_data_adjust(uint32_t *buf, const uint16_t *joy_x, const uint16_t *joy_y);

#ifdef __cplusplus
}
#endif

#endif /* __M5_UNIT_JOYSTICK2_H__ */
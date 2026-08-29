#include "crsf.h"
#include "app_config.h"
#include "main.h"

extern UART_HandleTypeDef huart4;

#define CRSF_ADDRESS_FLIGHT_CONTROLLER     0xC8U
#define CRSF_FRAMETYPE_RC_CHANNELS_PACKED  0x16U
#define CRSF_PAYLOAD_SIZE                  22U
#define CRSF_FRAME_SIZE                    (1U + 1U + 1U + CRSF_PAYLOAD_SIZE + 1U) /* 26 */

static uint16_t s_channels[APP_CRSF_CHANNEL_COUNT];
static uint8_t  s_frame[CRSF_FRAME_SIZE];

static void CRSF_PackChannels(uint8_t payload[CRSF_PAYLOAD_SIZE])
{
    uint32_t bits           = 0U;
    uint8_t  bits_in_buffer = 0U;
    uint8_t  out_index      = 0U;

    for (uint8_t i = 0U; i < APP_CRSF_CHANNEL_COUNT; i++)
    {
        bits |= (uint32_t)(s_channels[i] & 0x07FFU) << bits_in_buffer;
        bits_in_buffer += 11U;

        while (bits_in_buffer >= 8U)
        {
            payload[out_index++] = (uint8_t)(bits & 0xFFU);
            bits >>= 8U;
            bits_in_buffer -= 8U;
        }
    }
}

uint8_t CRSF_ComputeCrc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0x00U;

    for (uint8_t i = 0U; i < len; i++)
    {
        crc ^= data[i];
        for (uint8_t j = 0U; j < 8U; j++)
        {
            crc = (crc & 0x80U) ? (uint8_t)((crc << 1U) ^ 0xD5U) : (uint8_t)(crc << 1U);
        }
    }
    return crc;
}

void CRSF_Init(void)
{
    for (uint8_t i = 0U; i < APP_CRSF_CHANNEL_COUNT; i++)
    {
        s_channels[i] = APP_CRSF_CHANNEL_VALUE_NEUTRAL;
    }
}

bool CRSF_SetChannel(uint8_t channel, uint16_t value)
{
    if (channel >= APP_CRSF_CHANNEL_COUNT)
    {
        return false;
    }

    if (value > APP_CRSF_CHANNEL_VALUE_MAX)
    {
        value = APP_CRSF_CHANNEL_VALUE_MAX;
    }
    s_channels[channel] = value;
    return true;
}

bool CRSF_Transmit(void)
{
    s_frame[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    s_frame[1] = (uint8_t)(CRSF_FRAME_SIZE - 2U); /* type + payload + crc = 24 */
    s_frame[2] = CRSF_FRAMETYPE_RC_CHANNELS_PACKED;

    CRSF_PackChannels(&s_frame[3]);

    s_frame[CRSF_FRAME_SIZE - 1U] = CRSF_ComputeCrc8(s_frame, CRSF_FRAME_SIZE - 1U);

    /* TODO：若后续希望非阻塞发送，可改为 HAL_UART_Transmit_IT/DMA，
     * 并补 UART4 中断使能与 IRQHandler。当前使用阻塞发送。 */
    return (HAL_UART_Transmit(&huart4, s_frame, CRSF_FRAME_SIZE,
                              APP_CRSF_UART_TIMEOUT_MS) == HAL_OK);
}

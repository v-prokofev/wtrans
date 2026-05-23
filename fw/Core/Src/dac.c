/**
 * @file    dac.c
 * @brief   DAC161S997 driver — two independent 4-20 mA current loop outputs.
 *
 * Hardware connections (from project_spec.md):
 *   SPI1  : PA5=SCK, PA6=MISO, PA7=MOSI  (Full-Duplex Master, 1.125 MHz)
 *   DAC1  : PA4 = nCS  (Speed channel)
 *   DAC2  : PB0 = nCS  (Direction channel)
 *   nERR1 : PB1 = DAC1 error flag (active LOW)
 *   nERR2 : PB2 = DAC2 error flag (active LOW)
 *
 * DAC161S997 protocol:
 *   - 3-byte SPI frames: [Addr(8)] [Data_MSB(8)] [Data_LSB(8)]
 *   - CPOL=0, CPHA=1 (Mode 1)  — see datasheet p.23
 *   - CS active LOW, toggled around each 3-byte transaction
 *
 * Current formula:
 *   I_out = (CODE / 65536) * 24 mA
 *   CODE  = (I_mA / 24.0) * 65536
 */

#include "dac.h"
#include <stddef.h>

/* -------------------------------------------------------------------------
 * Private variables
 * ------------------------------------------------------------------------- */
static SPI_HandleTypeDef *_hspi = NULL;

/* -------------------------------------------------------------------------
 * Private helpers
 * ------------------------------------------------------------------------- */

/** Assert chip-select for the given channel (drive CS LOW). */
static inline void cs_select(uint8_t channel)
{
    if (channel == DAC_CHANNEL_SPEED)
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET); /* PB0 = pin 18 = DAC1_nCS */
    else
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET); /* PA4 = pin 14 = DAC2_nCS */
}

/** Deassert chip-select for the given channel (drive CS HIGH). */
static inline void cs_deselect(uint8_t channel)
{
    if (channel == DAC_CHANNEL_SPEED)
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);   /* PB0 = pin 18 = DAC1_nCS */
    else
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);   /* PA4 = pin 14 = DAC2_nCS */
}

/**
 * @brief  Write a 3-byte SPI frame to the DAC161S997.
 *         Frame layout: [7:0]=addr, [15:8]=data_high, [23:16]=data_low
 */
static void DAC_WriteReg(uint8_t channel, uint8_t addr, uint16_t data)
{
    uint8_t frame[3];
    frame[0] = addr;
    frame[1] = (uint8_t)(data >> 8);
    frame[2] = (uint8_t)(data & 0xFF);

    cs_select(channel);
    HAL_SPI_Transmit(_hspi, frame, 3, 10);
    cs_deselect(channel);
}

/** Convert mA to a 16-bit DAC code. Input is clamped to [0, 24] mA. */
static uint16_t mA_to_code(float mA)
{
    if (mA < 0.0f)  mA = 0.0f;
    if (mA > DAC_CURRENT_FULLSCALE_MA) mA = DAC_CURRENT_FULLSCALE_MA;
    uint32_t code = (uint32_t)((mA / DAC_CURRENT_FULLSCALE_MA) * 65536.0f);
    if (code > 0xFFFF) code = 0xFFFF;
    return (uint16_t)code;
}

/** Configure one DAC channel: enable normal mode, no loopback. */
static void DAC_ConfigChannel(uint8_t channel)
{
    /* RESET register — soft reset */
    DAC_WriteReg(channel, DAC161S997_REG_RESET, 0xC33C);
    HAL_Delay(1);

    /* WR_MODE: normal write mode (0x0000 = direct write, no loopback) */
    DAC_WriteReg(channel, DAC161S997_REG_WR_MODE, 0x0000);

    /* ERR_CONFIG: enable low/high error outputs on nERR pin */
    DAC_WriteReg(channel, DAC161S997_REG_ERR_CONFIG, 0x0001);

    /* Preset to error level until real data arrives */
    DAC_WriteReg(channel, DAC161S997_REG_DACCODE, mA_to_code(DAC_CURRENT_ERROR_MA));
}

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

void DAC_Init(SPI_HandleTypeDef *hspi)
{
    _hspi = hspi;

    /* Both CS lines idle HIGH */
    cs_deselect(DAC_CHANNEL_SPEED);
    cs_deselect(DAC_CHANNEL_DIRECTION);

    HAL_Delay(2); /* Power-on stabilisation */

    DAC_ConfigChannel(DAC_CHANNEL_SPEED);
    DAC_ConfigChannel(DAC_CHANNEL_DIRECTION);
}

void DAC_WriteCode(uint8_t channel, uint16_t code)
{
    if (_hspi == NULL) return;
    DAC_WriteReg(channel, DAC161S997_REG_DACCODE, code);
}

void DAC_SetCurrent(uint8_t channel, float mA)
{
    /* Clamp to valid output range */
    if (mA < DAC_CURRENT_ERROR_MA) mA = DAC_CURRENT_ERROR_MA;
    if (mA > DAC_CURRENT_MAX_MA)   mA = DAC_CURRENT_MAX_MA;
    DAC_WriteCode(channel, mA_to_code(mA));
}

void DAC_UpdateOutputs(float speed_ms, float dir_deg)
{
    /* Speed: 0..60 m/s -> 4..20 mA */
    float spd_mA = DAC_CURRENT_MIN_MA + (speed_ms / 60.0f) * 16.0f;

    /* Direction: 0..360 deg -> 4..20 mA */
    float dir_mA = DAC_CURRENT_MIN_MA + (dir_deg / 360.0f) * 16.0f;

    DAC_SetCurrent(DAC_CHANNEL_SPEED,     spd_mA);
    DAC_SetCurrent(DAC_CHANNEL_DIRECTION, dir_mA);
}

void DAC_SetError(void)
{
    DAC_SetCurrent(DAC_CHANNEL_SPEED,     DAC_CURRENT_ERROR_MA);
    DAC_SetCurrent(DAC_CHANNEL_DIRECTION, DAC_CURRENT_ERROR_MA);
}

uint8_t DAC_ReadErrors(void)
{
    uint8_t err = 0;
    /* nERR pins are active LOW — low = fault */
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_2) == GPIO_PIN_RESET) err |= 0x01; /* DAC1: PB2 = pin 20 = DAC1_nERR */
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == GPIO_PIN_RESET) err |= 0x02; /* DAC2: PB1 = pin 19 = DAC2_nERR */
    return err;
}

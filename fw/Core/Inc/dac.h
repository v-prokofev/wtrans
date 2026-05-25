#ifndef __DAC_H
#define __DAC_H

#include "stm32f1xx_hal.h"

/* DAC161S997 Register addresses */
#define DAC161S997_REG_WR_MODE    0x01
#define DAC161S997_REG_DACCODE    0x04
#define DAC161S997_REG_ERR_CONFIG 0x05
#define DAC161S997_REG_ERR_LOW    0x06
#define DAC161S997_REG_ERR_HIGH   0x07
#define DAC161S997_REG_STATUS     0x08  /* Read-only: chip status flags */
#define DAC161S997_REG_RESET      0x09

/* STATUS register bit masks (read-only) */
#define DAC_STATUS_LOOP_ERR  (1 << 3)  /* Loop open / overload */
#define DAC_STATUS_SPI_TOUT  (1 << 4)  /* SPI timeout */
#define DAC_STATUS_SPI_ERR   (1 << 5)  /* SPI frame error */
#define DAC_STATUS_FERR      (1 << 6)  /* Frame error */
#define DAC_STATUS_CERR      (1 << 7)  /* Checksum error */

/* Current range constants (mA) */
#define DAC_CURRENT_MIN_MA       4.0f
#define DAC_CURRENT_MAX_MA       20.0f
#define DAC_CURRENT_ERROR_MA     3.5f   /* NAMUR NE43 low error */
#define DAC_CURRENT_FULLSCALE_MA 24.0f  /* DAC161S997 full scale */

/* Channel identifiers */
#define DAC_CHANNEL_SPEED     1   /* DAC1: PA4 nCS */
#define DAC_CHANNEL_DIRECTION 2   /* DAC2: PB0 nCS */

/**
 * @brief  Initialize SPI and both DAC channels.
 *         Must be called after MX_SPI1_Init().
 * @param  hspi  Pointer to SPI handle (SPI1)
 */
void DAC_Init(SPI_HandleTypeDef *hspi);

/**
 * @brief  Write a raw 16-bit code to the specified DAC channel.
 * @param  channel  DAC_CHANNEL_SPEED or DAC_CHANNEL_DIRECTION
 * @param  code     16-bit DAC code (0x0000..0xFFFF)
 */
void DAC_WriteCode(uint8_t channel, uint16_t code);

/**
 * @brief  Set output current on a DAC channel.
 * @param  channel  DAC_CHANNEL_SPEED or DAC_CHANNEL_DIRECTION
 * @param  mA       Desired current in mA (clamped to 3.5..20 mA)
 */
void DAC_SetCurrent(uint8_t channel, float mA);

/**
 * @brief  Convert a physical value to mA and update the DAC.
 * @param  speed_ms   Wind speed, m/s  (0..60)   -> 4..20 mA
 * @param  dir_deg    Wind direction, deg (0..360) -> 4..20 mA
 */
void DAC_UpdateOutputs(float speed_ms, float dir_deg);

/**
 * @brief  Set both channels to NAMUR NE43 low-error level (3.5 mA).
 */
void DAC_SetError(void);

/**
 * @brief  Read nERR pins.
 * @retval 0 = no error, non-zero = HW fault detected (bit0=DAC1, bit1=DAC2)
 */
uint8_t DAC_ReadErrors(void);

/**
 * @brief  Read the STATUS register from a DAC channel via SPI.
 *         Sends a NOOP (all-zeros) to clock out the status word.
 * @param  channel  DAC_CHANNEL_SPEED or DAC_CHANNEL_DIRECTION
 * @retval 16-bit STATUS register value (see DAC_STATUS_* masks)
 */
uint16_t DAC_ReadStatus(uint8_t channel);

/**
 * @brief  Periodic keepalive — re-write last DAC codes to prevent SPI timeout.
 *         Call from main loop every ~50 ms.
 */
void DAC_Refresh(void);

#endif /* __DAC_H */

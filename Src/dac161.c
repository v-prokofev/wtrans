#include "stm32f103xb.h"
#include <stdint.h>
#include "dac161.h"

/**
 * @brief  Initialize GPIOs and SPI1 for DAC communication
 */
void DAC_Init(void) {
    // 1. Enable Clocks
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_SPI1EN;

    // 2. Configure Pins
    // PA4 (CS1), PB0 (CS2) as General Purpose Output Push-Pull (Max 50MHz) -> 0x3
    GPIOA->CRL &= ~(0xF << 16); // Clear PA4
    GPIOA->CRL |=  (0x3 << 16); // PA4 Output PP 50MHz
    GPIOA->BSRR = (1 << 4);     // Set CS1 High

    GPIOB->CRL &= ~(0xF << 0);  // Clear PB0
    GPIOB->CRL |=  (0x3 << 0);  // PB0 Output PP 50MHz
    GPIOB->BSRR = (1 << 0);     // Set CS2 High

    // PA5 (SCK), PA7 (MOSI) as Alternate Function Push-Pull -> 0xB
    GPIOA->CRL &= ~(0xF << 20); // Clear PA5
    GPIOA->CRL |=  (0xB << 20); // PA5 AF-PP 50MHz
    GPIOA->CRL &= ~(0xF << 28); // Clear PA7
    GPIOA->CRL |=  (0xB << 28); // PA7 AF-PP 50MHz

    // PA6 (MISO) as Input Floating (Default) or Input Pull-up
    GPIOA->CRL &= ~(0xF << 24); // Clear PA6
    GPIOA->CRL |=  (0x4 << 24); // PA6 Input Floating

    // 3. Configure SPI1
    // Master, Mode 0 (CPOL=0, CPHA=0), MSB First, 8-bit Data
    // BaudRate = PCLK2 / 16 (72MHz / 16 = 4.5MHz) -> 0x3 << 3
    SPI1->CR1 = SPI_CR1_MSTR | (0x3 << 3);
    SPI1->CR1 |= SPI_CR1_SPE; // Enable SPI
}

/**
 * @brief  Send a 24-bit frame to the selected DAC
 * @param  cs_mask: BSRR mask for CS pin (bit 4 for PA4, etc)
 * @param  is_pa: 1 for GPIOA, 0 for GPIOB
 * @param  reg: 8-bit register address
 * @param  data: 16-bit data
 */
static void DAC_WriteRegister(uint32_t cs_pin, int is_pa, uint8_t reg, uint16_t data) {
    // Select DAC (CS Low)
    if (is_pa) GPIOA->BSRR = (cs_pin << 16);
    else       GPIOB->BSRR = (cs_pin << 16);

    // Send 8-bit address
    while (!(SPI1->SR & SPI_SR_TXE));
    SPI1->DR = reg;
    while (!(SPI1->SR & SPI_SR_RXNE));
    (void)SPI1->DR;

    // Send high byte of data
    while (!(SPI1->SR & SPI_SR_TXE));
    SPI1->DR = (uint8_t)(data >> 8);
    while (!(SPI1->SR & SPI_SR_RXNE));
    (void)SPI1->DR;

    // Send low byte of data
    while (!(SPI1->SR & SPI_SR_TXE));
    SPI1->DR = (uint8_t)(data & 0xFF);
    while (!(SPI1->SR & SPI_SR_RXNE));
    (void)SPI1->DR;

    // Wait for BSY to clear
    while (SPI1->SR & SPI_SR_BSY);

    // Deselect DAC (CS High)
    if (is_pa) GPIOA->BSRR = cs_pin;
    else       GPIOB->BSRR = cs_pin;
}

/**
 * @brief  Update the DAC output current code
 * @param  channel: 1 for Speed (PA4), 2 for Direction (PB0)
 * @param  code: 16-bit DAC code
 */
void DAC_UpdateCode(int channel, uint16_t code) {
    // Register 0x01 is DACCODE per DAC161S997 datasheet
    if (channel == 1) {
        // DAC1_nCS is PB0
        DAC_WriteRegister(1 << 0, 0, 0x01, code);
    } else {
        // DAC2_nCS is PA4
        DAC_WriteRegister(1 << 4, 1, 0x01, code);
    }
}

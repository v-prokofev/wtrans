#include "stm32f103xb.h"
#include "led.h"

/**
 * @brief Initialize the LED pin (PB3)
 */
void LED_Init(void) {
    // 1. Enable GPIOB and AFIO clocks
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | (1 << 0); // (1 << 0) is AFIOEN

    // 2. Disable JTAG but keep SWD to release PB3 from JTDO
    // AFIO_MAPR SWJ_CFG[2:0] = 010 (JTAG-DP Disabled and SW-DP Enabled)
    AFIO->MAPR = (AFIO->MAPR & ~0x07000000) | 0x02000000;

    // 3. Configure PB3 as General Purpose Output Push-Pull, max speed 2 MHz
    // PB3 configuration is in GPIOB_CRL, bits [15:12]
    // CNF3 = 00 (Push-Pull), MODE3 = 10 (Output 2MHz) -> 0x2
    GPIOB->CRL &= ~(0xF << 12);
    GPIOB->CRL |=  (0x2 << 12);
}

/**
 * @brief Turn the LED on
 */
void LED_On(void) {
    GPIOB->BSRR = (1 << 3); // Set PB3
}

/**
 * @brief Turn the LED off
 */
void LED_Off(void) {
    GPIOB->BSRR = (1 << 19); // Reset PB3 (BR3)
}

/**
 * @brief Toggle the LED state
 */
void LED_Toggle(void) {
    GPIOB->ODR ^= (1 << 3);
}

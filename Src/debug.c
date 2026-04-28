#include "stm32f103xb.h"
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include "debug.h"

/**
 * @brief  Initialize UART3 for Debug communication (X3)
 */
void Debug_Init(void) {
    // 1. Enable Clocks
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART3EN;

    // 2. Configure Pins
    // PB10 (TX) as AF-PP -> 0xB
    GPIOB->CRH &= ~(0xF << 8);
    GPIOB->CRH |=  (0xB << 8);
    
    // PB11 (RX) as Input Pull-up -> 0x8
    GPIOB->CRH &= ~(0xF << 12);
    GPIOB->CRH |=  (0x8 << 12);
    GPIOB->ODR |= (1 << 11);

    // PB12 (nRE), PB13 (DE) as GP Output PP -> 0x3
    GPIOB->CRH &= ~(0xFF << 16);
    GPIOB->CRH |=  (0x33 << 16);
    // Set to Transmit mode for debug
    GPIOB->BSRR = (1 << 13) | (1 << 12); // DE=1, nRE=1

    // 3. Configure UART3 (115200 bps @ 36MHz)
    // BRR = 36000000 / 115200 = 312.5 = 0x138 (approx)
    USART3->BRR = 0x138;
    USART3->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

/**
 * @brief  Send character to UART3
 */
int __io_putchar(int ch) {
    // Output to UART3
    while (!(USART3->SR & USART_SR_TXE));
    USART3->DR = (uint8_t)ch;
    return ch;
}

/**
 * @brief  Send string to Debug outputs
 */
void Debug_Print(const char *str) {
    while (*str) {
        __io_putchar(*str++);
    }
}

/**
 * @brief  Formatted print for debug
 */
void Debug_Printf(const char *fmt, ...) {
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Debug_Print(buf);
}

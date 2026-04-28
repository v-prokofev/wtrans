#include "stm32f103xb.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "sensor.h"

static char dma_rx_buffer[128];

/**
 * @brief  Initialize UART2 and DMA1 for RS-485 Sensor communication
 */
void Sensor_Init(void) {
    // 1. Enable Clocks
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    RCC->AHBENR  |= RCC_AHBENR_DMA1EN;

    // 2. Configure Pins
    // PA0 (nRE), PA1 (DE) as GP Output PP
    GPIOA->CRL &= ~(0xFF << 0);
    GPIOA->CRL |=  (0x33 << 0);
    GPIOA->BSRR = (1 << 0) | (1 << 16); // nRE=1, DE=0

    // PA2 (TX) as AF-PP, PA3 (RX) as Input Pull-up
    GPIOA->CRL &= ~(0xF << 8);
    GPIOA->CRL |=  (0xB << 8);
    GPIOA->CRL &= ~(0xF << 12);
    GPIOA->CRL |=  (0x8 << 12);
    GPIOA->ODR |= (1 << 3);

    // 3. Configure UART2 (9600 @ 36MHz)
    USART2->BRR = 0xEA6;
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
    USART2->CR3 |= USART_CR3_DMAR; // Enable DMA Receiver

    // 4. Configure DMA1 Channel 6
    DMA1_Channel6->CPAR = (uint32_t)&USART2->DR;
    DMA1_Channel6->CMAR = (uint32_t)dma_rx_buffer;
    // Circular mode (0x20), Memory increment (0x80), Enable (0x01)
    DMA1_Channel6->CCR = 0x20 | 0x80; 
}

void RS485_SetTransmitter(int active) {
    if (active) {
        GPIOA->BSRR = (1 << 1) | (1 << 0);          // DE = 1, nRE = 1
    } else {
        GPIOA->BSRR = (1 << 17) | (1 << 16);        // DE = 0, nRE = 0 (Receive)
    }
}

void Sensor_StartDMA(void) {
    DMA1_Channel6->CCR &= ~0x01; // Disable
    DMA1_Channel6->CNDTR = 128;  // Reset count
    memset(dma_rx_buffer, 0, 128);
    DMA1_Channel6->CCR |= 0x01;  // Enable
}

void Sensor_SendCommand(const char *cmd) {
    RS485_SetTransmitter(1);
    while (*cmd) {
        while (!(USART2->SR & USART_SR_TXE));
        USART2->DR = *cmd++;
    }
    while (!(USART2->SR & (1 << 6))); // TC
    RS485_SetTransmitter(0);
}

char* Sensor_GetBuffer(void) {
    return dma_rx_buffer;
}

int Sensor_ParseResponse(char *buf, float *p_spd, float *p_dir) {
    // Find '@' in buffer
    char *start = strchr(buf, '@');
    if (!start) return -1;

    char local_buf[128];
    strncpy(local_buf, start, 127);
    local_buf[127] = '\0';

    char *token;
    int index = 0;
    token = strtok(local_buf, " ");
    while (token != NULL) {
        if (index == 1) { // status
            if (atoi(token) != 0) return -1;
        } else if (index == 2) { // spd_act
            *p_spd = (float)atof(token);
        } else if (index == 6) { // dir_act
            *p_dir = (float)atof(token);
            return 0;
        }
        token = strtok(NULL, " ");
        index++;
    }
    return -1;
}

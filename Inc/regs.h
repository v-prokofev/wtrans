#ifndef REGS_H
#define REGS_H

#include <stdint.h>

/* --- STM32F103C8 Memory Map --- */
#define FLASH_BASE          0x08000000UL
#define SRAM_BASE           0x20000000UL
#define PERIPH_BASE         0x40000000UL

#define APB1_BASE           PERIPH_BASE
#define APB2_BASE           (PERIPH_BASE + 0x00010000UL)
#define AHB_BASE            (PERIPH_BASE + 0x00020000UL)

/* --- DMA Registers --- */
#define DMA1_BASE           (AHB_BASE + 0x0000UL)
#define DMA1_ISR            (*(volatile uint32_t *)(DMA1_BASE + 0x00))
#define DMA1_IFCR           (*(volatile uint32_t *)(DMA1_BASE + 0x04))

// Channel 6 (for UART2_RX)
#define DMA1_CH6_BASE       (DMA1_BASE + 0x08 + (5 * 20)) // Channel indices are 0-based in some docs, but usually we use 1-7
// Let's use explicit channel offsets: Ch1=0x08, Ch2=0x1C, Ch3=0x30, Ch4=0x44, Ch5=0x58, Ch6=0x6C, Ch7=0x80
#define DMA1_CCR6           (*(volatile uint32_t *)(DMA1_BASE + 0x6C))
#define DMA1_CNDTR6         (*(volatile uint32_t *)(DMA1_BASE + 0x70))
#define DMA1_CPAR6          (*(volatile uint32_t *)(DMA1_BASE + 0x74))
#define DMA1_CMAR6          (*(volatile uint32_t *)(DMA1_BASE + 0x78))

/* --- RCC Registers --- */
#define RCC_BASE            (AHB_BASE + 0x1000UL)
#define RCC_CR              (*(volatile uint32_t *)(RCC_BASE + 0x00))
#define RCC_CFGR            (*(volatile uint32_t *)(RCC_BASE + 0x04))
#define RCC_APB2ENR         (*(volatile uint32_t *)(RCC_BASE + 0x18))
#define RCC_APB1ENR         (*(volatile uint32_t *)(RCC_BASE + 0x1C))

#define RCC_APB2ENR_IOPAEN  (1 << 2)
#define RCC_APB2ENR_IOPBEN  (1 << 3)
#define RCC_APB2ENR_SPI1EN  (1 << 12)
#define RCC_APB1ENR_UART2EN (1 << 17)
#define RCC_APB1ENR_UART3EN (1 << 18)
#define RCC_AHBENR_DMA1EN   (1 << 0)
#define RCC_AHBENR          (*(volatile uint32_t *)(RCC_BASE + 0x14))

/* --- AFIO Registers --- */
#define AFIO_BASE           (APB2_BASE + 0x0000UL)
#define AFIO_MAPR           (*(volatile uint32_t *)(AFIO_BASE + 0x04))

/* --- GPIO Registers --- */
#define GPIOA_BASE          (APB2_BASE + 0x0800UL)
#define GPIOA_CRL           (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_CRH           (*(volatile uint32_t *)(GPIOA_BASE + 0x04))
#define GPIOA_IDR           (*(volatile uint32_t *)(GPIOA_BASE + 0x08))
#define GPIOA_ODR           (*(volatile uint32_t *)(GPIOA_BASE + 0x0C))
#define GPIOA_BSRR          (*(volatile uint32_t *)(GPIOA_BASE + 0x10))

#define GPIOB_BASE          (APB2_BASE + 0x0C00UL)
#define GPIOB_CRL           (*(volatile uint32_t *)(GPIOB_BASE + 0x00))
#define GPIOB_CRH           (*(volatile uint32_t *)(GPIOB_BASE + 0x04))
#define GPIOB_IDR           (*(volatile uint32_t *)(GPIOB_BASE + 0x08))
#define GPIOB_ODR           (*(volatile uint32_t *)(GPIOB_BASE + 0x0C))
#define GPIOB_BSRR          (*(volatile uint32_t *)(GPIOB_BASE + 0x10))

/* --- UART2 Registers --- */
#define UART2_BASE          (APB1_BASE + 0x4400UL)
#define UART2_SR            (*(volatile uint32_t *)(UART2_BASE + 0x00))
#define UART2_DR            (*(volatile uint32_t *)(UART2_BASE + 0x04))
#define UART2_BRR           (*(volatile uint32_t *)(UART2_BASE + 0x08))
#define UART2_CR1           (*(volatile uint32_t *)(UART2_BASE + 0x0C))

#define UART_SR_TXE         (1 << 7)
#define UART_SR_RXNE        (1 << 5)
#define UART_CR1_UE         (1 << 13)
#define UART_CR1_TE         (1 << 3)
#define UART_CR1_RE         (1 << 2)
#define UART_CR3_DMAR       (1 << 6)

/* --- UART3 Registers --- */
#define UART3_BASE          (APB1_BASE + 0x4800UL)
#define UART3_SR            (*(volatile uint32_t *)(UART3_BASE + 0x00))
#define UART3_DR            (*(volatile uint32_t *)(UART3_BASE + 0x04))
#define UART3_BRR           (*(volatile uint32_t *)(UART3_BASE + 0x08))
#define UART3_CR1           (*(volatile uint32_t *)(UART3_BASE + 0x0C))

/* --- SPI1 Registers --- */
#define SPI1_BASE           (APB2_BASE + 0x3000UL)
#define SPI1_CR1            (*(volatile uint32_t *)(SPI1_BASE + 0x00))
#define SPI1_SR             (*(volatile uint32_t *)(SPI1_BASE + 0x08))
#define SPI1_DR             (*(volatile uint32_t *)(SPI1_BASE + 0x0C))

#define SPI_CR1_MSTR        (1 << 2)
#define SPI_CR1_SPE         (1 << 6)
#define SPI_SR_TXE          (1 << 1)
#define SPI_SR_RXNE         (1 << 0)
#define SPI_SR_BSY          (1 << 7)

/* --- SysTick Registers --- */
#define SYSTICK_BASE        0xE000E010UL
#define SYSTICK_CSR         (*(volatile uint32_t *)(SYSTICK_BASE + 0x00))
#define SYSTICK_RVR         (*(volatile uint32_t *)(SYSTICK_BASE + 0x04))
#define SYSTICK_CVR         (*(volatile uint32_t *)(SYSTICK_BASE + 0x08))

#endif // REGS_H

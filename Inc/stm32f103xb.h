#ifndef __STM32F103xB_H
#define __STM32F103xB_H

#include <stdint.h>

#define __IO volatile

typedef struct {
  __IO uint32_t CR;
  __IO uint32_t CFGR;
  __IO uint32_t CIR;
  __IO uint32_t APB2RSTR;
  __IO uint32_t APB1RSTR;
  __IO uint32_t AHBENR;
  __IO uint32_t APB2ENR;
  __IO uint32_t APB1ENR;
  __IO uint32_t BDCR;
  __IO uint32_t CSR;
} RCC_TypeDef;

typedef struct {
  __IO uint32_t CRL;
  __IO uint32_t CRH;
  __IO uint32_t IDR;
  __IO uint32_t ODR;
  __IO uint32_t BSRR;
  __IO uint32_t BRR;
  __IO uint32_t LCKR;
} GPIO_TypeDef;

typedef struct {
  __IO uint32_t SR;
  __IO uint32_t DR;
  __IO uint32_t BRR;
  __IO uint32_t CR1;
  __IO uint32_t CR2;
  __IO uint32_t CR3;
  __IO uint32_t GTPR;
} USART_TypeDef;

typedef struct {
  __IO uint32_t CR1;
  __IO uint32_t CR2;
  __IO uint32_t SR;
  __IO uint32_t DR;
  __IO uint32_t CRCPR;
  __IO uint32_t RXCRCR;
  __IO uint32_t TXCRCR;
  __IO uint32_t I2SCFGR;
  __IO uint32_t I2SPR;
} SPI_TypeDef;

typedef struct {
  __IO uint32_t CCR;
  __IO uint32_t CNDTR;
  __IO uint32_t CPAR;
  __IO uint32_t CMAR;
} DMA_Channel_TypeDef;

typedef struct {
  __IO uint32_t EVCR;
  __IO uint32_t MAPR;
  __IO uint32_t EXTICR[4];
  __IO uint32_t MAPR2;  
} AFIO_TypeDef;

typedef struct {
  __IO uint32_t CTRL;
  __IO uint32_t LOAD;
  __IO uint32_t VAL;
  __IO uint32_t CALIB;
} SysTick_Type;

#define PERIPH_BASE           0x40000000UL
#define APB1PERIPH_BASE       PERIPH_BASE
#define APB2PERIPH_BASE       (PERIPH_BASE + 0x10000UL)
#define AHBPERIPH_BASE        (PERIPH_BASE + 0x20000UL)

#define AFIO_BASE             (APB2PERIPH_BASE + 0x0000UL)
#define GPIOA_BASE            (APB2PERIPH_BASE + 0x0800UL)
#define GPIOB_BASE            (APB2PERIPH_BASE + 0x0C00UL)
#define SPI1_BASE             (APB2PERIPH_BASE + 0x3000UL)
#define USART2_BASE           (APB1PERIPH_BASE + 0x4400UL)
#define USART3_BASE           (APB1PERIPH_BASE + 0x4800UL)
#define RCC_BASE              (AHBPERIPH_BASE + 0x1000UL)
#define DMA1_BASE             (AHBPERIPH_BASE + 0x0000UL)
#define DMA1_Channel6_BASE    (DMA1_BASE + 0x08 + 0x14 * 5) // 0x6C

#define SCS_BASE              (0xE000E000UL)
#define SysTick_BASE          (SCS_BASE +  0x0010UL)

#define RCC                   ((RCC_TypeDef *) RCC_BASE)
#define GPIOA                 ((GPIO_TypeDef *) GPIOA_BASE)
#define GPIOB                 ((GPIO_TypeDef *) GPIOB_BASE)
#define SPI1                  ((SPI_TypeDef *) SPI1_BASE)
#define USART2                ((USART_TypeDef *) USART2_BASE)
#define USART3                ((USART_TypeDef *) USART3_BASE)
#define DMA1_Channel6         ((DMA_Channel_TypeDef *) DMA1_Channel6_BASE)
#define AFIO                  ((AFIO_TypeDef *) AFIO_BASE)
#define SysTick               ((SysTick_Type *) SysTick_BASE)

// Bits
#define RCC_APB2ENR_IOPAEN  (1 << 2)
#define RCC_APB2ENR_IOPBEN  (1 << 3)
#define RCC_APB2ENR_SPI1EN  (1 << 12)
#define RCC_APB1ENR_USART2EN (1 << 17)
#define RCC_APB1ENR_USART3EN (1 << 18)
#define RCC_AHBENR_DMA1EN   (1 << 0)

#define USART_SR_TXE         (1 << 7)
#define USART_SR_RXNE        (1 << 5)
#define USART_SR_TC          (1 << 6)
#define USART_CR1_UE         (1 << 13)
#define USART_CR1_TE         (1 << 3)
#define USART_CR1_RE         (1 << 2)
#define USART_CR3_DMAR       (1 << 6)

#define SPI_CR1_MSTR        (1 << 2)
#define SPI_CR1_SPE         (1 << 6)
#define SPI_SR_TXE          (1 << 1)
#define SPI_SR_RXNE         (1 << 0)
#define SPI_SR_BSY          (1 << 7)

#endif

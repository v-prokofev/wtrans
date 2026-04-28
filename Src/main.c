#include "stm32f103xb.h"
#include <stdint.h>
#include <stdio.h>

#include "dac161.h"
#include "sensor.h"
#include "debug.h"
#include "led.h"
#include <string.h>

#define CODE_4MA     10922
#define CODE_20MA    54613
#define CODE_3_5MA   9557
#define MAX_SPEED    60.0f
#define MAX_DIR      360.0f

volatile uint32_t sys_tick = 0;
volatile int sensor_ok = 0;
volatile uint32_t last_poll_time = 0;
volatile int sensor_state = 0; // 0: Idle, 1: Waiting for response

void SysTick_Handler(void) {
    sys_tick++;
}

/**
 * @brief  System Clock Configuration for 16MHz HSE -> 72MHz SYSCLK
 */
void SystemClock_Config(void) {
    // 1. Enable HSE
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY));

    // 2. Configure Flash Latency (2 wait states for 72MHz)
    FLASH->ACR |= FLASH_ACR_LATENCY_2 | FLASH_ACR_PRFTBE;

    // 3. Configure PLL: HSE/2 * 9 = 16/2 * 9 = 72MHz
    RCC->CFGR |= RCC_CFGR_PLLSRC_HSE | RCC_CFGR_PLLXTPRE_HSE_DIV2 | RCC_CFGR_PLLMULL9;
    
    // 4. Configure Bus Prescalers: HCLK=72MHz, PCLK2=72MHz, PCLK1=36MHz
    RCC->CFGR |= RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE2_DIV1 | RCC_CFGR_PPRE1_DIV2;

    // 5. Enable PLL
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    // 6. Select PLL as system clock source
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS_PLL) != RCC_CFGR_SWS_PLL);
}

void SysTick_Init(void) {
    // 72MHz core clock, 1ms interrupt
    SysTick->LOAD = 72000 - 1;
    SysTick->VAL = 0;
    SysTick->CTRL = 0x07; // Enable, TICKINT, CLKSOURCE=CPU
}

uint32_t GetTick(void) {
    return sys_tick;
}

void DelayMs(uint32_t ms) {
    uint32_t start = GetTick();
    while ((GetTick() - start) < ms);
}

uint16_t MapValueToDAC(float val, float max_val) {
    if (val < 0.0f) val = 0.0f;
    if (val > max_val) val = max_val;
    return (uint16_t)(CODE_4MA + (val / max_val) * (CODE_20MA - CODE_4MA));
}

/**
 * @brief  TIM2 Handler (High Priority): Main Processing Logic
 */
void TIM2_IRQHandler(void) {
    if (TIM2->SR & TIM_SR_UIF) {
        TIM2->SR &= ~TIM_SR_UIF;

        uint32_t now = GetTick();
        
        // 1. Sensor Polling Logic
        if (sensor_state == 0) {
            if (now - last_poll_time >= 1000) { // 1 Hz polling rate
                last_poll_time = now;
                Sensor_StartDMA();
                Sensor_SendCommand("@1 MES\r\n");
                sensor_state = 1;
            }
        } else if (sensor_state == 1) {
            char *buf = Sensor_GetBuffer();
            // Check if we received a newline (end of response) or timed out (200ms)
            if (strchr(buf, '\n') != NULL || (now - last_poll_time >= 200)) {
                float speed = 0, direction = 0;
                
                if (Sensor_ParseResponse(buf, &speed, &direction) == 0) {
                    sensor_ok = 1;
                    DAC_UpdateCode(1, MapValueToDAC(speed, MAX_SPEED));
                    DAC_UpdateCode(2, MapValueToDAC(direction, MAX_DIR));
                    Debug_Printf("Data: SPD=%.2f, DIR=%.1f\r\n", speed, direction);
                } else {
                    sensor_ok = 0;
                    DAC_UpdateCode(1, CODE_3_5MA);
                    DAC_UpdateCode(2, CODE_3_5MA);
                    Debug_Printf("Error: No data or status error\r\n");
                }
                sensor_state = 0; // Return to idle state
            }
        }
    }
}

/**
 * @brief  TIM3 Handler (Low Priority): LED Indication
 */
void TIM3_IRQHandler(void) {
    if (TIM3->SR & TIM_SR_UIF) {
        TIM3->SR &= ~TIM_SR_UIF;

        static uint32_t led_cnt = 0;
        uint32_t led_period = sensor_ok ? 20 : 100; // 200ms or 1000ms at 10ms tick
        if (++led_cnt >= led_period) {
            led_cnt = 0;
            LED_Toggle();
        }
    }
}

void Timers_Init(void) {
    // Enable Clocks
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN | RCC_APB1ENR_TIM3EN;

    // Configure TIM2 (High Priority, 10ms tick)
    TIM2->PSC = 36000 - 1; // 1kHz
    TIM2->ARR = 10 - 1;    // 100Hz (10ms)
    TIM2->DIER |= TIM_DIER_UIE;
    TIM2->CR1 |= TIM_CR1_CEN;

    // Configure TIM3 (Low Priority, 10ms tick)
    TIM3->PSC = 36000 - 1;
    TIM3->ARR = 10 - 1;
    TIM3->DIER |= TIM_DIER_UIE;
    TIM3->CR1 |= TIM_CR1_CEN;

    // NVIC Priorities: TIM2 (0 - Highest), TIM3 (1 - Lower)
    NVIC->IP[TIM2_IRQn] = (0 << 4);
    NVIC->IP[TIM3_IRQn] = (1 << 4);

    // Enable IRQs
    NVIC->ISER[TIM2_IRQn >> 5] = (1 << (TIM2_IRQn & 0x1F));
    NVIC->ISER[TIM3_IRQn >> 5] = (1 << (TIM3_IRQn & 0x1F));
}

int main(void) {
    SystemClock_Config();
    SysTick_Init();
    DAC_Init();
    Sensor_Init();
    Debug_Init();
    LED_Init();
    Timers_Init();
    
    Debug_Printf("--- DVU-01 Converter Started (IRQ mode) ---\r\n");
    
    while (1) {
        // Main loop is empty, all logic in IRQs
        __asm("WFI");
    }
}

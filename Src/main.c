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

void SysTick_Handler(void) {
    sys_tick++;
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

int main(void) {
    SysTick_Init();
    DAC_Init();
    Sensor_Init();
    Debug_Init();
    LED_Init();
    
    Debug_Printf("--- DVU-01 Converter Started ---\r\n");
    
    int sensor_ok = 0;
    uint32_t last_poll_time = 0;
    uint32_t led_toggle_time = 0;
    int sensor_state = 0; // 0: Idle, 1: Waiting for response
    
    while (1) {
        uint32_t now = GetTick();
        
        // 1. Sensor Polling Task
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
        
        // 2. LED Indication Task
        // sensor_ok: Fast blink (toggle every 200ms)
        // error: Slow blink (toggle every 1000ms)
        uint32_t led_period = sensor_ok ? 200 : 1000;
        if (now - led_toggle_time >= led_period) {
            led_toggle_time = now;
            LED_Toggle();
        }
    }
}

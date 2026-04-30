#include "sensor.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void Sensor_Init(Sensor_t *sensor, uint8_t id) {
    sensor->id = id;
    sensor->state = SENSOR_INIT_STOP_AMES;
    sensor->speed = 0.0f;
    sensor->direction = 0.0f;
    sensor->last_sync = 0;
}

// Internal helper for RS-485 transmit
static void Sensor_Send(UART_HandleTypeDef *huart, char *cmd) {
    extern UART_HandleTypeDef huart3;
    // Debug TX
    HAL_UART_Transmit(&huart3, (uint8_t *)"TX: ", 4, 10);
    HAL_UART_Transmit(&huart3, (uint8_t *)cmd, strlen(cmd), 10);

    // Switch to TX
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);   // DE = 1
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);   // nRE = 1
    
    HAL_UART_Transmit(huart, (uint8_t*)cmd, strlen(cmd), 100);
    
    // Wait for TC
    uint32_t timeout = 1000;
    while(__HAL_UART_GET_FLAG(huart, UART_FLAG_TC) == RESET && timeout--);
    
    // Switch to RX
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET); // DE = 0
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET); // nRE = 0
}

int Sensor_Parse(Sensor_t *sensor, char *buffer) {
    extern UART_HandleTypeDef huart3;
    if (!buffer || strlen(buffer) < 5) return 0;

    char *parse_ptr = NULL;
    
    // Option A: Response with @id:0 prefix (Network mode)
    char *colon_ptr = strchr(buffer, ':');
    if (colon_ptr != NULL) {
        parse_ptr = colon_ptr + 2; 
    } else {
        // Option B: Response without prefix (Command mode)
        char *ptr = buffer;
        while (*ptr && !(*ptr >= '0' && *ptr <= '9' || *ptr == '-')) {
            ptr++;
        }
        if (*ptr) {
            parse_ptr = ptr;
        }
    }

    if (parse_ptr != NULL) {
        char *endptr;
        // 1. Status (int)
        long status = strtol(parse_ptr, &endptr, 10);
        if (endptr == parse_ptr) return 0;
        
        // 2. spd_act
        float s_act = strtof(endptr, &endptr);
        // 3. spd_avg
        float s_avg = strtof(endptr, &endptr);
        // 4. spd_max
        float s_max = strtof(endptr, &endptr);
        // 5. spd_min
        float s_min = strtof(endptr, &endptr);
        // 6. spd_std
        float s_std = strtof(endptr, &endptr);
        // 7. dir_act
        float d_act = strtof(endptr, &endptr);
        
        // If we reached here and endptr moved, we at least have some data
        if (endptr != parse_ptr) {
            sensor->speed = s_act;
            sensor->direction = d_act;
            return 1;
        }
    }

    // Option C: Spd= Dir= debug line
    char *spd_ptr = strstr(buffer, "Spd=");
    char *dir_ptr = strstr(buffer, "Dir=");
    if (spd_ptr && dir_ptr) {
        if (sscanf(spd_ptr, "Spd=%f", &sensor->speed) == 1 && 
            sscanf(dir_ptr, "Dir=%f", &sensor->direction) == 1) {
            return 1;
        }
    }

    // Option D: Version response check (for init)
    if (sensor->state == SENSOR_INIT_GET_VER) {
        if (strstr(buffer, "DVU") || strstr(buffer, "VER")) {
            return 2; 
        }
    }

    return 0;
}

void Sensor_Step(Sensor_t *sensor, UART_HandleTypeDef *huart) {
    char cmd[32];
    static uint32_t last_cmd_time = 0;
    uint32_t now = HAL_GetTick();

    if (now - last_cmd_time < 500) return;
    last_cmd_time = now;
    
    switch (sensor->state) {
        case SENSOR_INIT_STOP_AMES:
            snprintf(cmd, sizeof(cmd), "@%d AMES 0 0\r\n", sensor->id);
            Sensor_Send(huart, cmd);
            sensor->state = SENSOR_INIT_OPEN;
            sensor->last_sync = now;
            break;

        case SENSOR_INIT_OPEN:
            if (now - sensor->last_sync > 2000) {
                sensor->state = SENSOR_INIT_STOP_AMES;
            } else {
                snprintf(cmd, sizeof(cmd), "OPEN %d\r\n", sensor->id);
                Sensor_Send(huart, cmd);
                sensor->state = SENSOR_INIT_GET_VER;
            }
            break;

        case SENSOR_INIT_GET_VER:
            if (now - sensor->last_sync > 4000) { // Longer timeout for session + ver
                sensor->state = SENSOR_INIT_STOP_AMES;
            } else {
                snprintf(cmd, sizeof(cmd), "VER\r\n");
                Sensor_Send(huart, cmd);
            }
            break;

        case SENSOR_READY_POLL:
            snprintf(cmd, sizeof(cmd), "M\r\n");
            Sensor_Send(huart, cmd);
            break;
            
        case SENSOR_ERROR:
            sensor->state = SENSOR_INIT_STOP_AMES;
            break;
    }
}

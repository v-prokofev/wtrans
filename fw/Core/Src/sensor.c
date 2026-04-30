#include "sensor.h"
#include <string.h>
#include <stdio.h>

void Sensor_Init(Sensor_t *sensor, uint8_t id) {
    sensor->id = id;
    sensor->state = SENSOR_INIT_STOP_AMES;
    sensor->speed = 0.0f;
    sensor->direction = 0.0f;
    sensor->last_sync = 0;
}

// Internal helper for RS-485 transmit
static void Sensor_Send(UART_HandleTypeDef *huart, char *cmd) {
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
    if (!buffer || strlen(buffer) < 5) return 0;

    char *parse_ptr = NULL;
    
    // Option A: Response with @id:0 prefix (Network mode)
    char *colon_ptr = strchr(buffer, ':');
    if (colon_ptr != NULL) {
        parse_ptr = colon_ptr + 2; // Skip ":0 "
    } else if (buffer[0] >= '0' && buffer[0] <= '9') {
        // Option B: Response without prefix (Command mode / Session opened)
        parse_ptr = buffer;
    }

    if (parse_ptr != NULL) {
        int status;
        float s_act, s_avg, s_max, s_min, s_unused;
        float d_act, d_avg, d_max, d_min, d_unused;
        
        // Try parsing 10 fields (standard DVU message)
        if (sscanf(parse_ptr, "%d %f %f %f %f %f %f %f %f %f", 
                   &status, &s_act, &s_avg, &s_max, &s_min, &s_unused, &d_act, &d_avg, &d_max, &d_min) >= 7) {
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

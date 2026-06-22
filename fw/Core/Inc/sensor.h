#ifndef __SENSOR_H
#define __SENSOR_H

#include "stm32f1xx_hal.h"

typedef enum {
    SENSOR_INIT_STOP_AMES = 0,  /* Send @N AMES 0 0 to stop any ongoing auto-send */
    SENSOR_INIT_OPEN,           /* Send OPEN N */
    SENSOR_INIT_GET_VER,        /* Send VER, wait for DVU/VER reply */
    SENSOR_READY_START_AMES,    /* Send @N AMES 0 1000 to enable auto-send at 1 Hz */
    SENSOR_READY_AMES,          /* Listening mode: AMES packets arrive automatically */
    SENSOR_READY_POLL,          /* Legacy: Send M 1, used as fallback only */
    SENSOR_ERROR
} SensorState_t;

typedef struct {
    float speed;
    float direction;
    SensorState_t state;
    uint32_t last_sync;       /* Timestamp of last state transition */
    uint32_t ames_last_data;  /* Timestamp of last received AMES packet (updated externally) */
    uint8_t id;
} Sensor_t;

void Sensor_Init(Sensor_t *sensor, uint8_t id);
int  Sensor_Parse(Sensor_t *sensor, char *buffer);
void Sensor_Step(Sensor_t *sensor, UART_HandleTypeDef *huart);

#endif /* __SENSOR_H */

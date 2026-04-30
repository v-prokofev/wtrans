#ifndef __SENSOR_H
#define __SENSOR_H

#include "stm32f1xx_hal.h"

typedef enum {
    SENSOR_INIT_STOP_AMES = 0,
    SENSOR_INIT_OPEN,
    SENSOR_INIT_GET_VER,
    SENSOR_READY_POLL,
    SENSOR_ERROR
} SensorState_t;

typedef struct {
    float speed;
    float direction;
    SensorState_t state;
    uint32_t last_sync;
    uint8_t id;
} Sensor_t;

void Sensor_Init(Sensor_t *sensor, uint8_t id);
int  Sensor_Parse(Sensor_t *sensor, char *buffer);
void Sensor_Step(Sensor_t *sensor, UART_HandleTypeDef *huart);

#endif /* __SENSOR_H */

#ifndef SENSOR_H
#define SENSOR_H

void Sensor_Init(void);
void Sensor_StartDMA(void);
void Sensor_SendCommand(const char *cmd);
char* Sensor_GetBuffer(void);
int Sensor_ParseResponse(char *buf, float *p_spd, float *p_dir);

#endif // SENSOR_H

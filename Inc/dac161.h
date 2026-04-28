#ifndef DAC161_H
#define DAC161_H

#include <stdint.h>

void DAC_Init(void);
void DAC_UpdateCode(int channel, uint16_t code);

#endif // DAC161_H

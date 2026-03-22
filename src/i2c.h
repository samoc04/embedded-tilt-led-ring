#ifndef I2C_H
#define I2C_H

#include <stdint.h>

#define WRITE 0
#define READ 1

void initI2C(void);
void ResetI2C(void);
void I2CStart(uint8_t address, int rw, int nbytes);
void I2CReStart(uint8_t address, int rw, int nbytes);
void I2CStop(void);
void I2CWrite(uint8_t Data);
uint8_t I2CRead(void);

#endif
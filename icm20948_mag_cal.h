#ifndef ICM20948_MAG_CAL_H
#define ICM20948_MAG_CAL_H

#include "stm32f3xx_hal.h"
#include <stdint.h>

typedef struct
{
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;

    uint8_t icm_who_am_i;
    uint8_t mag_who_am_i;
} ICM20948_MAG_t;

typedef struct
{
    int16_t x_raw;
    int16_t y_raw;
    int16_t z_raw;

    int32_t x_milli_uT;
    int32_t y_milli_uT;
    int32_t z_milli_uT;

    uint8_t data_ready;
    uint8_t overflow;
} ICM20948_MAG_Data_t;

typedef struct
{
    int16_t min_x;
    int16_t max_x;
    int16_t min_y;
    int16_t max_y;
    int16_t min_z;
    int16_t max_z;

    float offset_x;
    float offset_y;
    float offset_z;

    float scale_x;
    float scale_y;
    float scale_z;

    uint32_t sample_count;
    uint8_t valid;
} ICM20948_MAG_Calibration_t;

typedef struct
{
    float x_raw_corrected;
    float y_raw_corrected;
    float z_raw_corrected;

    float x_uT;
    float y_uT;
    float z_uT;
} ICM20948_MAG_CalibratedData_t;

HAL_StatusTypeDef ICM20948_MAG_Init(
        ICM20948_MAG_t *dev,
        SPI_HandleTypeDef *hspi,
        GPIO_TypeDef *cs_port,
        uint16_t cs_pin);

HAL_StatusTypeDef ICM20948_MAG_Read(
        ICM20948_MAG_t *dev,
        ICM20948_MAG_Data_t *data);

void ICM20948_MAG_CalibrationBegin(
        ICM20948_MAG_Calibration_t *cal);

void ICM20948_MAG_CalibrationAddSample(
        ICM20948_MAG_Calibration_t *cal,
        const ICM20948_MAG_Data_t *data);

HAL_StatusTypeDef ICM20948_MAG_CalibrationFinish(
        ICM20948_MAG_Calibration_t *cal);

void ICM20948_MAG_ApplyCalibration(
        const ICM20948_MAG_Calibration_t *cal,
        const ICM20948_MAG_Data_t *raw,
        ICM20948_MAG_CalibratedData_t *out);

#endif

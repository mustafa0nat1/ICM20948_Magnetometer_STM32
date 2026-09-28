#include "icm20948_mag_cal.h"
#include <limits.h>

/* ---------------- ICM-20948 USER BANK 0 ---------------- */

#define ICM20948_REG_WHO_AM_I             0x00
#define ICM20948_REG_USER_CTRL            0x03
#define ICM20948_REG_LP_CONFIG            0x05
#define ICM20948_REG_PWR_MGMT_1           0x06
#define ICM20948_REG_PWR_MGMT_2           0x07
#define ICM20948_REG_I2C_MST_STATUS       0x17
#define ICM20948_REG_EXT_SENS_DATA_00     0x3B
#define ICM20948_REG_BANK_SEL             0x7F

#define ICM20948_WHO_AM_I_VALUE           0xEA

/* ---------------- ICM-20948 USER BANK 3 ---------------- */

#define ICM20948_B3_I2C_MST_ODR_CONFIG    0x00
#define ICM20948_B3_I2C_MST_CTRL          0x01

#define ICM20948_B3_I2C_SLV0_ADDR         0x03
#define ICM20948_B3_I2C_SLV0_REG          0x04
#define ICM20948_B3_I2C_SLV0_CTRL         0x05

#define ICM20948_B3_I2C_SLV4_ADDR         0x13
#define ICM20948_B3_I2C_SLV4_REG          0x14
#define ICM20948_B3_I2C_SLV4_CTRL         0x15
#define ICM20948_B3_I2C_SLV4_DO           0x16
#define ICM20948_B3_I2C_SLV4_DI           0x17

#define ICM20948_BANK_0                    0
#define ICM20948_BANK_3                    3

/* ---------------- AK09916 ---------------- */

#define AK09916_I2C_ADDRESS                0x0C
#define AK09916_REG_WIA2                   0x01
#define AK09916_REG_ST1                    0x10
#define AK09916_REG_CNTL2                  0x31
#define AK09916_REG_CNTL3                  0x32

#define AK09916_WHO_AM_I_VALUE             0x09
#define AK09916_MODE_CONTINUOUS_4          0x08

/* 0.15 uT / LSB = 150 milli-uT / LSB */
#define AK09916_MILLI_UT_PER_LSB           150

/* ------------------------------------------------------- */

static void CS_Low(ICM20948_MAG_t *dev)
{
    HAL_GPIO_WritePin(
            dev->cs_port,
            dev->cs_pin,
            GPIO_PIN_RESET);
}

static void CS_High(ICM20948_MAG_t *dev)
{
    HAL_GPIO_WritePin(
            dev->cs_port,
            dev->cs_pin,
            GPIO_PIN_SET);
}

static HAL_StatusTypeDef ICM_WriteRegister(
        ICM20948_MAG_t *dev,
        uint8_t address,
        uint8_t value)
{
    uint8_t tx[2];

    tx[0] = address & 0x7F;
    tx[1] = value;

    CS_Low(dev);

    HAL_StatusTypeDef status =
            HAL_SPI_Transmit(
                    dev->hspi,
                    tx,
                    2,
                    1000);

    CS_High(dev);

    return status;
}

static HAL_StatusTypeDef ICM_ReadRegister(
        ICM20948_MAG_t *dev,
        uint8_t address,
        uint8_t *value)
{
    uint8_t tx[2] = {0};
    uint8_t rx[2] = {0};

    tx[0] = address | 0x80;

    CS_Low(dev);

    HAL_StatusTypeDef status =
            HAL_SPI_TransmitReceive(
                    dev->hspi,
                    tx,
                    rx,
                    2,
                    1000);

    CS_High(dev);

    if (status == HAL_OK)
    {
        *value = rx[1];
    }

    return status;
}

static HAL_StatusTypeDef ICM_ReadRegisters(
        ICM20948_MAG_t *dev,
        uint8_t start_address,
        uint8_t *data,
        uint8_t length)
{
    if ((data == NULL) || (length == 0) || (length > 24))
    {
        return HAL_ERROR;
    }

    uint8_t tx[25] = {0};
    uint8_t rx[25] = {0};

    tx[0] = start_address | 0x80;

    CS_Low(dev);

    HAL_StatusTypeDef status =
            HAL_SPI_TransmitReceive(
                    dev->hspi,
                    tx,
                    rx,
                    length + 1,
                    1000);

    CS_High(dev);

    if (status != HAL_OK)
    {
        return status;
    }

    for (uint8_t i = 0; i < length; i++)
    {
        data[i] = rx[i + 1];
    }

    return HAL_OK;
}

static HAL_StatusTypeDef SelectBank(
        ICM20948_MAG_t *dev,
        uint8_t bank)
{
    return ICM_WriteRegister(
            dev,
            ICM20948_REG_BANK_SEL,
            (uint8_t)((bank & 0x03) << 4));
}

/* ------------------------------------------------------- */
/* SLAVE 4: AK09916 single-byte access                     */
/* ------------------------------------------------------- */

static HAL_StatusTypeDef WaitSlave4(
        ICM20948_MAG_t *dev,
        uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < timeout_ms)
    {
        uint8_t status = 0;

        if (SelectBank(dev, ICM20948_BANK_0) != HAL_OK)
        {
            return HAL_ERROR;
        }

        if (ICM_ReadRegister(
                dev,
                ICM20948_REG_I2C_MST_STATUS,
                &status) != HAL_OK)
        {
            return HAL_ERROR;
        }

        if (status & 0x10)
        {
            return HAL_ERROR;
        }

        if (status & 0x40)
        {
            return HAL_OK;
        }

        HAL_Delay(1);
    }

    return HAL_TIMEOUT;
}

static HAL_StatusTypeDef AK_WriteByte(
        ICM20948_MAG_t *dev,
        uint8_t reg,
        uint8_t value)
{
    HAL_StatusTypeDef status;

    status = SelectBank(dev, ICM20948_BANK_3);
    if (status != HAL_OK) return status;

    status = ICM_WriteRegister(
            dev,
            ICM20948_B3_I2C_SLV4_ADDR,
            AK09916_I2C_ADDRESS);
    if (status != HAL_OK) return status;

    status = ICM_WriteRegister(
            dev,
            ICM20948_B3_I2C_SLV4_REG,
            reg);
    if (status != HAL_OK) return status;

    status = ICM_WriteRegister(
            dev,
            ICM20948_B3_I2C_SLV4_DO,
            value);
    if (status != HAL_OK) return status;

    status = ICM_WriteRegister(
            dev,
            ICM20948_B3_I2C_SLV4_CTRL,
            0x80);
    if (status != HAL_OK) return status;

    status = WaitSlave4(dev, 100);

    SelectBank(dev, ICM20948_BANK_0);

    return status;
}

static HAL_StatusTypeDef AK_ReadByte(
        ICM20948_MAG_t *dev,
        uint8_t reg,
        uint8_t *value)
{
    HAL_StatusTypeDef status;

    status = SelectBank(dev, ICM20948_BANK_3);
    if (status != HAL_OK) return status;

    status = ICM_WriteRegister(
            dev,
            ICM20948_B3_I2C_SLV4_ADDR,
            0x80 | AK09916_I2C_ADDRESS);
    if (status != HAL_OK) return status;

    status = ICM_WriteRegister(
            dev,
            ICM20948_B3_I2C_SLV4_REG,
            reg);
    if (status != HAL_OK) return status;

    status = ICM_WriteRegister(
            dev,
            ICM20948_B3_I2C_SLV4_CTRL,
            0x80);
    if (status != HAL_OK) return status;

    status = WaitSlave4(dev, 100);
    if (status != HAL_OK)
    {
        return status;
    }

    status = SelectBank(dev, ICM20948_BANK_3);
    if (status != HAL_OK) return status;

    status = ICM_ReadRegister(
            dev,
            ICM20948_B3_I2C_SLV4_DI,
            value);

    SelectBank(dev, ICM20948_BANK_0);

    return status;
}

/* ------------------------------------------------------- */
/* Magnetometer init / read                                */
/* ------------------------------------------------------- */

HAL_StatusTypeDef ICM20948_MAG_Init(
        ICM20948_MAG_t *dev,
        SPI_HandleTypeDef *hspi,
        GPIO_TypeDef *cs_port,
        uint16_t cs_pin)
{
    HAL_StatusTypeDef status;

    if ((dev == NULL) ||
        (hspi == NULL) ||
        (cs_port == NULL))
    {
        return HAL_ERROR;
    }

    dev->hspi = hspi;
    dev->cs_port = cs_port;
    dev->cs_pin = cs_pin;
    dev->icm_who_am_i = 0;
    dev->mag_who_am_i = 0;

    CS_High(dev);

    HAL_Delay(200);

    status = SelectBank(dev, ICM20948_BANK_0);
    if (status != HAL_OK) return status;

    status = ICM_ReadRegister(
            dev,
            ICM20948_REG_WHO_AM_I,
            &dev->icm_who_am_i);

    if (status != HAL_OK)
    {
        return status;
    }

    if (dev->icm_who_am_i != ICM20948_WHO_AM_I_VALUE)
    {
        return HAL_ERROR;
    }

    /*
     * USER_CTRL:
     * bit5 I2C_MST_EN = 1
     * bit4 I2C_IF_DIS = 1
     */
    status = ICM_WriteRegister(
            dev,
            ICM20948_REG_USER_CTRL,
            0x30);

    if (status != HAL_OK) return status;

    /*
     * Wake device, CLKSEL = 1
     */
    status = ICM_WriteRegister(
            dev,
            ICM20948_REG_PWR_MGMT_1,
            0x01);

    if (status != HAL_OK) return status;

    HAL_Delay(50);

    /*
     * Magnetometer-only project:
     * disable accel + gyro.
     */
    status = ICM_WriteRegister(
            dev,
            ICM20948_REG_PWR_MGMT_2,
            0x3F);

    if (status != HAL_OK) return status;

    HAL_Delay(1);

    /*
     * Duty-cycled internal I2C master.
     */
    status = ICM_WriteRegister(
            dev,
            ICM20948_REG_LP_CONFIG,
            0x40);

    if (status != HAL_OK) return status;

    status = SelectBank(dev, ICM20948_BANK_3);
    if (status != HAL_OK) return status;

    /*
     * Internal I2C master ODR:
     * 1.1 kHz / 2^3 = 137.5 Hz
     */
    status = ICM_WriteRegister(
            dev,
            ICM20948_B3_I2C_MST_ODR_CONFIG,
            0x03);

    if (status != HAL_OK) return status;

    /*
     * I2C_MST_CLK = 7 -> nominal 345.6 kHz
     */
    status = ICM_WriteRegister(
            dev,
            ICM20948_B3_I2C_MST_CTRL,
            0x07);

    if (status != HAL_OK) return status;

    /*
     * Reset AK09916.
     */
    status = AK_WriteByte(
            dev,
            AK09916_REG_CNTL3,
            0x01);

    if (status != HAL_OK) return status;

    HAL_Delay(100);

    /*
     * Check AK09916 ID.
     */
    status = AK_ReadByte(
            dev,
            AK09916_REG_WIA2,
            &dev->mag_who_am_i);

    if (status != HAL_OK)
    {
        return status;
    }

    if (dev->mag_who_am_i != AK09916_WHO_AM_I_VALUE)
    {
        return HAL_ERROR;
    }

    /*
     * Continuous measurement mode 4.
     */
    status = AK_WriteByte(
            dev,
            AK09916_REG_CNTL2,
            AK09916_MODE_CONTINUOUS_4);

    if (status != HAL_OK)
    {
        return status;
    }

    HAL_Delay(20);

    /*
     * Continuous Slave 0 read:
     * ST1 -> ST2, total 9 bytes.
     */
    status = SelectBank(dev, ICM20948_BANK_3);
    if (status != HAL_OK) return status;

    status = ICM_WriteRegister(
            dev,
            ICM20948_B3_I2C_SLV0_ADDR,
            0x80 | AK09916_I2C_ADDRESS);

    if (status != HAL_OK) return status;

    status = ICM_WriteRegister(
            dev,
            ICM20948_B3_I2C_SLV0_REG,
            AK09916_REG_ST1);

    if (status != HAL_OK) return status;

    status = ICM_WriteRegister(
            dev,
            ICM20948_B3_I2C_SLV0_CTRL,
            0x89);

    if (status != HAL_OK) return status;

    HAL_Delay(100);

    return SelectBank(dev, ICM20948_BANK_0);
}

HAL_StatusTypeDef ICM20948_MAG_Read(
        ICM20948_MAG_t *dev,
        ICM20948_MAG_Data_t *data)
{
    uint8_t ext[9];

    if ((dev == NULL) || (data == NULL))
    {
        return HAL_ERROR;
    }

    HAL_StatusTypeDef status =
            SelectBank(dev, ICM20948_BANK_0);

    if (status != HAL_OK)
    {
        return status;
    }

    status = ICM_ReadRegisters(
            dev,
            ICM20948_REG_EXT_SENS_DATA_00,
            ext,
            9);

    if (status != HAL_OK)
    {
        return status;
    }

    data->data_ready =
            (ext[0] & 0x01) ? 1 : 0;

    data->overflow =
            (ext[8] & 0x08) ? 1 : 0;

    /*
     * AK09916 = little-endian.
     */
    data->x_raw =
            (int16_t)(
                    ((uint16_t)ext[2] << 8) |
                    ext[1]);

    data->y_raw =
            (int16_t)(
                    ((uint16_t)ext[4] << 8) |
                    ext[3]);

    data->z_raw =
            (int16_t)(
                    ((uint16_t)ext[6] << 8) |
                    ext[5]);

    data->x_milli_uT =
            (int32_t)data->x_raw *
            AK09916_MILLI_UT_PER_LSB;

    data->y_milli_uT =
            (int32_t)data->y_raw *
            AK09916_MILLI_UT_PER_LSB;

    data->z_milli_uT =
            (int32_t)data->z_raw *
            AK09916_MILLI_UT_PER_LSB;

    return HAL_OK;
}

/* ------------------------------------------------------- */
/* Min / max magnetometer calibration                      */
/* ------------------------------------------------------- */

void ICM20948_MAG_CalibrationBegin(
        ICM20948_MAG_Calibration_t *cal)
{
    if (cal == NULL)
    {
        return;
    }

    cal->min_x = INT16_MAX;
    cal->max_x = INT16_MIN;

    cal->min_y = INT16_MAX;
    cal->max_y = INT16_MIN;

    cal->min_z = INT16_MAX;
    cal->max_z = INT16_MIN;

    cal->offset_x = 0.0f;
    cal->offset_y = 0.0f;
    cal->offset_z = 0.0f;

    cal->scale_x = 1.0f;
    cal->scale_y = 1.0f;
    cal->scale_z = 1.0f;

    cal->sample_count = 0;
    cal->valid = 0;
}

void ICM20948_MAG_CalibrationAddSample(
        ICM20948_MAG_Calibration_t *cal,
        const ICM20948_MAG_Data_t *data)
{
    if ((cal == NULL) || (data == NULL))
    {
        return;
    }

    if (!data->data_ready || data->overflow)
    {
        return;
    }

    if (data->x_raw < cal->min_x) cal->min_x = data->x_raw;
    if (data->x_raw > cal->max_x) cal->max_x = data->x_raw;

    if (data->y_raw < cal->min_y) cal->min_y = data->y_raw;
    if (data->y_raw > cal->max_y) cal->max_y = data->y_raw;

    if (data->z_raw < cal->min_z) cal->min_z = data->z_raw;
    if (data->z_raw > cal->max_z) cal->max_z = data->z_raw;

    cal->sample_count++;
}

HAL_StatusTypeDef ICM20948_MAG_CalibrationFinish(
        ICM20948_MAG_Calibration_t *cal)
{
    if (cal == NULL)
    {
        return HAL_ERROR;
    }

    if (cal->sample_count < 100)
    {
        return HAL_ERROR;
    }

    float radius_x =
            ((float)cal->max_x - (float)cal->min_x) * 0.5f;

    float radius_y =
            ((float)cal->max_y - (float)cal->min_y) * 0.5f;

    float radius_z =
            ((float)cal->max_z - (float)cal->min_z) * 0.5f;

    /*
     * If one axis barely changed, the sensor was not
     * rotated through enough orientations.
     */
    if ((radius_x < 20.0f) ||
        (radius_y < 20.0f) ||
        (radius_z < 20.0f))
    {
        return HAL_ERROR;
    }

    cal->offset_x =
            ((float)cal->max_x + (float)cal->min_x) * 0.5f;

    cal->offset_y =
            ((float)cal->max_y + (float)cal->min_y) * 0.5f;

    cal->offset_z =
            ((float)cal->max_z + (float)cal->min_z) * 0.5f;

    float average_radius =
            (radius_x + radius_y + radius_z) / 3.0f;

    cal->scale_x =
            average_radius / radius_x;

    cal->scale_y =
            average_radius / radius_y;

    cal->scale_z =
            average_radius / radius_z;

    cal->valid = 1;

    return HAL_OK;
}

void ICM20948_MAG_ApplyCalibration(
        const ICM20948_MAG_Calibration_t *cal,
        const ICM20948_MAG_Data_t *raw,
        ICM20948_MAG_CalibratedData_t *out)
{
    if ((cal == NULL) ||
        (raw == NULL) ||
        (out == NULL))
    {
        return;
    }

    if (!cal->valid)
    {
        out->x_raw_corrected = (float)raw->x_raw;
        out->y_raw_corrected = (float)raw->y_raw;
        out->z_raw_corrected = (float)raw->z_raw;
    }
    else
    {
        out->x_raw_corrected =
                ((float)raw->x_raw - cal->offset_x) *
                cal->scale_x;

        out->y_raw_corrected =
                ((float)raw->y_raw - cal->offset_y) *
                cal->scale_y;

        out->z_raw_corrected =
                ((float)raw->z_raw - cal->offset_z) *
                cal->scale_z;
    }

    /*
     * Align AK09916 magnetometer axes with the ICM-20948
     * accelerometer/gyroscope frame used by the INS project:
     * X stays the same, Y and Z are inverted.
     */
    out->x_uT =
            out->x_raw_corrected * 0.15f;

    out->y_uT =
            -out->y_raw_corrected * 0.15f;

    out->z_uT =
            -out->z_raw_corrected * 0.15f;
}

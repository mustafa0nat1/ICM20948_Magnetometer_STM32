/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : ICM-20948 Magnetometer Termite Calibration
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"

/* USER CODE BEGIN Includes */

#include "icm20948_mag_cal.h"

#include <stdio.h>
#include <string.h>

/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/

SPI_HandleTypeDef hspi1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

ICM20948_MAG_t mag;
ICM20948_MAG_Data_t magData;
ICM20948_MAG_Calibration_t magCal;
ICM20948_MAG_CalibratedData_t magCalData;

char uartBuffer[256];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/

void SystemClock_Config(void);

static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_UART_Init(void);

/* USER CODE BEGIN PFP */

static void UART_Send(const char *text);

static void FormatFloat3(
        char *buffer,
        size_t buffer_size,
        float value);

/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

static void UART_Send(const char *text)
{
    HAL_UART_Transmit(
            &huart2,
            (uint8_t *)text,
            strlen(text),
            HAL_MAX_DELAY);
}

/*
 * Float printf linker ayari gerektirmesin diye
 * sayiyi kendimiz 3 basamakli stringe ceviriyoruz.
 */
static void FormatFloat3(
        char *buffer,
        size_t buffer_size,
        float value)
{
    int32_t scaled;

    if (value >= 0.0f)
    {
        scaled = (int32_t)(value * 1000.0f + 0.5f);
    }
    else
    {
        scaled = (int32_t)(value * 1000.0f - 0.5f);
    }

    int32_t whole =
            scaled / 1000;

    int32_t fraction =
            scaled % 1000;

    if (fraction < 0)
    {
        fraction = -fraction;
    }

    if ((scaled < 0) && (whole == 0))
    {
        snprintf(
                buffer,
                buffer_size,
                "-0.%03ld",
                (long)fraction);
    }
    else
    {
        snprintf(
                buffer,
                buffer_size,
                "%ld.%03ld",
                (long)whole,
                (long)fraction);
    }
}

/* USER CODE END 0 */

int main(void)
{
    HAL_Init();

    SystemClock_Config();

    MX_GPIO_Init();
    MX_SPI1_Init();
    MX_USART2_UART_Init();

    /* USER CODE BEGIN 2 */

    UART_Send("\r\n");
    UART_Send("============================================\r\n");
    UART_Send("ICM-20948 MAGNETOMETER CALIBRATION\r\n");
    UART_Send("============================================\r\n");

    if (ICM20948_MAG_Init(
            &mag,
            &hspi1,
            GPIOA,
            GPIO_PIN_4) != HAL_OK)
    {
        snprintf(
                uartBuffer,
                sizeof(uartBuffer),
                "INIT ERROR | ICM:0x%02X MAG:0x%02X\r\n",
                mag.icm_who_am_i,
                mag.mag_who_am_i);

        UART_Send(uartBuffer);

        while (1)
        {
            HAL_Delay(1000);
        }
    }

    snprintf(
            uartBuffer,
            sizeof(uartBuffer),
            "ICM-20948 FOUND : 0x%02X\r\n"
            "AK09916 FOUND   : 0x%02X\r\n\r\n",
            mag.icm_who_am_i,
            mag.mag_who_am_i);

    UART_Send(uartBuffer);

    UART_Send("CALIBRATION WILL START IN 5 SECONDS\r\n");
    UART_Send("During calibration rotate the board slowly\r\n");
    UART_Send("through as many 3D orientations as possible.\r\n");
    UART_Send("Do not keep it only flat on the table.\r\n\r\n");

    for (int i = 5; i >= 1; i--)
    {
        snprintf(
                uartBuffer,
                sizeof(uartBuffer),
                "Starting in %d...\r\n",
                i);

        UART_Send(uartBuffer);
        HAL_Delay(1000);
    }

    ICM20948_MAG_CalibrationBegin(&magCal);

    UART_Send("\r\n");
    UART_Send("========== CALIBRATION START ==========\r\n");

    uint32_t calibration_start =
            HAL_GetTick();

    uint32_t last_progress =
            calibration_start;

    const uint32_t calibration_time_ms =
            30000;

    while ((HAL_GetTick() - calibration_start) <
            calibration_time_ms)
    {
        if (ICM20948_MAG_Read(
                &mag,
                &magData) == HAL_OK)
        {
            ICM20948_MAG_CalibrationAddSample(
                    &magCal,
                    &magData);
        }

        /*
         * Her saniye ilerleme ve mevcut min/max.
         */
        if ((HAL_GetTick() - last_progress) >= 1000)
        {
            last_progress = HAL_GetTick();

            uint32_t elapsed =
                    HAL_GetTick() - calibration_start;

            uint32_t remaining =
                    (calibration_time_ms > elapsed)
                    ? (calibration_time_ms - elapsed) / 1000
                    : 0;

            snprintf(
                    uartBuffer,
                    sizeof(uartBuffer),

                    "LEFT:%2lu s | "
                    "X[%6d,%6d] "
                    "Y[%6d,%6d] "
                    "Z[%6d,%6d] | "
                    "N:%lu\r\n",

                    (unsigned long)remaining,

                    magCal.min_x,
                    magCal.max_x,

                    magCal.min_y,
                    magCal.max_y,

                    magCal.min_z,
                    magCal.max_z,

                    (unsigned long)magCal.sample_count);

            UART_Send(uartBuffer);
        }

        HAL_Delay(10);
    }

    UART_Send("=========== CALIBRATION END ===========\r\n\r\n");

    if (ICM20948_MAG_CalibrationFinish(
            &magCal) != HAL_OK)
    {
        UART_Send("CALIBRATION FAILED\r\n");
        UART_Send("Not enough movement / axis range.\r\n");
        UART_Send("Press RESET and rotate through more orientations.\r\n");

        while (1)
        {
            HAL_Delay(1000);
        }
    }

    char ox[24], oy[24], oz[24];
    char sx[24], sy[24], sz[24];

    FormatFloat3(
            ox,
            sizeof(ox),
            magCal.offset_x);

    FormatFloat3(
            oy,
            sizeof(oy),
            magCal.offset_y);

    FormatFloat3(
            oz,
            sizeof(oz),
            magCal.offset_z);

    FormatFloat3(
            sx,
            sizeof(sx),
            magCal.scale_x);

    FormatFloat3(
            sy,
            sizeof(sy),
            magCal.scale_y);

    FormatFloat3(
            sz,
            sizeof(sz),
            magCal.scale_z);

    UART_Send("CALIBRATION SUCCESS\r\n\r\n");

    snprintf(
            uartBuffer,
            sizeof(uartBuffer),

            "MIN/MAX RAW\r\n"
            "X min:%d max:%d\r\n"
            "Y min:%d max:%d\r\n"
            "Z min:%d max:%d\r\n\r\n",

            magCal.min_x,
            magCal.max_x,

            magCal.min_y,
            magCal.max_y,

            magCal.min_z,
            magCal.max_z);

    UART_Send(uartBuffer);

    snprintf(
            uartBuffer,
            sizeof(uartBuffer),

            "HARD-IRON OFFSET [raw LSB]\r\n"
            "X:%s Y:%s Z:%s\r\n\r\n"
            "AXIS SCALE\r\n"
            "X:%s Y:%s Z:%s\r\n\r\n",

            ox,
            oy,
            oz,

            sx,
            sy,
            sz);

    UART_Send(uartBuffer);

    UART_Send("NORMAL MODE STARTED\r\n");
    UART_Send("RAW and CALIBRATED values will now be printed.\r\n\r\n");

    /* USER CODE END 2 */

    /* USER CODE BEGIN WHILE */

    while (1)
    {
        if (ICM20948_MAG_Read(
                &mag,
                &magData) == HAL_OK)
        {
            if (!magData.overflow)
            {
                ICM20948_MAG_ApplyCalibration(
                        &magCal,
                        &magData,
                        &magCalData);

                char cx[24], cy[24], cz[24];
                char ux[24], uy[24], uz[24];

                FormatFloat3(
                        cx,
                        sizeof(cx),
                        magCalData.x_raw_corrected);

                FormatFloat3(
                        cy,
                        sizeof(cy),
                        magCalData.y_raw_corrected);

                FormatFloat3(
                        cz,
                        sizeof(cz),
                        magCalData.z_raw_corrected);

                FormatFloat3(
                        ux,
                        sizeof(ux),
                        magCalData.x_uT);

                FormatFloat3(
                        uy,
                        sizeof(uy),
                        magCalData.y_uT);

                FormatFloat3(
                        uz,
                        sizeof(uz),
                        magCalData.z_uT);

                snprintf(
                        uartBuffer,
                        sizeof(uartBuffer),

                        "RAW X:%6d Y:%6d Z:%6d | "
                        "CAL X:%s Y:%s Z:%s | "
                        "uT X:%s Y:%s Z:%s | "
                        "DRDY:%u HOFL:%u\r\n",

                        magData.x_raw,
                        magData.y_raw,
                        magData.z_raw,

                        cx,
                        cy,
                        cz,

                        ux,
                        uy,
                        uz,

                        magData.data_ready,
                        magData.overflow);

                UART_Send(uartBuffer);
            }
        }
        else
        {
            UART_Send("MAG READ ERROR\r\n");
        }

        HAL_Delay(100);
    }

    /* USER CODE END WHILE */
}


/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType =
            RCC_OSCILLATORTYPE_HSI;

    RCC_OscInitStruct.HSIState =
            RCC_HSI_ON;

    RCC_OscInitStruct.HSICalibrationValue =
            RCC_HSICALIBRATION_DEFAULT;

    RCC_OscInitStruct.PLL.PLLState =
            RCC_PLL_ON;

    RCC_OscInitStruct.PLL.PLLSource =
            RCC_PLLSOURCE_HSI;

    RCC_OscInitStruct.PLL.PLLMUL =
            RCC_PLL_MUL4;

    if (HAL_RCC_OscConfig(
            &RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType =
            RCC_CLOCKTYPE_HCLK |
            RCC_CLOCKTYPE_SYSCLK |
            RCC_CLOCKTYPE_PCLK1 |
            RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource =
            RCC_SYSCLKSOURCE_PLLCLK;

    RCC_ClkInitStruct.AHBCLKDivider =
            RCC_SYSCLK_DIV2;

    RCC_ClkInitStruct.APB1CLKDivider =
            RCC_HCLK_DIV2;

    RCC_ClkInitStruct.APB2CLKDivider =
            RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(
            &RCC_ClkInitStruct,
            FLASH_LATENCY_0) != HAL_OK)
    {
        Error_Handler();
    }
}


/**
  * @brief SPI1 Initialization Function
  * @retval None
  */
static void MX_SPI1_Init(void)
{
    hspi1.Instance = SPI1;

    hspi1.Init.Mode =
            SPI_MODE_MASTER;

    hspi1.Init.Direction =
            SPI_DIRECTION_2LINES;

    hspi1.Init.DataSize =
            SPI_DATASIZE_8BIT;

    hspi1.Init.CLKPolarity =
            SPI_POLARITY_LOW;

    hspi1.Init.CLKPhase =
            SPI_PHASE_1EDGE;

    hspi1.Init.NSS =
            SPI_NSS_SOFT;

    hspi1.Init.BaudRatePrescaler =
            SPI_BAUDRATEPRESCALER_8;

    hspi1.Init.FirstBit =
            SPI_FIRSTBIT_MSB;

    hspi1.Init.TIMode =
            SPI_TIMODE_DISABLE;

    hspi1.Init.CRCCalculation =
            SPI_CRCCALCULATION_DISABLE;

    hspi1.Init.CRCPolynomial = 7;

    hspi1.Init.CRCLength =
            SPI_CRC_LENGTH_DATASIZE;

    hspi1.Init.NSSPMode =
            SPI_NSS_PULSE_DISABLE;

    if (HAL_SPI_Init(&hspi1) != HAL_OK)
    {
        Error_Handler();
    }
}


/**
  * @brief USART2 Initialization Function
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;

    huart2.Init.BaudRate =
            115200;

    huart2.Init.WordLength =
            UART_WORDLENGTH_8B;

    huart2.Init.StopBits =
            UART_STOPBITS_1;

    huart2.Init.Parity =
            UART_PARITY_NONE;

    huart2.Init.Mode =
            UART_MODE_TX_RX;

    huart2.Init.HwFlowCtl =
            UART_HWCONTROL_NONE;

    huart2.Init.OverSampling =
            UART_OVERSAMPLING_16;

    huart2.Init.OneBitSampling =
            UART_ONE_BIT_SAMPLE_DISABLE;

    huart2.AdvancedInit.AdvFeatureInit =
            UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
        Error_Handler();
    }
}


/**
  * @brief GPIO Initialization Function
  * @retval None
  */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    HAL_GPIO_WritePin(
            GPIOA,
            GPIO_PIN_4,
            GPIO_PIN_SET);

    GPIO_InitStruct.Pin =
            GPIO_PIN_4;

    GPIO_InitStruct.Mode =
            GPIO_MODE_OUTPUT_PP;

    GPIO_InitStruct.Pull =
            GPIO_NOPULL;

    GPIO_InitStruct.Speed =
            GPIO_SPEED_FREQ_HIGH;

    HAL_GPIO_Init(
            GPIOA,
            &GPIO_InitStruct);
}


void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}


#ifdef USE_FULL_ASSERT
void assert_failed(
        uint8_t *file,
        uint32_t line)
{
}
#endif

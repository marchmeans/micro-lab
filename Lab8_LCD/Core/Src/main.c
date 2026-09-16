/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ILI9341_STM32_Driver.h"
#include "ILI9341_GFX.h"
#include "ILI9341_Touchscreen.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define AM2320_ADDR   (0x5C << 1)

/* Build a 16-bit RGB565 colour from 8-bit R,G,B */
#define RGB565(r,g,b)  ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

#define PALE_RED    RGB565(255,180,180)
#define PALE_GREEN  RGB565(180,255,180)
#define PALE_BLUE   RGB565(180,180,255)

/* Layout constants */
#define ROW_R_Y   70
#define ROW_G_Y   130
#define ROW_B_Y   190
#define BAR_X     65
#define BAR_W     190
#define BAR_H     22
#define CIRC_X    35
#define CIRC_R    20
#define PCT_X     265

/* Touch calibration (measured 4-corner values from earlier calibration) */
#define TOUCH_RY_AT_X0    65606
#define TOUCH_RY_AT_X320  6127
#define TOUCH_RX_AT_Y0    56343
#define TOUCH_RX_AT_Y240  7309
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
float   temperature = 0.0f;
float   humidity    = 0.0f;
uint8_t sensorOK     = 0;

/* RGB brightness percentages (0-100), start at 0% for all colours */
uint8_t redPct   = 0;
uint8_t greenPct = 0;
uint8_t bluePct  = 0;

uint8_t  touchWasPressed = 0;   /* กัน debounce: แตะค้างไม่ให้เพิ่มค่าซ้ำ */
uint32_t lastSensorTick  = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */
uint16_t AM2320_CRC16(uint8_t *ptr, uint8_t length);
uint8_t  AM2320_Read(float *temp, float *hum);

void Draw_Screen1_Static(void);
void Draw_TempHumidity(float temp, float hum);
void Draw_Mixed_Circle(uint8_t r, uint8_t g, uint8_t b);
void Draw_RGB_Row(uint16_t y, uint16_t barColour, uint16_t paleColour, uint8_t percent);

static void Touch_Get_Raw(uint16_t *raw_x, uint16_t *raw_y);
static void Touch_Get_Screen_Coords(uint16_t *screen_x, uint16_t *screen_y);
static uint8_t Point_In_Circle(uint16_t px, uint16_t py, uint16_t cx, uint16_t cy, uint16_t r);
void Handle_Touch(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

uint16_t AM2320_CRC16(uint8_t *ptr, uint8_t length)
{
  uint16_t crc = 0xFFFF;
  for (uint8_t i = 0; i < length; i++)
  {
    crc ^= ptr[i];
    for (uint8_t j = 0; j < 8; j++)
    {
      if (crc & 0x01) { crc >>= 1; crc ^= 0xA001; }
      else             { crc >>= 1; }
    }
  }
  return crc;
}

uint8_t AM2320_Read(float *temp, float *hum)
{
  uint8_t wakeByte = 0x00;
  uint8_t cmd[3]   = {0x03, 0x00, 0x04};
  uint8_t data[8]  = {0};

  HAL_I2C_Master_Transmit(&hi2c1, AM2320_ADDR, &wakeByte, 1, 10);
  HAL_Delay(2);

  if (HAL_I2C_Master_Transmit(&hi2c1, AM2320_ADDR, cmd, 3, 10) != HAL_OK) return 0;
  HAL_Delay(2);

  if (HAL_I2C_Master_Receive(&hi2c1, AM2320_ADDR, data, 8, 10) != HAL_OK) return 0;

  if (data[0] != 0x03 || data[1] != 0x04) return 0;

  uint16_t crcReceived = (data[7] << 8) | data[6];
  uint16_t crcCalc     = AM2320_CRC16(data, 6);
  if (crcReceived != crcCalc) return 0;

  uint16_t rawHum  = (data[2] << 8) | data[3];
  uint16_t rawTemp = (data[4] << 8) | data[5];

  *hum = rawHum / 10.0f;
  if (rawTemp & 0x8000) *temp = -((rawTemp & 0x7FFF) / 10.0f);
  else                  *temp = rawTemp / 10.0f;

  return 1;
}

/* วาด layout คงที่ครั้งเดียวตอนเริ่ม: กรอบ, วงกลมสี R/G/B, แถบพื้นหลัง, label */
void Draw_Screen1_Static(void)
{
  ILI9341_Fill_Screen(WHITE);

  ILI9341_Draw_Hollow_Rectangle_Coord(2, 2, 317, 237, BLACK);
  ILI9341_Draw_Horizontal_Line(2, 58, 315, BLACK);

  ILI9341_Draw_Filled_Circle(CIRC_X, ROW_R_Y + 20, CIRC_R, RED);
  ILI9341_Draw_Filled_Circle(CIRC_X, ROW_G_Y + 20, CIRC_R, GREEN);
  ILI9341_Draw_Filled_Circle(CIRC_X, ROW_B_Y + 20, CIRC_R, BLUE);
}

void Draw_TempHumidity(float temp, float hum)
{
  char buf[16];

  snprintf(buf, sizeof(buf), "%4.1f C", temp);
  ILI9341_Draw_Text(buf, 10, 15, BLACK, 3, WHITE);

  snprintf(buf, sizeof(buf), "%4.1f %%RH", hum);
  ILI9341_Draw_Text(buf, 215, 15, BLACK, 2, WHITE);
}

void Draw_Mixed_Circle(uint8_t r, uint8_t g, uint8_t b)
{
  uint8_t r8 = (uint16_t)r * 255 / 100;
  uint8_t g8 = (uint16_t)g * 255 / 100;
  uint8_t b8 = (uint16_t)b * 255 / 100;
  uint16_t mixed = RGB565(r8, g8, b8);

  ILI9341_Draw_Filled_Circle(160, 35, 25, mixed);
}

void Draw_RGB_Row(uint16_t y, uint16_t barColour, uint16_t paleColour, uint8_t percent)
{
  uint16_t darkW = (uint16_t)BAR_W * percent / 100;

  if (darkW > 0)
    ILI9341_Draw_Rectangle(BAR_X, y, darkW, BAR_H, barColour);
  if (darkW < BAR_W)
    ILI9341_Draw_Rectangle(BAR_X + darkW, y, BAR_W - darkW, BAR_H, paleColour);

  char buf[8];
  snprintf(buf, sizeof(buf), "%3d%%", percent);
  ILI9341_Draw_Text(buf, PCT_X, y + 2, BLACK, 2, WHITE);
}

/* --- Touch handling --- */

static void Touch_Get_Raw(uint16_t *raw_x, uint16_t *raw_y)
{
    const uint8_t samples = 10;
    uint32_t sum_x = 0, sum_y = 0;

    HAL_GPIO_WritePin(TP_CS_PORT, TP_CS_PIN, GPIO_PIN_RESET);

    for (uint8_t i = 0; i < samples; i++)
    {
        TP_Write(CMD_RDY);
        sum_y += TP_Read();

        TP_Write(CMD_RDX);
        sum_x += TP_Read();
    }

    HAL_GPIO_WritePin(TP_CS_PORT, TP_CS_PIN, GPIO_PIN_SET);

    *raw_x = (uint16_t)(sum_x / samples);
    *raw_y = (uint16_t)(sum_y / samples);
}

static void Touch_Get_Screen_Coords(uint16_t *screen_x, uint16_t *screen_y)
{
    uint16_t raw_x, raw_y;
    Touch_Get_Raw(&raw_x, &raw_y);

    int32_t sx = ((int32_t)raw_y - TOUCH_RY_AT_X0) * 320
                 / (TOUCH_RY_AT_X320 - TOUCH_RY_AT_X0);
    int32_t sy = ((int32_t)raw_x - TOUCH_RX_AT_Y0) * 240
                 / (TOUCH_RX_AT_Y240 - TOUCH_RX_AT_Y0);

    if (sx < 0) sx = 0;
    if (sx > 320) sx = 320;
    if (sy < 0) sy = 0;
    if (sy > 240) sy = 240;

    *screen_x = (uint16_t)sx;
    *screen_y = (uint16_t)sy;
}

static uint8_t Point_In_Circle(uint16_t px, uint16_t py, uint16_t cx, uint16_t cy, uint16_t r)
{
    int32_t dx = (int32_t)px - (int32_t)cx;
    int32_t dy = (int32_t)py - (int32_t)cy;
    return (dx * dx + dy * dy) <= (int32_t)(r * r);
}

/* เช็คการแตะ 1 ครั้ง (rising edge only) และอัปเดตค่า/วาดใหม่เฉพาะแถวที่เปลี่ยน
   เมื่อถึง 100% แล้วแตะอีกครั้ง จะวนกลับไปที่ 0% */
void Handle_Touch(void)
{
    uint8_t isPressed = TP_Touchpad_Pressed();

    if (isPressed && !touchWasPressed)
    {
        uint16_t x, y;
        Touch_Get_Screen_Coords(&x, &y);

        if (Point_In_Circle(x, y, CIRC_X, ROW_R_Y + 20, CIRC_R))
        {
            if (redPct >= 100) redPct = 0;
            else                redPct += 10;
            Draw_RGB_Row(ROW_R_Y, RED, PALE_RED, redPct);
            Draw_Mixed_Circle(redPct, greenPct, bluePct);
        }
        else if (Point_In_Circle(x, y, CIRC_X, ROW_G_Y + 20, CIRC_R))
        {
            if (greenPct >= 100) greenPct = 0;
            else                  greenPct += 10;
            Draw_RGB_Row(ROW_G_Y, GREEN, PALE_GREEN, greenPct);
            Draw_Mixed_Circle(redPct, greenPct, bluePct);
        }
        else if (Point_In_Circle(x, y, CIRC_X, ROW_B_Y + 20, CIRC_R))
        {
            if (bluePct >= 100) bluePct = 0;
            else                 bluePct += 10;
            Draw_RGB_Row(ROW_B_Y, BLUE, PALE_BLUE, bluePct);
            Draw_Mixed_Circle(redPct, greenPct, bluePct);
        }
    }

    touchWasPressed = isPressed;
}

/* USER CODE END 0 */

int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  MPU_Config();
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */

  ILI9341_Init();
  ILI9341_Set_Rotation(SCREEN_HORIZONTAL_1);

  Draw_Screen1_Static();
  Draw_Mixed_Circle(redPct, greenPct, bluePct);
  Draw_RGB_Row(ROW_R_Y, RED,   PALE_RED,   redPct);
  Draw_RGB_Row(ROW_G_Y, GREEN, PALE_GREEN, greenPct);
  Draw_RGB_Row(ROW_B_Y, BLUE,  PALE_BLUE,  bluePct);

  lastSensorTick = HAL_GetTick();

  /* USER CODE END 2 */

  /* USER CODE BEGIN WHILE */
  while (1)
  {
    Handle_Touch();

    /* อ่านเซนเซอร์ทุก 2 วินาที แบบไม่บล็อก touch */
    if (HAL_GetTick() - lastSensorTick >= 2000)
    {
      sensorOK = AM2320_Read(&temperature, &humidity);
      if (sensorOK)
      {
        Draw_TempHumidity(temperature, humidity);
      }
      lastSensorTick = HAL_GetTick();
    }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 216;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  HAL_MPU_Disable();

  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif

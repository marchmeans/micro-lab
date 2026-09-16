/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "i2c.h"
#include "rng.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ILI9341_STM32_Driver.h"
#include "ILI9341_GFX.h"
#include "ILI9341_Touchscreen.h"
#include "photo.h"
#include <stdio.h>
#include <math.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum { SCR_1, SCR_2 } Screen_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define TOP_H     50
#define ROW_H     63
#define BAR_X0    40
#define BAR_X1    225
#define BAR_W     185
#define BAR_HH    8
#define CIRC_X    22
#define CIRC_R    13
#define ROW1_CY   81
#define ROW2_CY   144
#define ROW3_CY   207
#define MIX_CX    155
#define MIX_CY    25
#define MIX_R     18

// Touch calibration — raw ADC corner values from 4-point calibration on this board
// Re-run calibration if touch feels off (tap corners, read serial, update these 4 numbers)
#define TS_X_MIN    9     // pos[1] when tapping left edge
#define TS_X_RANGE  318   // (pos[1] at right edge 327) - TS_X_MIN
#define TS_Y_MAX    238   // pos[0] when tapping top edge (Y axis is inverted)
#define TS_Y_RANGE  221   // TS_Y_MAX - (pos[0] at bottom edge 17)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static float r_pct = 0.5f, g_pct = 0.3f, b_pct = 0.7f;
static float temp_c = 27.0f, hum_rh = 55.0f;
static Screen_t cur_scr = SCR_1;
static uint32_t scr2_tick = 0;
static uint32_t sensor_tick = 0;
static uint32_t touch_tick = 0;
static uint32_t display_tick = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
uint16_t CRC16_2(uint8_t *ptr, uint8_t length);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void draw_region(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, const uint16_t *data) {
    ILI9341_Set_Address(x0, y0, x1, y1);
    ILI9341_Write_Command(0x2C);
    uint32_t n = (uint32_t)(x1-x0+1) * (y1-y0+1);
    for (uint32_t i = 0; i < n; i++) {
        ILI9341_Write_Data(data[i] >> 8);
        ILI9341_Write_Data(data[i] & 0xFF);
    }
}

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint16_t)(r >> 3) << 11) | ((uint16_t)(g >> 2) << 5) | (b >> 3);
}

static uint8_t hit_circle(uint16_t tx, uint16_t ty, uint16_t cx, uint16_t cy, uint16_t r) {
    int32_t dx = (int32_t)tx - cx, dy = (int32_t)ty - cy;
    return dx*dx + dy*dy <= (int32_t)(r*r);
}

// Returns 1 if touch is on a bar row; writes 0.0–1.0 value based on X position
static uint8_t hit_bar(uint16_t tx, uint16_t ty, uint16_t cy, float *out) {
    if (tx < BAR_X0 || tx > BAR_X1) return 0;
    if (ty < cy - BAR_HH - 10 || ty > cy + BAR_HH + 10) return 0;
    *out = (float)(tx - BAR_X0) / (float)BAR_W;  // tx in [BAR_X0..BAR_X1] so result is always 0.0–1.0
    return 1;
}

static void draw_bar(uint16_t cy, float pct, uint16_t col) {
    uint16_t fw = (uint16_t)(pct * BAR_W);
    if (fw > 0)
        ILI9341_Draw_Filled_Rectangle_Coord(BAR_X0, cy - BAR_HH, BAR_X0 + fw, cy + BAR_HH, col);
    if (fw < BAR_W)
        ILI9341_Draw_Filled_Rectangle_Coord(BAR_X0 + fw, cy - BAR_HH, BAR_X1, cy + BAR_HH, LIGHTGREY);
}

static void update_scr1(void) {
    char buf[24];

    // Mixed colour circle — scale 0.0–1.0 to 0–255 per channel
    uint16_t mixed_color = rgb565(
        (uint8_t)(r_pct * 255),
        (uint8_t)(g_pct * 255),
        (uint8_t)(b_pct * 255)
    );
    ILI9341_Draw_Filled_Circle(MIX_CX, MIX_CY, MIX_R, mixed_color);
    ILI9341_Draw_Hollow_Circle(MIX_CX, MIX_CY, MIX_R, BLACK);

    // Temperature — multiply by 10 so snprintf can print one decimal without float support
    // e.g. 27.2°C → t10=272 → "%d.%d" with 27 and 2
    int32_t t10 = (int32_t)roundf(temp_c * 10);
    snprintf(buf, sizeof(buf), "%d.%dC ", t10 / 10, (t10 < 0 ? -t10 : t10) % 10);
    ILI9341_Draw_Text(buf, 5, 17, BLACK, 2, WHITE);

    // Humidity — same trick; always positive so no sign handling needed
    int32_t h10 = (int32_t)roundf(hum_rh * 10);
    snprintf(buf, sizeof(buf), "%d.%d%%RH", h10 / 10, h10 % 10);
    ILI9341_Draw_Text(buf, 175, 17, BLACK, 2, WHITE);

    // RGB bars and percentage labels
    draw_bar(ROW1_CY, r_pct, RED);
    draw_bar(ROW2_CY, g_pct, GREEN);
    draw_bar(ROW3_CY, b_pct, BLUE);

    float   *channels[3] = { &r_pct, &g_pct, &b_pct };
    uint16_t label_y[3]  = { ROW1_CY - BAR_HH, ROW2_CY - BAR_HH, ROW3_CY - BAR_HH };
    for (int i = 0; i < 3; i++) {
        snprintf(buf, 8, "%d%%", (int)(*channels[i] * 100));
        ILI9341_Draw_Text(buf, 230, label_y[i], BLACK, 1, WHITE);
    }
}

static void draw_scr1(void) {
    ILI9341_Fill_Screen(WHITE);
    ILI9341_Draw_Horizontal_Line(0, TOP_H, 320, DARKGREY);
    ILI9341_Draw_Horizontal_Line(0, TOP_H + ROW_H,     320, LIGHTGREY);
    ILI9341_Draw_Horizontal_Line(0, TOP_H + 2*ROW_H,   320, LIGHTGREY);
    ILI9341_Draw_Filled_Circle(CIRC_X, ROW1_CY, CIRC_R, RED);
    ILI9341_Draw_Filled_Circle(CIRC_X, ROW2_CY, CIRC_R, GREEN);
    ILI9341_Draw_Filled_Circle(CIRC_X, ROW3_CY, CIRC_R, BLUE);
    update_scr1();
}

static void draw_scr2(void) {
    uint16_t mc = rgb565((uint8_t)(r_pct*255), (uint8_t)(g_pct*255), (uint8_t)(b_pct*255));
    ILI9341_Fill_Screen(WHITE);
    draw_region(5, 10, 104, 129, photo_data);
    ILI9341_Draw_Text("Group No.12", 115, 30, mc, 2, WHITE);
    ILI9341_Draw_Text("Maris", 115, 68, mc, 2, WHITE);
    ILI9341_Draw_Text("Methamaneechote", 115, 98, mc, 2, WHITE);
    ILI9341_Draw_Text("67011162", 115, 128, mc, 2, WHITE);
    ILI9341_Draw_Text("Touch photo/5s->Scr1", 5, 170, DARKGREY, 1, WHITE);
}

static uint8_t read_am2320(void) {
    uint8_t cmd[3] = {0x03, 0x00, 0x04}, buf[8] = {0};
    HAL_I2C_Master_Transmit(&hi2c1, 0x5C << 1, NULL, 0, 200); // wake (expect NACK)
    HAL_Delay(1);
    if (HAL_I2C_Master_Transmit(&hi2c1, 0x5C << 1, cmd, 3, 200) != HAL_OK) return 0;
    HAL_Delay(2);
    if (HAL_I2C_Master_Receive(&hi2c1, 0x5C << 1, buf, 8, 200) != HAL_OK) return 0;
    uint16_t rcrc = (buf[7] << 8) | buf[6];
    if (rcrc != CRC16_2(buf, 6)) return 0;
    uint16_t raw_t = ((buf[4] & 0x7F) << 8) | buf[5];
    temp_c = (buf[4] & 0x80) ? -(raw_t / 10.0f) : (raw_t / 10.0f);
    hum_rh = ((buf[2] << 8) | buf[3]) / 10.0f;
    return 1;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_RNG_Init();
  MX_SPI5_Init();
  MX_TIM1_Init();
  MX_USART3_UART_Init();
  MX_I2C1_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
  ILI9341_Init();
  ILI9341_Set_Rotation(SCREEN_HORIZONTAL_1);
  draw_scr1();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint32_t now = HAL_GetTick();

    // Screen 2: auto-return after 5s
    if (cur_scr == SCR_2 && now - scr2_tick >= 5000) {
      cur_scr = SCR_1;
      draw_scr1();
    }

    // Backlight: potentiometer → 20-100% duty (ARR=999)
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 5) == HAL_OK) {
      uint32_t adc = HAL_ADC_GetValue(&hadc1);
      uint32_t duty = 200 + (adc * 800) / 4095;  // 200..999 = 20%..100%
      __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, duty);
    }
    HAL_ADC_Stop(&hadc1);

    // Sensor update every 2s (AM2320 min read interval)
    if (now - sensor_tick >= 2000) {
      sensor_tick = now;
      read_am2320();
    }

    // Display update every 100ms — keeps screen values current without flicker
    if (cur_scr == SCR_1 && now - display_tick >= 100) {
      display_tick = now;
      update_scr1();
    }

    // Touch (debounce 200ms)
    if (now - touch_tick >= 200 && TP_Touchpad_Pressed()) {
      touch_tick = now;
      uint16_t pos[2];
      if (TP_Read_Coordinates(pos) == TOUCHPAD_DATA_OK) {
        // Remap touch coords to screen coords (axes swapped + scaled from calibration)
        int32_t sx = ((int32_t)pos[1] - TS_X_MIN) * 320 / TS_X_RANGE;
        int32_t sy = (TS_Y_MAX - (int32_t)pos[0]) * 240 / TS_Y_RANGE;
        sx = sx < 0 ? 0 : sx > 319 ? 319 : sx;
        sy = sy < 0 ? 0 : sy > 239 ? 239 : sy;
        uint16_t tx = (uint16_t)sx, ty = (uint16_t)sy;
        if (cur_scr == SCR_1) {
          if (hit_circle(tx, ty, MIX_CX, MIX_CY, MIX_R + 5)) {
            // touch mixed-colour circle → switch to screen 2
            cur_scr = SCR_2; scr2_tick = now; draw_scr2();
          } else if (hit_bar(tx, ty, ROW1_CY, &r_pct)) {
            // touch red bar → set red from X position
          } else if (hit_bar(tx, ty, ROW2_CY, &g_pct)) {
            // touch green bar → set green from X position
          } else if (hit_bar(tx, ty, ROW3_CY, &b_pct)) {
            // touch blue bar → set blue from X position
          } else if (hit_circle(tx, ty, CIRC_X, ROW1_CY, CIRC_R+8)) {
            r_pct += 0.1f; if (r_pct > 1.01f) r_pct = 0.0f;
          } else if (hit_circle(tx, ty, CIRC_X, ROW2_CY, CIRC_R+8)) {
            g_pct += 0.1f; if (g_pct > 1.01f) g_pct = 0.0f;
          } else if (hit_circle(tx, ty, CIRC_X, ROW3_CY, CIRC_R+8)) {
            b_pct += 0.1f; if (b_pct > 1.01f) b_pct = 0.0f;
          }
        } else {
          // Screen 2: touch photo area → return
          if (tx >= 5 && tx <= 105 && ty >= 10 && ty <= 130) {
            cur_scr = SCR_1; draw_scr1();
          }
        }
      }
    }
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 200;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 9;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_6) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
uint16_t CRC16_2(uint8_t *ptr, uint8_t length) {
    uint16_t crc = 0xFFFF;
    while (length--) {
        crc ^= *ptr++;
        for (uint8_t s = 0; s < 8; s++) {
            if (crc & 0x01) { crc >>= 1; crc ^= 0xA001; }
            else              { crc >>= 1; }
        }
    }
    return crc;
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

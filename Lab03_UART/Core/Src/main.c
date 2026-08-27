/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  *                   Lab 3 - UART (Experiment 4: menu + LEDs)
  *                   01276314 Microcontroller Interfacing, KMITL
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
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "string.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* On-board LEDs of the Nucleo-F767ZI board (no external wiring needed) */
#define LED1_PORT   GPIOB
#define LED1_PIN    GPIO_PIN_0   /* LD1 - green */
#define LED2_PORT   GPIOB
#define LED2_PIN    GPIO_PIN_7   /* LD2 - blue  */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
char rx_buf;

char menu[] =
    "\r\n"
    "Display Blinking LED   PRESS (1, 2)\r\n"
    "Display Group Members  PRESS m\r\n"
    "Quit                   PRESS q\r\n";

char prompt[]   = "Input => ";
char newline[]  = "\r\n";
char quit_msg[] = "Quit\r\n";
char unknown[]  = "Unknown Command\r\n";

/* TODO: replace with your real group members (student ID + full name) */
char members[] =
    "67011162\r\nMaris Methamaneechote\r\n"
    "67011634\r\nPaphada Borisutsukkamol\r\n";

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */
static void UART_Print(char *s);
static void UART_PutChar(char c);
static char UART_GetChar(void);
static void Blink_LED(GPIO_TypeDef *port, uint16_t pin, uint8_t times, uint32_t ms);
static void LED_GPIO_Init(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* Wait until the previous transmission is done (TC=1), then send a string */
static void UART_Print(char *s)
{
    while (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_TC) == RESET) {}
    HAL_UART_Transmit(&huart3, (uint8_t *)s, strlen(s), 1000);
}

/* Send a single character */
static void UART_PutChar(char c)
{
    while (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_TC) == RESET) {}
    HAL_UART_Transmit(&huart3, (uint8_t *)&c, 1, 1000);
}

/* Wait until a byte is available (RXNE=1), then read and return it */
static char UART_GetChar(void)
{
    char c;
    while (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_RXNE) == RESET) {}
    HAL_UART_Receive(&huart3, (uint8_t *)&c, 1, 1000);
    return c;
}

/* Blink one LED 'times' times with 'ms' delay on each state */
static void Blink_LED(GPIO_TypeDef *port, uint16_t pin, uint8_t times, uint32_t ms)
{
    for (uint8_t i = 0; i < times; i++)
    {
        HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
        HAL_Delay(ms);
        HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
        HAL_Delay(ms);
    }
}

/* Configure PB0 / PB7 as push-pull outputs for the on-board LEDs.
   Done here so you do NOT have to add the LEDs in CubeMX. */
static void LED_GPIO_Init(void)
{
    GPIO_InitTypeDef led = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOB, LED1_PIN | LED2_PIN, GPIO_PIN_RESET);

    led.Pin   = LED1_PIN | LED2_PIN;
    led.Mode  = GPIO_MODE_OUTPUT_PP;
    led.Pull  = GPIO_NOPULL;
    led.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &led);
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

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

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
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  LED_GPIO_Init();

  /* Show the full menu once at start-up */
  UART_Print(menu);
  UART_Print(prompt);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      /* 1. Wait for one character from the terminal */
      rx_buf = UART_GetChar();

      /* 2. Echo it back, then move to a new line */
      UART_PutChar(rx_buf);
      UART_Print(newline);

      /* 3. Perform the task for that character (Table 6.1) */
      switch (rx_buf)
      {
          case '1':
              Blink_LED(LED1_PORT, LED1_PIN, 3, 300);
              break;

          case '2':
              Blink_LED(LED2_PORT, LED2_PIN, 3, 300);
              break;

          case 'm':
              UART_Print(members);
              break;

          case 'q':
              UART_Print(quit_msg);
              break;                 /* leave the switch, then exit below */

          default:
              UART_Print(unknown);
              break;
      }

      /* 4. On 'q': stop responding (stop TX/RX) */
      if (rx_buf == 'q')
      {
          break;                     /* exits the while(1) loop */
      }

      /* 5. Otherwise prompt again with only "Input => " */
      UART_Print(prompt);
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
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
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

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

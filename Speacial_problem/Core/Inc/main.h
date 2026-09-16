/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define T_IRQ_Pin GPIO_PIN_2
#define T_IRQ_GPIO_Port GPIOE
#define T_CLK_Pin GPIO_PIN_3
#define T_CLK_GPIO_Port GPIOE
#define T_DO_Pin GPIO_PIN_4
#define T_DO_GPIO_Port GPIOE
#define T_DIN_Pin GPIO_PIN_5
#define T_DIN_GPIO_Port GPIOE
#define T_CS_Pin GPIO_PIN_6
#define T_CS_GPIO_Port GPIOE
#define CS_Pin GPIO_PIN_0
#define CS_GPIO_Port GPIOG
#define DC_Pin GPIO_PIN_1
#define DC_GPIO_Port GPIOG
#define RES_Pin GPIO_PIN_1
#define RES_GPIO_Port GPIOD

/* USER CODE BEGIN Private defines */
#define RST_Pin          RES_Pin
#define RST_GPIO_Port    RES_GPIO_Port
#define T_MISO_Pin       T_DO_Pin
#define T_MISO_GPIO_Port T_DO_GPIO_Port
#define T_MOSI_Pin       T_DIN_Pin
#define T_MOSI_GPIO_Port T_DIN_GPIO_Port
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

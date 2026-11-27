/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : 三灯流水灯 —— 引脚宏定义
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

void Error_Handler(void);

/* Private defines -----------------------------------------------------------*/
/* 三个 LED：PB0 / PB1 / PB2（CubeMX 里 User Label 命名为 LED0 / LED1 / LED2）*/
#define LED0_Pin GPIO_PIN_0
#define LED0_GPIO_Port GPIOA
#define LED1_Pin GPIO_PIN_1
#define LED1_GPIO_Port GPIOA
#define LED2_Pin GPIO_PIN_2
#define LED2_GPIO_Port GPIOA

#define KEY_DIR_Pin GPIO_PIN_0
#define KEY_DIR_GPIO_Port GPIOB
#define KEY_RUN_Pin GPIO_PIN_1
#define KEY_RUN_GPIO_Port GPIOB

#define OLED_SCL_Pin GPIO_PIN_8
#define OLED_SCL_GPIO_Port GPIOB
#define OLED_SDA_Pin GPIO_PIN_9
#define OLED_SDA_GPIO_Port GPIOB

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

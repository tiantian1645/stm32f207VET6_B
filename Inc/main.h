/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.h
 * @brief          : Header for main.c file.
 *                   This file contains the common defines of the application.
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
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
#include "stm32f2xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"
#include "string.h"
#include "error.h"
#include "protocol.h"

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */
/* 任务优先级 定时器任务优先级参考 configTIMER_TASK_PRIORITY */
#define TASK_PRIORITY_COMM_OUT_TX 3
#define TASK_PRIORITY_COMM_MAIN_TX 4
#define TASK_PRIORITY_COMM_DATA_TX 5
#define TASK_PRIORITY_STORGE 6
#define TASK_PRIORITY_MOTOR 8

/* 应用程序版本 单精度浮点数 */
#define APP_VERSION ((float)0.272)

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef * htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
void FL_Error_Handler(char * file, int line);
uint8_t GetHardwareVersion(void);
void reset_Tim1(void);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define OPTSW_OUT2_Pin GPIO_PIN_2
#define OPTSW_OUT2_GPIO_Port GPIOE
#define OPTSW_OUT2_EXTI_IRQn EXTI2_IRQn
#define OPTSW_OUT3_Pin GPIO_PIN_3
#define OPTSW_OUT3_GPIO_Port GPIOE
#define OPTSW_OUT4_Pin GPIO_PIN_4
#define OPTSW_OUT4_GPIO_Port GPIOE
#define OPTSW_OUT5_Pin GPIO_PIN_5
#define OPTSW_OUT5_GPIO_Port GPIOE
#define BUZZ_ON_Pin GPIO_PIN_6
#define BUZZ_ON_GPIO_Port GPIOE
#define STEP_NCS1_Pin GPIO_PIN_13
#define STEP_NCS1_GPIO_Port GPIOC
#define STEP_NFLG1_Pin GPIO_PIN_14
#define STEP_NFLG1_GPIO_Port GPIOC
#define ADC_NTC_TOP4_Pin GPIO_PIN_0
#define ADC_NTC_TOP4_GPIO_Port GPIOC
#define ADC_NTC_TOP2_Pin GPIO_PIN_1
#define ADC_NTC_TOP2_GPIO_Port GPIOC
#define ADC_NTC_TOP5_Pin GPIO_PIN_2
#define ADC_NTC_TOP5_GPIO_Port GPIOC
#define ADC_NTC_TOP3_Pin GPIO_PIN_3
#define ADC_NTC_TOP3_GPIO_Port GPIOC
#define ADC_NTC_TOP1_Pin GPIO_PIN_0
#define ADC_NTC_TOP1_GPIO_Port GPIOA
#define ADC_NTC_TOP6_Pin GPIO_PIN_1
#define ADC_NTC_TOP6_GPIO_Port GPIOA
#define ADC_NTC_BTM1_Pin GPIO_PIN_2
#define ADC_NTC_BTM1_GPIO_Port GPIOA
#define ADC_NTC_BTM2_Pin GPIO_PIN_3
#define ADC_NTC_BTM2_GPIO_Port GPIOA
#define ADC_NTC_ENV_Pin GPIO_PIN_4
#define ADC_NTC_ENV_GPIO_Port GPIOA
#define SPI1_SCK_Pin GPIO_PIN_5
#define SPI1_SCK_GPIO_Port GPIOA
#define SPI1_MISO_Pin GPIO_PIN_6
#define SPI1_MISO_GPIO_Port GPIOA
#define SPI1_MOSI_Pin GPIO_PIN_7
#define SPI1_MOSI_GPIO_Port GPIOA
#define BC_AIM_WK_N_Pin GPIO_PIN_5
#define BC_AIM_WK_N_GPIO_Port GPIOC
#define STEP_STEP_Pin GPIO_PIN_9
#define STEP_STEP_GPIO_Port GPIOE
#define STEP_NRST_Pin GPIO_PIN_10
#define STEP_NRST_GPIO_Port GPIOE
#define STEP_NCS2_Pin GPIO_PIN_12
#define STEP_NCS2_GPIO_Port GPIOE
#define STEP_DIR2_Pin GPIO_PIN_13
#define STEP_DIR2_GPIO_Port GPIOE
#define BC_TRIG_N_Pin GPIO_PIN_14
#define BC_TRIG_N_GPIO_Port GPIOE
#define STEP_NFLG2_Pin GPIO_PIN_15
#define STEP_NFLG2_GPIO_Port GPIOE
#define BC_RXD_Pin GPIO_PIN_10
#define BC_RXD_GPIO_Port GPIOB
#define BC_TXD_Pin GPIO_PIN_11
#define BC_TXD_GPIO_Port GPIOB
#define MOT_NRST_Pin GPIO_PIN_12
#define MOT_NRST_GPIO_Port GPIOB
#define MOT_SCK_Pin GPIO_PIN_13
#define MOT_SCK_GPIO_Port GPIOB
#define MOT_SDO_Pin GPIO_PIN_14
#define MOT_SDO_GPIO_Port GPIOB
#define MOT_SDI_Pin GPIO_PIN_15
#define MOT_SDI_GPIO_Port GPIOB
#define MOT_NCS1_Pin GPIO_PIN_8
#define MOT_NCS1_GPIO_Port GPIOD
#define MOT_NCS2_Pin GPIO_PIN_9
#define MOT_NCS2_GPIO_Port GPIOD
#define MOT_NBUSY1_Pin GPIO_PIN_10
#define MOT_NBUSY1_GPIO_Port GPIOD
#define MOT_NFLG2_Pin GPIO_PIN_11
#define MOT_NFLG2_GPIO_Port GPIOD
#define MOT_NFLG1_Pin GPIO_PIN_12
#define MOT_NFLG1_GPIO_Port GPIOD
#define MOT_NBUSY2_Pin GPIO_PIN_13
#define MOT_NBUSY2_GPIO_Port GPIOD
#define HEATER_BTM_Pin GPIO_PIN_15
#define HEATER_BTM_GPIO_Port GPIOD
#define FAN_PWM_Pin GPIO_PIN_6
#define FAN_PWM_GPIO_Port GPIOC
#define FAN_EN_Pin GPIO_PIN_7
#define FAN_EN_GPIO_Port GPIOC
#define HEATER_TOP_Pin GPIO_PIN_8
#define HEATER_TOP_GPIO_Port GPIOC
#define LAMP1_Pin GPIO_PIN_8
#define LAMP1_GPIO_Port GPIOA
#define CORE_RXD_Pin GPIO_PIN_9
#define CORE_RXD_GPIO_Port GPIOA
#define CORE_TXD_Pin GPIO_PIN_10
#define CORE_TXD_GPIO_Port GPIOA
#define LAMP2_Pin GPIO_PIN_11
#define LAMP2_GPIO_Port GPIOA
#define LAMP3_Pin GPIO_PIN_12
#define LAMP3_GPIO_Port GPIOA
#define SPI1_NSS_Pin GPIO_PIN_15
#define SPI1_NSS_GPIO_Port GPIOA
#define LED_RUN_Pin GPIO_PIN_10
#define LED_RUN_GPIO_Port GPIOC
#define MCU_TXD_Pin GPIO_PIN_12
#define MCU_TXD_GPIO_Port GPIOC
#define MCU_RXD_Pin GPIO_PIN_2
#define MCU_RXD_GPIO_Port GPIOD
#define FRONT_RESET_Pin GPIO_PIN_3
#define FRONT_RESET_GPIO_Port GPIOD
#define FRONT_TRIG_IN_Pin GPIO_PIN_4
#define FRONT_TRIG_IN_GPIO_Port GPIOD
#define FRONT_TRIG_IN_EXTI_IRQn EXTI4_IRQn
#define FRONT_RXD_Pin GPIO_PIN_5
#define FRONT_RXD_GPIO_Port GPIOD
#define FRONT_TXD_Pin GPIO_PIN_6
#define FRONT_TXD_GPIO_Port GPIOD
#define FRONT_STATUS_Pin GPIO_PIN_7
#define FRONT_STATUS_GPIO_Port GPIOD
#define CARD_IN_Pin GPIO_PIN_5
#define CARD_IN_GPIO_Port GPIOB
#define CARD_I2C_SCL_Pin GPIO_PIN_6
#define CARD_I2C_SCL_GPIO_Port GPIOB
#define CARD_I2C_SDA_Pin GPIO_PIN_7
#define CARD_I2C_SDA_GPIO_Port GPIOB
#define STEP_DIR1_Pin GPIO_PIN_9
#define STEP_DIR1_GPIO_Port GPIOB
#define OPTSW_OUT0_Pin GPIO_PIN_0
#define OPTSW_OUT0_GPIO_Port GPIOE
#define OPTSW_OUT1_Pin GPIO_PIN_1
#define OPTSW_OUT1_GPIO_Port GPIOE
/* USER CODE BEGIN Private defines */
#define ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

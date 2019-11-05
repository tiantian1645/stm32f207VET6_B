/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __M_DRV8824_H
#define __M_DRV8824_H

/* Includes ------------------------------------------------------------------*/
#include "motor.h"

/* Private includes ----------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/
#define STEP_TIM_PSC (1 - 1)
#define STEP_TIM_ARR (0xFFFF)
#define STEP_TIM_RCR (1 - 1)
#define STEP_TIM_PUL (0xFFF)

#define STEP_BASIC_FREQ (120000)
#define STEP_REAL_FREQ (72000)

/* Exported types ------------------------------------------------------------*/
typedef enum {
    eM_DRV8824_Index_0 = 0,
    eM_DRV8824_Index_1 = 1,
} eM_DRV8824_Index;

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void m_drv8824_Init(void);
void m_drv8824_SetDir(eMotorDir dir);
uint8_t m_drv8824_Get_Flag(void);
void m_drv8824_Reset_All(void);

uint8_t m_drv8824_release(void);
uint8_t m_drv8824_release_ISR(void);
uint8_t m_drv8824_Index_Switch(eM_DRV8824_Index index, uint32_t timeout);
uint8_t m_drv8824_Clear_Flag(void);

uint32_t gPWM_TEST_AW_CNT_Get(void);
void gPWM_TEST_AW_CNT_Inc(void);
void gPWM_TEST_AW_CNT_Clear(void);

void PWM_AW_Stop(void);
uint8_t PWM_AW_IRQ_CallBcak(void);

/* Private defines -----------------------------------------------------------*/

#endif

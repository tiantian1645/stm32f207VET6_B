/* Define to prevent recursive inclusion -------------------------------------*/

/* Includes ------------------------------------------------------------------*/
#include "motor.h"

/* Private includes ----------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/
#define STEP_TIM_PSC (12000 - 1)
#define STEP_TIM_ARR (0xFFFF)
#define STEP_TIM_RCR (1 - 1)

/* Exported types ------------------------------------------------------------*/
typedef enum {
    eM_DRV8824_Index_0 = 0,
    eM_DRV8824_Index_1 = 1,
} eM_DRV8824_Index;

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void m_drv8824_Init(void);
void m_drv8824_SetDir(eMotorDir dir);
uint8_t m_drv8824_Index_Switch(eM_DRV8824_Index index, uint32_t timeout);
void PWM_AW_IRQ_CallBcak(void);
HAL_StatusTypeDef PWM_Start_AW(void);

/* Private defines -----------------------------------------------------------*/

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __HEAT_MOTOR_H
#define __HEAT_MOTOR_H

/* Includes ------------------------------------------------------------------*/
#include "motor.h"

/* Private includes ----------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/
#define heat_Motor_Up() heat_Motor_Run(eMotorDir_FWD, 3000)
#define heat_Motor_Down() heat_Motor_Run(eMotorDir_REV, 3000)

/* Exported types ------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
eMotorDir gHeat_Motor_Dir_Get(void);

void heat_Motor_Active(void);
void heat_Motor_Deactive(void);

uint8_t heat_Motor_Position_Is_Down(void);
uint8_t heat_Motor_Position_Is_Up(void);
uint8_t heat_Motor_Run(eMotorDir dir, uint32_t timeout);
uint8_t heat_Motor_Wait_Stop(uint32_t timeout);
uint8_t heat_Motor_PWM_Gen_Up(void);
uint8_t heat_Motor_PWM_Gen_Down(void);

/* Private defines -----------------------------------------------------------*/

#endif

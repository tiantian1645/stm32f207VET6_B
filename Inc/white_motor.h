/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __WHITE_MOTOR_H
#define __WHITE_MOTOR_H

/* Includes ------------------------------------------------------------------*/
#include "motor.h"

/* Private includes ----------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/
#define WHITE_MOTOR_RUN_PERIOD 4000

#define WHITE_MOTOR_RUN_PD_TIMEOUT 600
#define WHITE_MOTOR_RUN_WH_TIMEOUT 1000

#define white_Motor_PD() white_Motor_Run(eMotorDir_REV, WHITE_MOTOR_RUN_PD_TIMEOUT)
#define white_Motor_WH() white_Motor_Run(eMotorDir_FWD, WHITE_MOTOR_RUN_WH_TIMEOUT)

/* Exported types ------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
eMotorDir gWhite_Motor_Dir_Get(void);

void white_Motor_Active(void);
void white_Motor_Deactive(void);

uint8_t white_Motor_Position_Is_Down(void);
uint8_t white_Motor_Position_Is_In(void);
uint8_t white_Motor_Run(eMotorDir dir, uint32_t timeout);
uint8_t white_Motor_Toggle(uint32_t timeout);
uint8_t white_Motor_Wait_Stop(uint32_t timeout);
uint8_t white_Motor_PWM_Gen_In(void);
uint8_t white_Motor_PWM_Gen_Out(void);

/* Private defines -----------------------------------------------------------*/

#endif

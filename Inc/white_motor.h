/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __WHITE_MOTOR_H
#define __WHITE_MOTOR_H

/* Includes ------------------------------------------------------------------*/
#include "motor.h"

/* Private includes ----------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/
#define WHITE_MOTOR_RUN_PD_TIMEOUT 450 /* 关联 COMM_DATA_PD_TIMER_PERIOD */
#define WHITE_MOTOR_RUN_WH_TIMEOUT 1200

/* Exported types ------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
eMotorDir gWhite_Motor_Dir_Get(void);

void white_Motor_Active(void);
void white_Motor_Deactive(void);

uint8_t white_Motor_Position_Is_Down(void);
uint8_t white_Motor_Position_Is_In(void);
uint8_t white_Motor_Toggle();
uint8_t white_Motor_Wait_Stop(uint32_t timeout);
uint8_t white_Motor_PWM_Gen_In(void);
uint8_t white_Motor_PWM_Gen_Out(void);

void whilte_Motor_Init(void);

uint8_t white_Motor_PD(void);
uint8_t white_Motor_WH(void);
/* Private defines -----------------------------------------------------------*/

#endif

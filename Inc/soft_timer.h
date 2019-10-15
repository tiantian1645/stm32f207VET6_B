/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __SOFT_TIMER_H
#define __SOFT_TIMER_H
/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void soft_timer_Init(void);
void soft_timer_Temp_Pause(void);
void soft_timer_Temp_Resume(void);
BaseType_t soft_timer_Temp_IsActive(void);
void soft_timer_Temp_Comm_Set(eProtocol_COMM_Index comm_index, uint8_t sw);

/* Private defines -----------------------------------------------------------*/

#endif

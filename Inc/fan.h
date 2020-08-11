/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __FAN_H
#define __FAN_H

/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/
#define FAN_PSC (108 - 1) /* TIM8 APB2 108MHz / 108 = 1000000Hz */
#define FAN_ARR (250 - 1)
#define FAN_CCR ((FAN_ARR + 1) / 2)

#define FAN_IC_PSC (65536 - 1) /* TIM10 APB2 108MHz */
#define FAN_IC_ARR (65536 - 1) /* 16 bits */
#define FAN_IC_IF (0b1111)     /* 4 bits */

/* Exported types ------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void fan_Init(void);

void fan_Ctrl_Deal(float temp_env);

void fan_IC_Error_Report_Enable(void);
void fan_IC_Error_Report_Disable(void);
void fan_IC_Error_Deal(void);

/* Private defines -----------------------------------------------------------*/

#endif

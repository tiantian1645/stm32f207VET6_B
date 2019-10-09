/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __FAN_H
#define __FAN_H

/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/
#define FAN_PSC (2000 - 1) /* TIM8 APB2 120MHz / 2000 = 60000Hz */
#define FAN_ARR (60000 - 1)
#define FAN_CCR ((FAN_ARR + 1) / 2)

/* Exported types ------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void fan_Start(void);
void fan_Stop(void);
void fan_Adjust(float rate);

/* Private defines -----------------------------------------------------------*/

#endif

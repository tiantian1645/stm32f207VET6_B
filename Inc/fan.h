/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __FAN_H
#define __FAN_H

/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/
#define FAN_PSC (120 - 1) /* TIM8 APB2 120MHz / 120 = 1000000Hz */
#define FAN_ARR (250 - 1)
#define FAN_CCR ((FAN_ARR + 1) / 2)

/* Exported types ------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void fan_Start(void);
void fan_Stop(void);
void fan_Adjust(float rate);

/* Private defines -----------------------------------------------------------*/

#endif

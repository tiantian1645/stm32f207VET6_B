/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __HEATER_H
#define __HEATER_H
/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/
#define HEATER_BTM_PSC (12000 - 1)
#define HEATER_BTM_ARR (10000 - 1)
#define HEATER_BTM_CCR (8000)

/* Exported types ------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void beater_BTM_Output_Ctl(float pr);
void beater_BTM_Output_Start(void);
void beater_BTM_Output_Stop(void);

void heater_BTM_Output_Init(void);
void heater_BTM_Output_Keep_Deal(void);

/* Private defines -----------------------------------------------------------*/

#endif

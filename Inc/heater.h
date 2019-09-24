/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __HEATER_H
#define __HEATER_H
/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/
#define HEATER_BTM_PSC (12 - 1)
#define HEATER_BTM_ARR (10000 - 1)
#define HEATER_BTM_CCR (5000)

#define HEATER_TOP_PSC (12 - 1)
#define HEATER_TOP_ARR (10000 - 1)
#define HEATER_TOP_CCR (4000)

/* Exported types ------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void beater_BTM_Output_Ctl(float pr);
void beater_BTM_Output_Start(void);
void beater_BTM_Output_Stop(void);

void heater_BTM_Output_Init(void);
void heater_BTM_Output_Keep_Deal(void);

void heater_BTM_Log_PID(void);

void beater_TOP_Output_Ctl(float pr);
void beater_TOP_Output_Start(void);
void beater_TOP_Output_Stop(void);

void heater_TOP_Output_Init(void);
void heater_TOP_Output_Keep_Deal(void);

void heater_TOP_Log_PID(void);

/* Private defines -----------------------------------------------------------*/

#endif

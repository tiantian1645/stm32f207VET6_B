/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __HEATER_H
#define __HEATER_H
/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/
#define HEATER_BTM_PSC (12 - 1)
#define HEATER_BTM_ARR (10000 - 1)
#define HEATER_BTM_CCR (10000 - 1)

#define HEATER_TOP_PSC (12 - 1)
#define HEATER_TOP_ARR (10000 - 1)
#define HEATER_TOP_CCR (10000 - 1)

#define HEATER_BTM_DEFAULT_SETPOINT (37)
#define HEATER_TOP_DEFAULT_SETPOINT (37)

/* Exported types ------------------------------------------------------------*/
typedef enum {
    eHeater_PID_Conf_Kp,
    eHeater_PID_Conf_Ki,
    eHeater_PID_Conf_Kd,
    eHeater_PID_Conf_Set_Point,
    eHeater_PID_Conf_Input,
    eHeater_PID_Conf_Output,
} eHeater_PID_Conf;

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void heater_BTM_Output_Ctl(float pr);
void heater_BTM_Output_Start(void);
void heater_BTM_Output_Stop(void);
uint8_t heater_BTM_Output_Is_Live(void);

void heater_BTM_Output_Init(void);
void heater_BTM_Output_Keep_Deal(void);

void heater_BTM_Log_PID(void);

void heater_TOP_Output_Ctl(float pr);
void heater_TOP_Output_Start(void);
void heater_TOP_Output_Stop(void);
uint8_t heater_TOP_Output_Is_Live(void);

void heater_TOP_Output_Init(void);
void heater_TOP_Output_Keep_Deal(void);

void heater_TOP_Log_PID(void);

float heater_BTM_Conf_Get(eHeater_PID_Conf offset);
void heater_BTM_Conf_Set(eHeater_PID_Conf offset, float data);
float heater_TOP_Conf_Get(eHeater_PID_Conf offset);
void heater_TOP_Conf_Set(eHeater_PID_Conf offset, float data);

/* Private defines -----------------------------------------------------------*/

#endif

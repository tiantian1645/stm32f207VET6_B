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

#define HEATER_BTM_OUTDOOR_SETPOINT (37.2)
#define HEATER_TOP_OUTDOOR_SETPOINT (37.5)

#define HEATER_BTM_OVERSHOOT_LEVEL_TIMEOUT (12) /* 下加热体过冲目标温度持续时间 */
#define HEATER_BTM_OVERSHOOT_TARGET (37.2)      /* 下加热体过冲目标温度 */
#define HEATER_BTM_OVERSHOOT_TIMEOUT (72)       /* 下加热体过冲持续时间 */

#define HEATER_TOP_OVERSHOOT_LEVEL_TIMEOUT (12) /* 上加热体过冲目标温度持续时间 */
#define HEATER_TOP_OVERSHOOT_TARGET (37.3)      /* 上加热体过冲目标温度 */
#define HEATER_TOP_OVERSHOOT_TIMEOUT (72)       /* 上加热体过冲持续时间 */

/* Exported types ------------------------------------------------------------*/
typedef enum {
    eHeater_PID_Conf_Kp,
    eHeater_PID_Conf_Ki,
    eHeater_PID_Conf_Kd,
    eHeater_PID_Conf_Set_Point,
    eHeater_PID_Conf_Input,
    eHeater_PID_Conf_Output,
    eHeater_PID_Conf_Min_Output,
    eHeater_PID_Conf_Max_Output,
} eHeater_PID_Conf;

typedef enum {
    eHeater_BTM,
    eHeater_TOP,
} eHeater_Index;

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
uint8_t heater_Overshoot_Flag_Get(eHeater_Index idx);
void heater_Overshoot_Flag_Set(eHeater_Index idx, uint8_t flag);

uint8_t heater_Outdoor_Flag_Get(eHeater_Index idx);
void heater_Outdoor_Flag_Set(eHeater_Index idx, uint8_t flag);

float heater_BTM_Setpoint_Get(void);
void heater_BTM_Setpoint_Set(float setpoint);
void heater_BTM_Output_Ctl(float pr);
void heater_BTM_Output_Start(void);
void heater_BTM_Output_Stop(void);
uint8_t heater_BTM_Output_Is_Live(void);

void heater_BTM_Output_Init(void);
void heater_BTM_Output_Keep_Deal(void);

void heater_BTM_Log_PID(void);

float heater_TOP_Setpoint_Get(void);
void heater_TOP_Setpoint_Set(float setpoint);
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

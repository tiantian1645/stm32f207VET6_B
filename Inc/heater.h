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

#define HEATER_BTM_SAMPLE 100
#define HEATER_TOP_SAMPLE 100

#define HEATER_BTM_DEFAULT_SETPOINT (37)
#define HEATER_TOP_DEFAULT_SETPOINT (37)

#define HEATER_BTM_OUTDOOR_SETPOINT (37.3)
#define HEATER_TOP_OUTDOOR_SETPOINT (37.3)

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

typedef struct {
    uint32_t start;       /* 起始时刻 毫秒 */
    float peak_delta;     /* 过冲目标温度偏差 */
    float level_duration; /* 过冲目标温度维持时间 单位:秒 */
    float whole_duration; /* 过冲过程持续时间 单位:秒 */
    float pk;             /* f(x) = a * ln(kx + b) + c */
    float pb;             /* f(x) = a * ln(kx + b) + c */
    float pa;             /* f(x) = a * ln(kx + b) + c */
    float pc;             /* f(x) = a * ln(kx + b) + c */
} sHeater_Overshoot;

typedef enum {
    eHeater_Overshoot_Param_peak_delta,
    eHeater_Overshoot_Param_level_duration,
    eHeater_Overshoot_Param_whole_duration,
    eHeater_Overshoot_Param_pk,
    eHeater_Overshoot_Param_pb,
    eHeater_Overshoot_Param_pa,
    eHeater_Overshoot_Param_pc,
} eHeater_Overshoot_Param_Index;

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void heater_Overshoot_Init(float env);
void heater_Overshoot_Handle(void);
void heater_Overshoot_Set_Parmer(eHeater_Index bt_idx, eHeater_Overshoot_Param_Index p_idx, float data);
float heater_Overshoot_Get_Parmer(eHeater_Index bt_idx, eHeater_Overshoot_Param_Index p_idx);
void heater_Overshoot_Set_All(eHeater_Index bt_idx, uint8_t * pBuffer);
void heater_Overshoot_Get_All(eHeater_Index bt_idx, uint8_t * pBuffer);

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
void heater_BTM_Output_PID_Adapt(float env_temp);
void heater_BTM_Output_Keep_Deal(void);

void heater_BTM_Log_PID(void);

float heater_TOP_Setpoint_Get(void);
void heater_TOP_Setpoint_Set(float setpoint);
void heater_TOP_Output_Ctl(float pr);
void heater_TOP_Output_Start(void);
void heater_TOP_Output_Stop(void);
uint8_t heater_TOP_Output_Is_Live(void);

void heater_TOP_Output_Init(void);
void heater_TOP_Output_PID_Adapt(float env_temp);
void heater_TOP_Output_Keep_Deal(void);

void heater_TOP_Log_PID(void);

float heater_BTM_Conf_Get(eHeater_PID_Conf offset);
void heater_BTM_Conf_Set(eHeater_PID_Conf offset, float data);
float heater_TOP_Conf_Get(eHeater_PID_Conf offset);
void heater_TOP_Conf_Set(eHeater_PID_Conf offset, float data);

/* Private defines -----------------------------------------------------------*/

#endif

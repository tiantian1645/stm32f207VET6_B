/**
 * @file    heater.c
 * @brief   上下加热体控制控制
 *
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "temperature.h"
#include "heater.h"
#include "pid_ctrl.h"
#include "storge_task.h"

/* Extern variables ----------------------------------------------------------*/
extern TIM_HandleTypeDef htim4;
extern TIM_HandleTypeDef htim3;

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/
#define HEATER_BTM_SAMPLE 10
#define HEATER_TOP_SAMPLE 10

#define HEATER_BTM_TIM htim4
#define HEATER_BTM_CHN TIM_CHANNEL_4
#define HEATER_TOP_TIM htim3
#define HEATER_TOP_CHN TIM_CHANNEL_3
/* Private variables ---------------------------------------------------------*/
static sPID_Ctrl_Conf gHeater_BTM_PID_Conf;
static sPID_Ctrl_Conf gHeater_TOP_PID_Conf;

static uint8_t gHeater_Overshoot_Flag = 0;
// Control loop input,output and setpoint variables
static float btm_input = 0, btm_output = 0, btm_setpoint = HEATER_BTM_DEFAULT_SETPOINT;
static float top_input = 0, top_output = 0, top_setpoint = HEATER_TOP_DEFAULT_SETPOINT;

/* Private constants ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
static float heater_PID_Conf_Param_Get(sPID_Ctrl_Conf * pConf, eHeater_PID_Conf offset);
static void heater_PID_Conf_Param_Set(sPID_Ctrl_Conf * pConf, eHeater_PID_Conf offset, float data);

/* Private user code ---------------------------------------------------------*/

/**
 * @brief  过冲标志 获取
 * @retval gHeater_Overshoot_Flag
 */
uint8_t heater_Overshoot_Flag_Get(eHeater_Index idx)
{
    return ((gHeater_Overshoot_Flag & (1 << idx)) > 0) ? (1) : (0);
}

/**
 * @brief  过冲标志 设置
 * @param  flag 设置值
 * @retval 参数数值
 */
void heater_Overshoot_Flag_Set(eHeater_Index idx, uint8_t flag)
{
    if (flag > 0) {
        gHeater_Overshoot_Flag |= (1 << idx);
    } else {
        gHeater_Overshoot_Flag &= (0xFF - (1 << idx));
    }
}

/**
 * @brief  目标值获取 下加热体
 * @retval 参数数值
 */
float heater_BTM_Setpoint_Get(void)
{
    return btm_setpoint;
}

/**
 * @brief  目标值设置 下加热体
 * @param  setpoint 设置值
 * @retval 参数数值
 */
void heater_BTM_Setpoint_Set(float setpoint)
{
    if (setpoint >= HEATER_BTM_DEFAULT_SETPOINT - 1.5 && setpoint <= HEATER_BTM_DEFAULT_SETPOINT + 1.5) {
        btm_setpoint = setpoint;
    }
}

/**
 * @brief  目标值获取 上加热体
 * @retval 参数数值
 */
float heater_TOP_Setpoint_Get(void)
{
    return top_setpoint;
}

/**
 * @brief  目标值设置 上加热体
 * @param  setpoint 设置值
 * @retval 参数数值
 */
void heater_TOP_Setpoint_Set(float setpoint)
{
    if (setpoint >= HEATER_TOP_DEFAULT_SETPOINT - 1.5 && setpoint <= HEATER_TOP_DEFAULT_SETPOINT + 1.5) {
        top_setpoint = setpoint;
    }
}

/**
 * @brief  PID参数 读取
 * @param  pConf  参数结构体指针
 * @param  offset 参数项别
 * @retval 参数数值
 */
static float heater_PID_Conf_Param_Get(sPID_Ctrl_Conf * pConf, eHeater_PID_Conf offset)
{
    switch (offset) {
        case eHeater_PID_Conf_Kp:
            return pConf->Kp;
        case eHeater_PID_Conf_Ki:
            return pConf->Ki;
        case eHeater_PID_Conf_Kd:
            return pConf->Kd;
        case eHeater_PID_Conf_Set_Point:
            return *(pConf->setpoint);
        case eHeater_PID_Conf_Input:
            return *(pConf->input);
        case eHeater_PID_Conf_Output:
            return *(pConf->output);
        case eHeater_PID_Conf_Min_Output:
            return pConf->omin;
        case eHeater_PID_Conf_Max_Output:
            return pConf->omax;
    }
    return 0;
}

/**
 * @brief  PID参数 设置
 * @param  pConf  参数结构体指针
 * @param  offset 参数项别
 * @param  data   数据
 * @retval None
 */
static void heater_PID_Conf_Param_Set(sPID_Ctrl_Conf * pConf, eHeater_PID_Conf offset, float data)
{
    switch (offset) {
        case eHeater_PID_Conf_Kp:
            pConf->Kp = data;
            break;
        case eHeater_PID_Conf_Ki:
            pConf->Ki = data;
            break;
        case eHeater_PID_Conf_Kd:
            pConf->Kd = data;
            break;
        case eHeater_PID_Conf_Set_Point:
            *(pConf->setpoint) = data;
            break;
        case eHeater_PID_Conf_Input:
            *(pConf->input) = data;
            break;
        case eHeater_PID_Conf_Output:
            *(pConf->output) = data;
            break;
        case eHeater_PID_Conf_Min_Output:
            pConf->omin = data;
            break;
        case eHeater_PID_Conf_Max_Output:
            pConf->omax = data;
            break;
    }
}

/**
 * @brief  PID参数 读取 下加热体
 * @param  offset 参数项别
 * @retval 参数数值
 */
float heater_BTM_Conf_Get(eHeater_PID_Conf offset)
{
    return heater_PID_Conf_Param_Get(&gHeater_BTM_PID_Conf, offset);
}

/**
 * @brief  PID参数 设置 下加热体
 * @param  offset 参数项别
 * @param  data   数据
 * @retval None
 */
void heater_BTM_Conf_Set(eHeater_PID_Conf offset, float data)
{
    return heater_PID_Conf_Param_Set(&gHeater_BTM_PID_Conf, offset, data);
}

/**
 * @brief  PID参数 读取 上加热体
 * @param  offset 参数项别
 * @retval 参数数值
 */
float heater_TOP_Conf_Get(eHeater_PID_Conf offset)
{
    return heater_PID_Conf_Param_Get(&gHeater_TOP_PID_Conf, offset);
}

/**
 * @brief  PID参数 设置 上加热体
 * @param  offset 参数项别
 * @param  data   数据
 * @retval None
 */
void heater_TOP_Conf_Set(eHeater_PID_Conf offset, float data)
{
    return heater_PID_Conf_Param_Set(&gHeater_TOP_PID_Conf, offset, data);
}

/**
 * @brief  下加热体 PWM 输出占空比调整
 * @note   范围 0 ~ 100
 * @param  pr 浮点数形式占空比
 * @retval None
 */
void heater_BTM_Output_Ctl(float pr)
{
    uint16_t ccr;

    ccr = ((uint16_t)(pr * HEATER_BTM_ARR)) % (HEATER_BTM_ARR + 1);
    __HAL_TIM_SET_COMPARE(&HEATER_BTM_TIM, HEATER_BTM_CHN, ccr);
}

/**
 * @brief  下加热体 PWM 输出 启动
 * @param  None
 * @retval None
 */

void heater_BTM_Output_Start(void)
{
    HAL_TIM_PWM_Start(&HEATER_BTM_TIM, HEATER_BTM_CHN);
}

/**
 * @brief  下加热体 PWM 输出 停止
 * @param  None
 * @retval None
 */
void heater_BTM_Output_Stop(void)
{
    HAL_TIM_PWM_Stop(&HEATER_BTM_TIM, HEATER_BTM_CHN);
}

/**
 * @brief  下加热体 PWM 输出 状态获取
 * @param  None
 * @retval PWM 输出 状态
 */
uint8_t heater_BTM_Output_Is_Live(void)
{
    return HEATER_BTM_TIM.Instance->CR1 == TIM_CR1_CEN;
}

/**
 * @brief  下加热体 PWM 输出 初始化
 * @param  None
 * @retval None
 */
void heater_BTM_Output_Init(void)
{
    // Prepare PID controller for operation
    pid_ctrl_init(&gHeater_BTM_PID_Conf, HEATER_BTM_SAMPLE, &btm_input, &btm_output, &btm_setpoint, 1200000, 4800, 4800);
    // Set controler output limits from 0 to 200
    pid_ctrl_limits(&gHeater_BTM_PID_Conf, 0, HEATER_BTM_ARR);
    // Allow PID to compute and change output
    pid_ctrl_auto(&gHeater_BTM_PID_Conf);
}

/**
 * @brief  下加热体 PWM 输出 PID 调整
 * @param  None
 * @retval None
 */
void heater_BTM_Output_Keep_Deal(void)
{
    uStorgeParamItem read_data;

    // Check if need to compute PID
    if (pid_ctrl_need_compute(&gHeater_BTM_PID_Conf)) {
        // Read process feedback
        btm_input = temp_Get_Temp_Data_BTM();
        storge_ParamReadSingle(eStorgeParamIndex_Heater_Offset_BTM, read_data.u8s);
        btm_input -= read_data.f32;
        // Compute new PID output value
        pid_ctrl_compute(&gHeater_BTM_PID_Conf);
        // Change actuator value
        heater_BTM_Output_Ctl(btm_output / gHeater_BTM_PID_Conf.omax);
    }
}

void heater_BTM_Log_PID(void)
{
    pid_ctrl_log_d("BTM", &gHeater_BTM_PID_Conf);
}

/**
 * @brief  上加热体 PWM 输出占空比调整
 * @note   范围 0 ~ 100
 * @param  pr 浮点数形式占空比
 * @retval None
 */
void heater_TOP_Output_Ctl(float pr)
{
    uint16_t ccr;

    ccr = ((uint16_t)(pr * HEATER_TOP_ARR)) % (HEATER_TOP_ARR + 1);
    __HAL_TIM_SET_COMPARE(&HEATER_TOP_TIM, HEATER_TOP_CHN, ccr);
}

/**
 * @brief  上加热体 PWM 输出 启动
 * @param  None
 * @retval None
 */

void heater_TOP_Output_Start(void)
{
    HAL_TIM_PWM_Start(&HEATER_TOP_TIM, HEATER_TOP_CHN);
}

/**
 * @brief  上加热体 PWM 输出 停止
 * @param  None
 * @retval None
 */
void heater_TOP_Output_Stop(void)
{
    HAL_TIM_PWM_Stop(&HEATER_TOP_TIM, HEATER_TOP_CHN);
}

/**
 * @brief  上加热体 PWM 输出 状态获取
 * @param  None
 * @retval PWM 输出 状态
 */
HAL_TIM_StateTypeDef heater_TOP_Output_Is_Live(void)
{
    return HEATER_TOP_TIM.Instance->CR1 == TIM_CR1_CEN;
}

/**
 * @brief  上加热体 PWM 输出 初始化
 * @param  None
 * @retval None
 */
void heater_TOP_Output_Init(void)
{
    // Prepare PID controller for operation
    pid_ctrl_init(&gHeater_TOP_PID_Conf, HEATER_TOP_SAMPLE, &top_input, &top_output, &top_setpoint, 600000, 1000, 2000);
    // Set controler output limits from 0 to 200
    pid_ctrl_limits(&gHeater_TOP_PID_Conf, 0, HEATER_TOP_ARR);
    // Allow PID to compute and change output
    pid_ctrl_auto(&gHeater_TOP_PID_Conf);
}

/**
 * @brief  上加热体 PWM 输出 PID 调整
 * @param  None
 * @retval None
 */
void heater_TOP_Output_Keep_Deal(void)
{
    uStorgeParamItem read_data;

    // Check if need to compute PID
    if (pid_ctrl_need_compute(&gHeater_TOP_PID_Conf)) {
        // Read process feedback
        top_input = temp_Get_Temp_Data_TOP();
        storge_ParamReadSingle(eStorgeParamIndex_Heater_Offset_TOP, read_data.u8s);
        top_input -= read_data.f32;
        // Compute new PID output value
        pid_ctrl_compute(&gHeater_TOP_PID_Conf);
        // Change actuator value
        heater_TOP_Output_Ctl(top_output / gHeater_TOP_PID_Conf.omax);
    }
}

void heater_TOP_Log_PID(void)
{
    pid_ctrl_log_d("TOP", &gHeater_TOP_PID_Conf);
}

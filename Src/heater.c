/**
 * @file    heater.c
 * @brief   上下加热体控制控制
 *
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <math.h>
#include "temperature.h"
#include "heater.h"
#include "pid_ctrl.h"
#include "storge_task.h"
#include "protocol.h"

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
static uint8_t gHeater_Outdoor_Flag = 0;

// Control loop input,output and setpoint variables
static float btm_input = 0, btm_output = 0, btm_setpoint = HEATER_BTM_DEFAULT_SETPOINT;
static float top_input = 0, top_output = 0, top_setpoint = HEATER_TOP_DEFAULT_SETPOINT;

static sHeater_Overshoot gHeater_BTM_Overshoot = {0};
static sHeater_Overshoot gHeater_TOP_Overshoot = {0};

/* Private constants ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
static float heater_PID_Conf_Param_Get(sPID_Ctrl_Conf * pConf, eHeater_PID_Conf offset);
static void heater_PID_Conf_Param_Set(sPID_Ctrl_Conf * pConf, eHeater_PID_Conf offset, float data);

/* Private user code ---------------------------------------------------------*/
/**
 * @brief  过冲参数初始化
 * @note   环境温度补偿
 * @param  env 环境温度
 * @retval None
 */
/**
 *
import numpy as np
from scipy.optimize import curve_fit
from loguru import logger

x = np.array([10, 25])
y = np.array([24, 18])

def fit_func(x, a, b):
    return a*x + b

params = curve_fit(fit_func, x, y)

for i in range(10, 36, 5):
    v = fit_func(i, *params[0])
    logger.debug(f"temp {i} duration {v:.2f} S")

 *
*/
void heater_Overshoot_Init(float env)
{
    float fall_duration, delta_temp_factor;
    uint32_t tick;

    if (env == 0) {
        env = temp_Get_Temp_Data_ENV();
        tick = 0xFFFFFFFF;
        gHeater_BTM_Overshoot.start = tick;
        gHeater_TOP_Overshoot.start = tick;
    } else {
        tick = HAL_GetTick();
        gHeater_BTM_Overshoot.start = tick;
        gHeater_TOP_Overshoot.start = tick;
    }

    if (env < 0) { /* For Debug Param */
        return;
    }

    delta_temp_factor = (25 - env) * (25 - env) / (25 - 10) * (25 - 10);

    gHeater_BTM_Overshoot.peak_delta = 0.3 + delta_temp_factor * 0.7;
    gHeater_BTM_Overshoot.level_duration = 18 + delta_temp_factor * 10.0;
    gHeater_BTM_Overshoot.whole_duration = 80;
    fall_duration = gHeater_BTM_Overshoot.whole_duration - gHeater_BTM_Overshoot.level_duration;
    gHeater_BTM_Overshoot.pk = 1.3;
    gHeater_BTM_Overshoot.pb = 1.5 * fall_duration;
    gHeater_BTM_Overshoot.pa =
        gHeater_BTM_Overshoot.peak_delta / log(gHeater_BTM_Overshoot.pb / (gHeater_BTM_Overshoot.pb - gHeater_BTM_Overshoot.pk * fall_duration));
    gHeater_BTM_Overshoot.pc = gHeater_BTM_Overshoot.pa * -1 * log(gHeater_BTM_Overshoot.pb - gHeater_BTM_Overshoot.pk * fall_duration);

    gHeater_TOP_Overshoot.peak_delta = 0.3 + delta_temp_factor * 0.7;
    gHeater_TOP_Overshoot.level_duration = 25 + delta_temp_factor * 10.0;
    gHeater_TOP_Overshoot.whole_duration = 60;
    fall_duration = gHeater_TOP_Overshoot.whole_duration - gHeater_TOP_Overshoot.level_duration;
    gHeater_TOP_Overshoot.pk = 1.3;
    gHeater_TOP_Overshoot.pb = 1.5 * fall_duration;
    gHeater_TOP_Overshoot.pa =
        gHeater_TOP_Overshoot.peak_delta / log(gHeater_TOP_Overshoot.pb / (gHeater_TOP_Overshoot.pb - gHeater_TOP_Overshoot.pk * fall_duration));
    gHeater_TOP_Overshoot.pc = gHeater_TOP_Overshoot.pa * -1 * log(gHeater_TOP_Overshoot.pb - gHeater_TOP_Overshoot.pk * fall_duration);
}

/**
 * @brief  过冲控制参数设置
 * @param  bt_idx  上下加热体索引
 * @param  p_idx 参数项索引
 * @param  float 设置值
 * @retval None
 */
void heater_Overshoot_Set_Parmer(eHeater_Index bt_idx, eHeater_Overshoot_Param_Index p_idx, float data)
{
    float * pParam;

    switch (bt_idx) {
        case eHeater_BTM:
            pParam = &(gHeater_BTM_Overshoot.peak_delta) + p_idx;
            break;
        default:
            pParam = &(gHeater_TOP_Overshoot.peak_delta) + p_idx;
            break;
    }
    *pParam = data;
}

/**
 * @brief  过冲控制参数读取
 * @param  bt_idx  上下加热体索引
 * @param  p_idx 参数项索引
 * @retval 参数值
 */
float heater_Overshoot_Get_Parmer(eHeater_Index bt_idx, eHeater_Overshoot_Param_Index p_idx)
{
    float data;

    switch (bt_idx) {
        case eHeater_BTM:
            data = *(&(gHeater_BTM_Overshoot.peak_delta) + p_idx);
            break;
        default:
            data = *(&(gHeater_TOP_Overshoot.peak_delta) + p_idx);
            break;
    }
    return data;
}

void heater_Overshoot_Set_All(eHeater_Index bt_idx, uint8_t * pBuffer)
{
    eHeater_Overshoot_Param_Index i;
    float data;
    float fall_duration;

    for (i = eHeater_Overshoot_Param_peak_delta; i <= eHeater_Overshoot_Param_whole_duration; ++i) {
        memcpy(&data, pBuffer, 4);
        heater_Overshoot_Set_Parmer(bt_idx, i, data);
        pBuffer += 4;
    }

    fall_duration = gHeater_BTM_Overshoot.whole_duration - gHeater_BTM_Overshoot.level_duration;
    gHeater_BTM_Overshoot.pk = 1.3;
    gHeater_BTM_Overshoot.pb = 1.5 * fall_duration;
    gHeater_BTM_Overshoot.pa =
        gHeater_BTM_Overshoot.peak_delta / log(gHeater_BTM_Overshoot.pb / (gHeater_BTM_Overshoot.pb - gHeater_BTM_Overshoot.pk * fall_duration));
    gHeater_BTM_Overshoot.pc = gHeater_BTM_Overshoot.pa * -1 * log(gHeater_BTM_Overshoot.pb - gHeater_BTM_Overshoot.pk * fall_duration);
    fall_duration = gHeater_TOP_Overshoot.whole_duration - gHeater_TOP_Overshoot.level_duration;
    gHeater_TOP_Overshoot.pk = 1.3;
    gHeater_TOP_Overshoot.pb = 1.5 * fall_duration;
    gHeater_TOP_Overshoot.pa =
        gHeater_TOP_Overshoot.peak_delta / log(gHeater_TOP_Overshoot.pb / (gHeater_TOP_Overshoot.pb - gHeater_TOP_Overshoot.pk * fall_duration));
    gHeater_TOP_Overshoot.pc = gHeater_TOP_Overshoot.pa * -1 * log(gHeater_TOP_Overshoot.pb - gHeater_TOP_Overshoot.pk * fall_duration);
}

void heater_Overshoot_Get_All(eHeater_Index bt_idx, uint8_t * pBuffer)
{
    eHeater_Overshoot_Param_Index i;
    float data;

    for (i = eHeater_Overshoot_Param_peak_delta; i <= eHeater_Overshoot_Param_pc; ++i) {
        data = heater_Overshoot_Get_Parmer(bt_idx, i);
        memcpy(pBuffer, &data, 4);
        pBuffer += 4;
    }
}

/**
 * @brief  过冲控制处理
 * @param  None
 * @retval None
 */
void heater_Overshoot_Handle(void)
{
    float offset_temp, dp;
    uint32_t tick;

    tick = HAL_GetTick();

    /* 下加热体过冲处理 */
    if (heater_Overshoot_Flag_Get(eHeater_BTM)) {                                                      /* 执行下加热体温度过冲 */
        if (tick - gHeater_BTM_Overshoot.start <= gHeater_BTM_Overshoot.level_duration * 1000) {       /* 维持阶段 */
            heater_BTM_Setpoint_Set(gHeater_BTM_Overshoot.peak_delta + HEATER_BTM_DEFAULT_SETPOINT);   /* 修改下加热体目标温度 */
        } else if (tick - gHeater_BTM_Overshoot.start > gHeater_BTM_Overshoot.whole_duration * 1000) { /* 完成过冲 */
            gHeater_BTM_Overshoot.start = 0xFFFFFFFF;                                                  /* 重置起始时间 */
            heater_Overshoot_Flag_Set(eHeater_BTM, 0);                                                 /* 取消过冲标志 */
            heater_BTM_Setpoint_Set(HEATER_BTM_DEFAULT_SETPOINT);                                      /* 修改下加热体目标温度 */
        } else {
            dp = (tick - gHeater_BTM_Overshoot.start) / 1000.0 - gHeater_BTM_Overshoot.level_duration;
            offset_temp = gHeater_BTM_Overshoot.pa * log(-dp * gHeater_BTM_Overshoot.pk + gHeater_BTM_Overshoot.pb) + gHeater_BTM_Overshoot.pc;
            heater_BTM_Setpoint_Set(offset_temp + HEATER_BTM_DEFAULT_SETPOINT); /* 修改下加热体目标温度 */
        }
    } else {
        if (30 < heater_BTM_Setpoint_Get() || heater_BTM_Setpoint_Get() < 50) {    /* 目标温度处于(30, 50)不受调试控制 */
            if (heater_Outdoor_Flag_Get(eHeater_BTM)) {                            /* 出仓温度设置标志 */
                heater_BTM_Setpoint_Set(HEATER_BTM_OUTDOOR_SETPOINT);              /* 出仓状态下调整标志 */
            } else if (heater_BTM_Setpoint_Get() != HEATER_BTM_DEFAULT_SETPOINT) { /* 默认温度 */
                heater_BTM_Setpoint_Set(HEATER_BTM_DEFAULT_SETPOINT);              /* 恢复下加热体目标温度 */
            }
        }
    }

    /* 上加热体过冲处理 */
    if (heater_Overshoot_Flag_Get(eHeater_TOP)) {                                                      /* 执行下加热体温度过冲 */
        if (tick - gHeater_TOP_Overshoot.start <= gHeater_TOP_Overshoot.level_duration * 1000) {       /* 维持阶段 */
            heater_TOP_Setpoint_Set(gHeater_TOP_Overshoot.peak_delta + HEATER_TOP_DEFAULT_SETPOINT);   /* 修改下加热体目标温度 */
        } else if (tick - gHeater_TOP_Overshoot.start > gHeater_TOP_Overshoot.whole_duration * 1000) { /* 完成过冲 */
            gHeater_TOP_Overshoot.start = 0xFFFFFFFF;                                                  /* 重置起始时间 */
            heater_Overshoot_Flag_Set(eHeater_TOP, 0);                                                 /* 取消过冲标志 */
            heater_TOP_Setpoint_Set(HEATER_TOP_DEFAULT_SETPOINT);                                      /* 修改下加热体目标温度 */
        } else {
            dp = (tick - gHeater_TOP_Overshoot.start) / 1000.0 - gHeater_TOP_Overshoot.level_duration;
            offset_temp = gHeater_TOP_Overshoot.pa * log(-dp * gHeater_TOP_Overshoot.pk + gHeater_TOP_Overshoot.pb) + gHeater_TOP_Overshoot.pc;
            heater_TOP_Setpoint_Set(offset_temp + HEATER_TOP_DEFAULT_SETPOINT); /* 修改下加热体目标温度 */
        }
    } else {
        if (30 < heater_TOP_Setpoint_Get() || heater_TOP_Setpoint_Get() < 50) {    /* 目标温度处于(30, 50)不受调试控制 */
            if (heater_Outdoor_Flag_Get(eHeater_TOP)) {                            /* 出仓温度设置标志 */
                heater_TOP_Setpoint_Set(HEATER_TOP_OUTDOOR_SETPOINT);              /* 出仓状态下调整标志 */
            } else if (heater_TOP_Setpoint_Get() != HEATER_TOP_DEFAULT_SETPOINT) { /* 默认温度 */
                heater_TOP_Setpoint_Set(HEATER_TOP_DEFAULT_SETPOINT);              /* 恢复下加热体目标温度 */
            }
        }
    }
}

/**
 * @brief  过冲标志 获取
 * @param  idx 上下索引
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
 * @brief  出仓加热调整标志 获取
 * @retval gHeater_Outdoor_Flag
 */
uint8_t heater_Outdoor_Flag_Get(eHeater_Index idx)
{
    return ((gHeater_Outdoor_Flag & (1 << idx)) > 0) ? (1) : (0);
}

/**
 * @brief  出仓加热调整标志 设置
 * @param  flag 设置值
 * @retval 参数数值
 */
void heater_Outdoor_Flag_Set(eHeater_Index idx, uint8_t flag)
{
    if (flag > 0) {
        gHeater_Outdoor_Flag |= (1 << idx);
    } else {
        gHeater_Outdoor_Flag &= (0xFF - (1 << idx));
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
 * @note   范围 0 ~ 1
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
    return (HEATER_BTM_TIM.Instance->CR1 & TIM_CR1_CEN) == TIM_CR1_CEN;
}

/**
 * @brief  下加热体 PWM 输出 初始化
 * @param  None
 * @retval None
 */
void heater_BTM_Output_Init(void)
{
    // Prepare PID controller for operation
    pid_ctrl_init(&gHeater_BTM_PID_Conf, HEATER_BTM_SAMPLE, &btm_input, &btm_output, &btm_setpoint, 600000, 1200, 1200);
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
    float env_temp;

    // Check if need to compute PID
    if (pid_ctrl_need_compute(&gHeater_BTM_PID_Conf)) {
        // Read process feedback
        btm_input = temp_Get_Temp_Data_BTM();
        if (btm_input > *(gHeater_BTM_PID_Conf.setpoint) + 2) {
            heater_BTM_Output_Ctl(0);
        } else {
            env_temp = temp_Get_Temp_Data_ENV();
            if (env_temp < 24 && env_temp > -7) {
                btm_input = btm_input - (24 - env_temp) * (24 - env_temp) * 0.0005;
            }
            // Compute new PID output value
            pid_ctrl_compute(&gHeater_BTM_PID_Conf);
            // Change actuator value
            heater_BTM_Output_Ctl(btm_output / gHeater_BTM_PID_Conf.omax);
        }
    }
}

void heater_BTM_Log_PID(void)
{
    pid_ctrl_log_d("BTM", &gHeater_BTM_PID_Conf);
}

/**
 * @brief  上加热体 PWM 输出占空比调整
 * @note   范围 0 ~ 1
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
    return (HEATER_TOP_TIM.Instance->CR1 & TIM_CR1_CEN) == TIM_CR1_CEN;
}

/**
 * @brief  上加热体 PWM 输出 初始化
 * @param  None
 * @retval None
 */
void heater_TOP_Output_Init(void)
{
    // Prepare PID controller for operation
    pid_ctrl_init(&gHeater_TOP_PID_Conf, HEATER_TOP_SAMPLE, &top_input, &top_output, &top_setpoint, 180000, 8400, 8000);
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
    float env_temp;

    // Check if need to compute PID
    if (pid_ctrl_need_compute(&gHeater_TOP_PID_Conf)) {
        // Read process feedback
        top_input = temp_Get_Temp_Data_TOP();
        if (top_input > *(gHeater_TOP_PID_Conf.setpoint) + 2) {
            heater_TOP_Output_Ctl(0);
        } else {
            env_temp = temp_Get_Temp_Data_ENV();
            if (env_temp < 24 && env_temp > -7) {
                top_input = top_input - (24 - env_temp) * (24 - env_temp) * 0.0005;
            }
            // Compute new PID output value
            pid_ctrl_compute(&gHeater_TOP_PID_Conf);
            // Change actuator value
            heater_TOP_Output_Ctl(top_output / gHeater_TOP_PID_Conf.omax);
        }
    }
}

void heater_TOP_Log_PID(void)
{
    pid_ctrl_log_d("TOP", &gHeater_TOP_PID_Conf);
}

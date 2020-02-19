/**
 * @file    soft_timer.c
 * @brief   系统软定时器任务
 *
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "heater.h"
#include "pid_ctrl.h"
#include "motor.h"
#include "soft_timer.h"
#include "beep.h"
#include "temperature.h"

/* Extern variables ----------------------------------------------------------*/
extern TIM_HandleTypeDef htim4;

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/
#define SOFT_TIMER_HEATER_PER 10

/* Private variables ---------------------------------------------------------*/
TimerHandle_t gTimerHandleHeater = NULL;

/* Private constants ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/* Private user code ---------------------------------------------------------*/

/**
 * @brief  软定时器回调 加热控制
 * @param  定时器任务句柄
 * @retval None
 */
void soft_timer_Heater_Call_Back(TimerHandle_t xTimer)
{
    static uint16_t btm_cnt = 0, top_cnt = 0;
    static uint8_t btm_flag = 0, top_flag = 0;

    if (heater_Overshoot_Flag_Get(eHeater_BTM)) { /* 执行下加热体温度过冲 */
        if (btm_cnt == 0) {
            btm_cnt = 1;
            heater_BTM_Setpoint_Set(HEATER_BTM_OVERSHOOT_TARGET); /* 修改下加热体目标温度 */
        } else {
            if (btm_flag == 0 && temp_Get_Temp_Data_BTM() >= HEATER_BTM_OVERSHOOT_TARGET) { /* 保持过冲温度计数开始标志位 */
                btm_flag = 1;
            }
            if (btm_flag) {
                if (++btm_cnt > HEATER_BTM_OVERSHOOT_TIMEOUT * (configTICK_RATE_HZ) / (SOFT_TIMER_HEATER_PER)) { /* 保持过冲温度超时计数 */
                    btm_cnt = 0;
                    btm_flag = 0;
                    heater_Overshoot_Flag_Set(eHeater_BTM, 0);
                    heater_BTM_Setpoint_Set(HEATER_BTM_DEFAULT_SETPOINT); /* 恢复下加热体目标温度 */
                } else if (temp_Get_Temp_Data_BTM() < HEATER_BTM_OVERSHOOT_TARGET - 0.08) {
                    heater_BTM_Conf_Set(eHeater_PID_Conf_Min_Output, 23 * HEATER_BTM_ARR / 100); /* 修改下加热体最小输出 */
                }
            }
        }
    } else { /* 温度过冲被停止 */
        btm_cnt = 0;
        btm_flag = 0;
        if (heater_BTM_Setpoint_Get() != HEATER_BTM_DEFAULT_SETPOINT) {
            heater_BTM_Setpoint_Set(HEATER_BTM_DEFAULT_SETPOINT); /* 恢复下加热体目标温度 */
        }
    }

    if (heater_Overshoot_Flag_Get(eHeater_TOP)) { /* 执行上加热体温度过冲 */
        if (top_cnt == 0) {
            top_cnt = 1;
            heater_TOP_Setpoint_Set(HEATER_TOP_OVERSHOOT_TARGET); /* 修改上加热体目标温度 */
        } else {
            if (top_flag == 0 && temp_Get_Temp_Data_TOP() >= HEATER_TOP_OVERSHOOT_TARGET) { /* 保持过冲温度计数开始标志位 */
                top_flag = 1;
            }
            if (top_flag) {
                if (++top_cnt > HEATER_TOP_OVERSHOOT_TIMEOUT * (configTICK_RATE_HZ) / (SOFT_TIMER_HEATER_PER)) { /* 保持过冲温度超时计数 */
                    top_cnt = 0;
                    top_flag = 0;
                    heater_Overshoot_Flag_Set(eHeater_TOP, 0);
                    heater_TOP_Setpoint_Set(HEATER_TOP_DEFAULT_SETPOINT); /* 恢复上加热体目标温度 */
                    heater_TOP_Conf_Set(eHeater_PID_Conf_Min_Output, 0);  /* 恢复上加热体最小输出 */
                    heater_BTM_Conf_Set(eHeater_PID_Conf_Min_Output, 0);  /* 恢复下加热体最小输出 */
                } else if (temp_Get_Temp_Data_TOP() < HEATER_TOP_OVERSHOOT_TARGET - 0.08) {
                    heater_TOP_Conf_Set(eHeater_PID_Conf_Min_Output, 75 * HEATER_TOP_ARR / 100); /* 修改上加热体最小输出 */
                }
            }
        }
    } else { /* 温度过冲被停止 */
        top_cnt = 0;
        top_flag = 0;
        if (heater_TOP_Setpoint_Get() != HEATER_TOP_DEFAULT_SETPOINT) {
            heater_TOP_Setpoint_Set(HEATER_TOP_DEFAULT_SETPOINT); /* 恢复上加热体目标温度 */
        }
    }

    heater_BTM_Output_Keep_Deal();
    heater_TOP_Output_Keep_Deal();
    motor_OPT_Status_Update();
    beep_Deal(SOFT_TIMER_HEATER_PER); /* 蜂鸣器处处理 */
}

/**
 * @brief  软定时器初始化
 * @param  None
 * @retval None
 */
void soft_timer_Heater_Init(void)
{
    gTimerHandleHeater = xTimerCreate("Heater Timer", SOFT_TIMER_HEATER_PER, pdTRUE, (void *)0, soft_timer_Heater_Call_Back);
    if (gTimerHandleHeater == NULL) {
        Error_Handler();
    }

    motor_OPT_Status_Init();
    beep_Init(); /* 蜂鸣器初始化 */
    heater_BTM_Output_Init();
    heater_BTM_Output_Start();

    heater_TOP_Output_Init();
    heater_TOP_Output_Start();

    xTimerStart(gTimerHandleHeater, portMAX_DELAY);
}

/**
 * @brief  软定时器初始化
 * @param  None
 * @retval None
 */
void soft_timer_Init(void)
{
    soft_timer_Heater_Init();
}

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
    static uint16_t cnt = 0;
    static uint8_t flag = 0;

    if (heater_Overshoot_Flag_Get()) { /* 执行温度过冲 */
        if (cnt == 0) {
            cnt = 1;
            heater_BTM_Setpoint_Set(HEATER_OVERSHOOT_TARGET); /* 修改下加热体目标温度 */
            heater_TOP_Setpoint_Set(HEATER_OVERSHOOT_TARGET); /* 修改上加热体目标温度 */
        } else {
            if (flag == 0 && temp_Get_Temp_Data_BTM() >= HEATER_OVERSHOOT_TARGET) { /* 保持过冲温度计数开始标志位 */
                flag = 1;
            }
            if (flag) {
                if (++cnt > HEATER_OVERSHOOT_TIMEOUT * (configTICK_RATE_HZ) / (SOFT_TIMER_HEATER_PER)) { /* 保持过冲温度超时计数 */
                    cnt = 0;
                    flag = 0;
                    heater_Overshoot_Flag_Set(0);
                    heater_BTM_Setpoint_Set(HEATER_BTM_DEFAULT_SETPOINT); /* 恢复下加热体目标温度 */
                    heater_TOP_Setpoint_Set(HEATER_TOP_DEFAULT_SETPOINT); /* 恢复上加热体目标温度 */
                }
            }
        }
    } else { /* 温度过冲被停止 */
        cnt = 0;
        flag = 0;
        if (heater_BTM_Setpoint_Get() != HEATER_BTM_DEFAULT_SETPOINT) {
            heater_BTM_Setpoint_Set(HEATER_BTM_DEFAULT_SETPOINT); /* 恢复下加热体目标温度 */
        }
        if (heater_TOP_Setpoint_Get() != HEATER_TOP_DEFAULT_SETPOINT) {
            heater_TOP_Setpoint_Set(HEATER_BTM_DEFAULT_SETPOINT); /* 恢复上加热体目标温度 */
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

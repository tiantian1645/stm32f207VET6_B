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
#include "i2c_eeprom.h"

/* Extern variables ----------------------------------------------------------*/
extern TIM_HandleTypeDef htim4;

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/
#define SOFT_TIMER_HEATER_PER (10)
#define SOFT_TIMER_HEATER_SECOND_UNIT ((configTICK_RATE_HZ) / (SOFT_TIMER_HEATER_PER))

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
    float btm_temp;

    if (heater_Overshoot_Flag_Get(eHeater_BTM)) { /* 执行下加热体温度过冲 */
        if (btm_cnt == 0) {
            btm_cnt = 1;
            heater_BTM_Setpoint_Set(HEATER_BTM_OVERSHOOT_TARGET); /* 修改下加热体目标温度 */
        } else {
            btm_temp = temp_Get_Temp_Data_BTM();
            if (btm_flag == 0 && btm_temp >= HEATER_BTM_OVERSHOOT_TARGET) { /* 保持过冲温度计数开始标志位 */
                btm_flag = 1;
            }
            if (btm_flag) {
                ++btm_cnt;
                if (btm_cnt < HEATER_BTM_OVERSHOOT_LEVEL_TIMEOUT * SOFT_TIMER_HEATER_SECOND_UNIT) { /* 保持过冲温度超时计数 */
                    if (btm_temp > HEATER_BTM_OVERSHOOT_TARGET) {
                        heater_BTM_Setpoint_Set(btm_temp);
                    }
                } else if (btm_cnt >= HEATER_BTM_OVERSHOOT_TIMEOUT * SOFT_TIMER_HEATER_SECOND_UNIT) { /* 完成过冲 */
                    btm_cnt = 0;
                    btm_flag = 0;
                    heater_Overshoot_Flag_Set(eHeater_BTM, 0);
                    heater_BTM_Setpoint_Set(HEATER_BTM_DEFAULT_SETPOINT); /* 恢复下加热体目标温度 */
                } else {
                    heater_BTM_Setpoint_Set(HEATER_BTM_OVERSHOOT_TARGET -
                                            (HEATER_BTM_OVERSHOOT_TARGET - HEATER_BTM_DEFAULT_SETPOINT) *
                                                ((float)(btm_cnt - HEATER_BTM_OVERSHOOT_LEVEL_TIMEOUT * SOFT_TIMER_HEATER_SECOND_UNIT) /
                                                 ((HEATER_BTM_OVERSHOOT_TIMEOUT - HEATER_BTM_OVERSHOOT_LEVEL_TIMEOUT) * SOFT_TIMER_HEATER_SECOND_UNIT)));
                }
            }
        }
    } else { /* 非温度过冲状态 */
        btm_cnt = 0;
        btm_flag = 0;
        if (heater_BTM_Setpoint_Get() < 50) { /* 目标温度50度以上时视为调试状态 */
            if (heater_Outdoor_Flag_Get(eHeater_BTM)) {
                heater_BTM_Setpoint_Set(HEATER_BTM_OUTDOOR_SETPOINT); /* 出仓状态下调整标志 */
            } else if (heater_BTM_Setpoint_Get() != HEATER_BTM_DEFAULT_SETPOINT) {
                heater_BTM_Setpoint_Set(HEATER_BTM_DEFAULT_SETPOINT); /* 恢复下加热体目标温度 */
            }
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
                ++top_cnt;
                if (top_cnt < HEATER_TOP_OVERSHOOT_LEVEL_TIMEOUT * SOFT_TIMER_HEATER_SECOND_UNIT) { /* 保持过冲温度超时计数 */

                } else if (top_cnt >= HEATER_TOP_OVERSHOOT_TIMEOUT * SOFT_TIMER_HEATER_SECOND_UNIT) { /* 完成过冲 */
                    top_cnt = 0;
                    top_flag = 0;
                    heater_Overshoot_Flag_Set(eHeater_TOP, 0);
                    heater_TOP_Setpoint_Set(HEATER_TOP_DEFAULT_SETPOINT); /* 恢复下加热体目标温度 */
                } else {
                    heater_TOP_Setpoint_Set(HEATER_TOP_OVERSHOOT_TARGET -
                                            (HEATER_TOP_OVERSHOOT_TARGET - HEATER_TOP_DEFAULT_SETPOINT) *
                                                ((float)(top_cnt - HEATER_TOP_OVERSHOOT_LEVEL_TIMEOUT * SOFT_TIMER_HEATER_SECOND_UNIT) /
                                                 ((HEATER_TOP_OVERSHOOT_TIMEOUT - HEATER_TOP_OVERSHOOT_LEVEL_TIMEOUT) * SOFT_TIMER_HEATER_SECOND_UNIT)));
                }
            }
        }
    } else { /* 非温度过冲状态 */
        top_cnt = 0;
        top_flag = 0;
        if (heater_TOP_Setpoint_Get() < 50) { /* 目标温度50度以上时视为调试状态 */
            if (heater_Outdoor_Flag_Get(eHeater_TOP)) {
                heater_TOP_Setpoint_Set(HEATER_TOP_OUTDOOR_SETPOINT); /* 出仓状态下调整标志 */
            } else if (heater_TOP_Setpoint_Get() != HEATER_TOP_DEFAULT_SETPOINT) {
                heater_TOP_Setpoint_Set(HEATER_TOP_DEFAULT_SETPOINT); /* 恢复上加热体目标温度 */
            }
        }
    }

    heater_BTM_Output_Keep_Deal();    /* 下加热体PID控制 */
    heater_TOP_Output_Keep_Deal();    /* 上加热体PID控制 */
    motor_OPT_Status_Update();        /* 电机光耦位置状态更新 */
    I2C_EEPROM_Card_Status_Update();  /* ID Code 卡插入状态更新 */
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

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
    heater_Overshoot_Handle();        /* 过冲控制 */
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

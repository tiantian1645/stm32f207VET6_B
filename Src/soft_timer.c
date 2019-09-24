/**
 * @file    soft_timer.c
 * @brief   系统软定时器任务
 *
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "temperature.h"
#include "heater.h"
#include "pid_ctrl.h"

/* Extern variables ----------------------------------------------------------*/
extern TIM_HandleTypeDef htim4;

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/
#define SOFT_TIMER_PER 10

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
    heater_BTM_Output_Keep_Deal();
    heater_TOP_Output_Keep_Deal();
}

/**
 * @brief  软定时器初始化
 * @param  None
 * @retval None
 */
void soft_timer_Init(void)
{
    gTimerHandleHeater = xTimerCreate("Soft Timer", SOFT_TIMER_PER, pdTRUE, (void *)0, soft_timer_Heater_Call_Back);
    if (gTimerHandleHeater == NULL) {
        Error_Handler();
    }

    heater_BTM_Output_Init();
    beater_BTM_Output_Start();

    heater_TOP_Output_Init();
    beater_TOP_Output_Start();

    xTimerStart(gTimerHandleHeater, 0);
}

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
#include "protocol.h"
#include "comm_out.h"
#include "comm_main.h"
#include "comm_data.h"

/* Extern variables ----------------------------------------------------------*/
extern TIM_HandleTypeDef htim4;

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/
#define SOFT_TIMER_HEATER_PER 10
#define SOFT_TIMER_TEMP_PER 5000

/* Private variables ---------------------------------------------------------*/
TimerHandle_t gTimerHandleHeater = NULL;
TimerHandle_t gTimerHandleTemp = NULL;

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
void soft_timer_Heater_Init(void)
{
    gTimerHandleHeater = xTimerCreate("Heater Timer", SOFT_TIMER_HEATER_PER, pdTRUE, (void *)0, soft_timer_Heater_Call_Back);
    if (gTimerHandleHeater == NULL) {
        Error_Handler();
    }

    heater_BTM_Output_Init();
    beater_BTM_Output_Start();

    heater_TOP_Output_Init();
    beater_TOP_Output_Start();

    xTimerStart(gTimerHandleHeater, portMAX_DELAY);
}

/**
 * @brief  软定时器 温度主动上送任务 暂停
 * @param  定时器任务句柄
 * @retval None
 */
void soft_timer_Temp_Pause(void)
{
    xTimerStop(gTimerHandleTemp, portMAX_DELAY);
}

/**
 * @brief  软定时器 温度主动上送任务 回复
 * @param  定时器任务句柄
 * @retval None
 */
void soft_timer_Temp_Resume(void)
{
    xTimerReset(gTimerHandleTemp, portMAX_DELAY);
}

/**
 * @brief  软定时器回调 温度主动上送
 * @param  定时器任务句柄
 * @retval None
 */
void soft_timer_Temp_Call_Back(TimerHandle_t xTimer)
{
    uint8_t buffer[10], length;
    uint16_t temp;

    if (comm_Data_Sample_Complete_Check() == 0) { /* 采样完成信号量被占用 采样中停止输出 */
        return;
    }

    temp = (uint16_t)(temp_Get_Temp_Data_BTM() * 100);
    buffer[0] = temp & 0xFF; /* 小端模式 */
    buffer[1] = temp >> 8;

    temp = (uint16_t)(temp_Get_Temp_Data_TOP() * 100);
    buffer[2] = temp & 0xFF; /* 小端模式 */
    buffer[3] = temp >> 8;

    length = buildPackOrigin(eComm_Main, eProtocoleRespPack_Client_TMP, buffer, 4);
    comm_Out_SendTask_QueueEmitCover(buffer, length);
}

/**
 * @brief  软定时器初始化
 * @param  None
 * @retval None
 */
void soft_timer_Temp_Init(void)
{
    gTimerHandleTemp = xTimerCreate("Temp Timer", SOFT_TIMER_TEMP_PER, pdTRUE, (void *)0, soft_timer_Temp_Call_Back);
    if (gTimerHandleTemp == NULL) {
        Error_Handler();
    }

    xTimerStart(gTimerHandleTemp, portMAX_DELAY);
}

/**
 * @brief  软定时器初始化
 * @param  None
 * @retval None
 */
void soft_timer_Init(void)
{
    soft_timer_Heater_Init();
    soft_timer_Temp_Init();
}

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
#include "beep.h"

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
static uint8_t gSoft_Timer_Comm_Ctl = 0;

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
    temp_Filter_Deal();
    heater_BTM_Output_Keep_Deal();
    heater_TOP_Output_Keep_Deal();
    beep_Deal(SOFT_TIMER_HEATER_PER);
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
 * @brief  软定时器 温度主动上送任务 查询存货状态
 * @param  定时器任务句柄
 * @retval xTimerIsTimerActive
 */
BaseType_t soft_timer_Temp_IsActive(void)
{
    return xTimerIsTimerActive(gTimerHandleTemp);
}

/**
 * @brief  软定时器 温度主动上送任务 串口发送许可
 * @param  comm_index 串口索引
 * @param  sw 操作  1 使能  0 失能
 * @retval None
 */
void soft_timer_Temp_Comm_Set(eProtocol_COMM_Index comm_index, uint8_t sw)
{
    if (sw > 0) {
        gSoft_Timer_Comm_Ctl |= (1 << comm_index);
    } else {
        gSoft_Timer_Comm_Ctl &= 0xFF - (1 << comm_index);
    }
}

/**
 * @brief  软定时器 温度主动上送任务 串口发送许可
 * @param  comm_index 串口索引
 * @retval 1 使能  0 失能
 */
uint8_t soft_timer_Temp_Comm_Get(eProtocol_COMM_Index comm_index)
{
    if (gSoft_Timer_Comm_Ctl & (1 << comm_index)) {
        return 1;
    } else {
        return 0;
    }
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

    if (comm_Out_SendTask_Queue_GetWaiting() > 2) { /* 队列内未处理数据多于2 */
        return;
    }
    if (soft_timer_Temp_Comm_Get(eComm_Out) == 0 && soft_timer_Temp_Comm_Get(eComm_Main) == 0) { /* 无需进行串口发送 */
        return;
    }

    temp = (uint16_t)(temp_Get_Temp_Data_BTM() * 100);
    buffer[0] = temp & 0xFF; /* 小端模式 */
    buffer[1] = temp >> 8;

    temp = (uint16_t)(temp_Get_Temp_Data_TOP() * 100);
    buffer[2] = temp & 0xFF; /* 小端模式 */
    buffer[3] = temp >> 8;

    if (soft_timer_Temp_Comm_Get(eComm_Out)) {
        length = buildPackOrigin(eComm_Out, eProtocoleRespPack_Client_TMP, buffer, 4);
        comm_Out_SendTask_QueueEmitCover(buffer, length);
    }
    // if (soft_timer_Temp_Comm_Get(eComm_Main)) {
    //     length = buildPackOrigin(eComm_Main, eProtocoleRespPack_Client_TMP, buffer, 4);
    //     comm_Main_SendTask_QueueEmitCover(buffer, length);
    // }
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
    soft_timer_Temp_Comm_Set(eComm_Out, 0);       /* 关闭外串口发送 */
    soft_timer_Temp_Comm_Set(eComm_Main, 0);      /* 关闭主板发送 */
    xTimerStart(gTimerHandleTemp, portMAX_DELAY); /* 启动任务 */
}

/**
 * @brief  软定时器初始化
 * @param  None
 * @retval None
 */
void soft_timer_Init(void)
{
    beep_Init();
    soft_timer_Heater_Init();
    soft_timer_Temp_Init();
}

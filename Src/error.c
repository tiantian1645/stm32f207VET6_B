/**
 * @file    error.c
 * @brief   装置故障信息处理
 *
 *
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "comm_out.h"
#include "comm_main.h"

/* Extern variables ----------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Private constants ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/* Private user code ---------------------------------------------------------*/

/**
 * @brief  发送故障信息到串口任务
 * @param  pp 故障外设
 * @param  detail 具体故障内容
 * @retval None
 */
void error_Emit(eError_Peripheral pp, uint8_t detail)
{
    sError_Info errorInfo;

    errorInfo.peripheral = pp;
    errorInfo.type = detail;
    comm_Main_SendTask_ErrorInfoQueueEmit(&errorInfo, 0); /* 发送给主板串口 */
    comm_Out_SendTask_ErrorInfoQueueEmit(&errorInfo, 0);  /* 发送给外串口 */
}

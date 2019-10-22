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
    uint8_t buff[9];

    buff[0] = pp;
    buff[1] = detail;
    comm_Main_SendTask_QueueEmitWithBuildCover(eProtocoleRespPack_Client_ERR, buff, 2); /* 发送给主板串口 */
    comm_Out_SendTask_QueueEmitWithModify(buff, 9, 0);                                  /* 转发给外串口 但不能阻塞主板串口 */
}

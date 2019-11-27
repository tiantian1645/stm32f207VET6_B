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
void error_Emit(eError_Code code)
{
    uint16_t errorCode;
    uint8_t out_mark = 0b11;

    switch (code) {
        case eError_Comm_Main_Busy: /* 主串口发送忙 */
        /* 外串口故障不能发送到主串口 */
        case eError_Comm_Out_Busy:        /* 外串口发送忙 */
        case eError_Comm_Out_Send_Failed: /* 外串口发送失败 */
        case eError_Comm_Out_Not_ACK:     /* 外串口没有收到ACK */
        case eError_Comm_Out_Wrong_ID:    /* 外串口异常ID */
        case eError_Comm_Out_Unknow_CMD:  /* 外串口异常功能码 */
        case eError_Comm_Out_Param_Error: /* 外串口报文参数异常 */
            out_mark = 0b01;
            break;
        /* 错误调试不发送到主串口 */
        case eError_Motor_Heater_Debug: /* 上加热体电机错误调试 */
        case eError_Motor_White_Debug:  /* 白板电机错误调试 */
        case eError_Motor_Tray_Debug:   /* 托盘电机错误调试 */
        case eError_Motor_Scan_Debug:   /* 扫码电机错误调试 */
        case eError_Scan_Debug:         /* 扫码枪错误调试 */
            out_mark = 0b00;
            break;
        default:
            break;
    }

    if (out_mark == 0) {
        return; /* 直接返回 */
    }

    errorCode = code; /*  uint32_t --> uint16_t */

    if (out_mark & 0b10) {                                    /* 主串口标志 */
        comm_Main_SendTask_ErrorInfoQueueEmit(&errorCode, 0); /* 发送给主板串口 */
    }
    if (protocol_Debug_ErrorReport() && (out_mark & 0b01)) { /* 外串口标志 */
        comm_Out_SendTask_ErrorInfoQueueEmit(&errorCode, 0); /* 发送给外串口 */
    }
}

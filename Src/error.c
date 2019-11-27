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

    switch (code) {
        case eError_Motor_Task_Busy:                     /* 电机任务忙 */
        case eError_Motor_Heater_Debug:                  /* 上加热体电机错误调试 */
        case eError_Motor_Heater_Timeout_Up:             /* 上加热体电机运动超时 上升方向 */
        case eError_Motor_Heater_Timeout_Down:           /* 上加热体电机运动超时 下降方向 */
        case eError_Motor_Heater_Status_Warui:           /* 上加热体电机驱动状态异常 */
        case eError_Motor_White_Debug:                   /* 白板电机错误调试 */
        case eError_Motor_White_Timeout_PD:              /* 白板电机运动超时 PD方向 */
        case eError_Motor_White_Timeout_WH:              /* 白板电机运动超时 白物质方向 */
        case eError_Motor_White_Status_Warui:            /* 白板电机驱动状态异常 */
        case eError_Motor_Tray_Debug:                    /* 托盘电机错误调试 */
        case eError_Motor_Tray_Busy:                     /* 托盘电机驱动忙 */
        case eError_Motor_Tray_Timeout:                  /* 托盘电机运动超时 */
        case eError_Motor_Tray_Status_Warui:             /* 托盘电机驱动状态异常 */
        case eError_Motor_Scan_Debug:                    /* 扫码电机错误调试 */
        case eError_Motor_Scan_Busy:                     /* 扫码电机驱动忙 */
        case eError_Motor_Scan_Timeout:                  /* 扫码电机运动超时 */
        case eError_Motor_Scan_Status_Warui:             /* 扫码电机驱动状态异常 */
        case eError_Temperature_Top_Abnormal:            /* 上加热体温度异常 */
        case eError_Temperature_Top_TooHigh:             /* 上加热体温度过高 */
        case eError_Temperature_Top_TooLow:              /* 上加热体温度过低 */
        case eError_Temperature_Btm_Abnormal:            /* 下加热体温度异常 */
        case eError_Temperature_Btm_TooHigh:             /* 下加热体温度过高 */
        case eError_Temperature_Btm_TooLow:              /* 下加热体温度过低 */
        case eError_Scan_Debug:                          /* 扫码枪错误调试 */
        case eError_Scan_Connect_Timeout:                /* 扫码枪通讯超时 */
        case eError_Scan_Config_Failed:                  /* 扫码枪配置失败 */
        case eError_Storge_Task_Busy:                    /* 存储任务忙 */
        case eError_ID_Card_Deal_Param:                  /* ID Code卡操作参数异常 */
        case eError_ID_Card_Read_Failed:                 /* ID Code卡读取失败 */
        case eError_ID_Card_Write_Failed:                /* ID Code卡写入失败 */
        case eError_ID_Card_Not_Insert:                  /* ID Code卡未插卡 */
        case eError_Out_Flash_Deal_Param:                /* 外部Flash操作参数异常 */
        case eError_Out_Flash_Read_Failed:               /* 外部Flash读取失败 */
        case eError_Out_Flash_Write_Failed:              /* 外部Flash写入失败 */
        case eError_Out_Flash_Unknow:                    /* 外部Flash型号无法识别 */
        case eError_Out_Flash_Storge_Param_Out_Of_Range: /* 外部Flash存储参数越限 */
        case eError_Comm_Main_Busy:                      /* 主串口发送忙 */
        case eError_Comm_Main_Send_Failed:               /* 主串口发送失败 */
        case eError_Comm_Main_Not_ACK:                   /* 主串口没有收到ACK */
        case eError_Comm_Main_Wrong_ID:                  /* 主串口异常ID */
        case eError_Comm_Main_Unknow_CMD:                /* 主串口异常功能码 */
        case eError_Comm_Main_Param_Error:               /* 主串口报文参数异常 */
        case eError_Comm_Data_Busy:                      /* 采样板查串口发送忙 */
        case eError_Comm_Data_Send_Failed:               /* 采样板查串口发送失败 */
        case eError_Comm_Data_Not_ACK:                   /* 采样板查串口没有收到ACK */
        case eError_Comm_Data_Wrong_ID:                  /* 采样板查串口异常ID */
        case eError_Comm_Data_Unknow_CMD:                /* 采样板查串口异常功能码 */
        case eError_Comm_Data_Param_Error:               /* 采样板查串口报文参数异常 */
        case eError_Comm_Out_Busy:                       /* 外串口发送忙 */
        case eError_Comm_Out_Send_Failed:                /* 外串口发送失败 */
        case eError_Comm_Out_Not_ACK:                    /* 外串口没有收到ACK */
        case eError_Comm_Out_Wrong_ID:                   /* 外串口异常ID */
        case eError_Comm_Out_Unknow_CMD:                 /* 外串口异常功能码 */
        case eError_Comm_Out_Param_Error:                /* 外串口报文参数异常 */
            errorCode = code;
            break;
        default:
            return;
    }

    if (out_mark & 0b10) {                                    /* 主串口标志 */
        comm_Main_SendTask_ErrorInfoQueueEmit(&errorCode, 0); /* 发送给主板串口 */
    }
    if (protocol_Debug_ErrorReport() && (out_mark & 0b01)) { /* 外串口标志 */
        comm_Out_SendTask_ErrorInfoQueueEmit(&errorCode, 0); /* 发送给外串口 */
    }
}

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __ERROR_H
#define __ERROR_H

/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/
#include "protocol.h"

/* Exported macro ------------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/
typedef enum {
    eError_Motor_Task_Busy, /* 电机任务忙 */

    eError_Motor_Heater_Debug,        /* 上加热体电机错误调试 */
    eError_Motor_Heater_Timeout_Up,   /* 上加热体电机运动超时 上升方向 */
    eError_Motor_Heater_Timeout_Down, /* 上加热体电机运动超时 下降方向 */
    eError_Motor_Heater_Status_Warui, /* 上加热体电机驱动状态异常 */

    eError_Motor_White_Debug,        /* 白板电机错误调试 */
    eError_Motor_White_Timeout_PD,   /* 白板电机运动超时 PD方向 */
    eError_Motor_White_Timeout_WH,   /* 白板电机运动超时 白物质方向 */
    eError_Motor_White_Status_Warui, /* 白板电机驱动状态异常 */

    eError_Motor_Tray_Debug,        /* 托盘电机错误调试 */
    eError_Motor_Tray_Busy,         /* 托盘电机驱动忙 */
    eError_Motor_Tray_Timeout,      /* 托盘电机运动超时 */
    eError_Motor_Tray_Status_Warui, /* 托盘电机驱动状态异常 */

    eError_Motor_Scan_Debug,        /* 扫码电机错误调试 */
    eError_Motor_Scan_Busy,         /* 扫码电机驱动忙 */
    eError_Motor_Scan_Timeout,      /* 扫码电机运动超时 */
    eError_Motor_Scan_Status_Warui, /* 扫码电机驱动状态异常 */

    eError_Temperature_Top_Abnormal, /* 上加热体温度异常 */
    eError_Temperature_Top_TooHigh,  /* 上加热体温度过高 */
    eError_Temperature_Top_TooLow,   /* 上加热体温度过低 */
    eError_Temperature_Btm_Abnormal, /* 下加热体温度异常 */
    eError_Temperature_Btm_TooHigh,  /* 下加热体温度过高 */
    eError_Temperature_Btm_TooLow,   /* 下加热体温度过低 */

    eError_Scan_Debug,           /* 扫码枪错误调试 */
    eError_Scan_Connect_Timeout, /* 扫码枪通讯超时 */
    eError_Scan_Config_Failed,   /* 扫码枪配置失败 */

    eError_Storge_Task_Busy, /* 存储任务忙 */

    eError_ID_Card_Deal_Param,   /* ID Code卡操作参数异常 */
    eError_ID_Card_Read_Failed,  /* ID Code卡读取失败 */
    eError_ID_Card_Write_Failed, /* ID Code卡写入失败 */
    eError_ID_Card_Not_Insert,   /* ID Code卡未插卡 */

    eError_Out_Flash_Deal_Param,                /* 外部Flash操作参数异常 */
    eError_Out_Flash_Read_Failed,               /* 外部Flash读取失败 */
    eError_Out_Flash_Write_Failed,              /* 外部Flash写入失败 */
    eError_Out_Flash_Unknow,                    /* 外部Flash型号无法识别 */
    eError_Out_Flash_Storge_Param_Out_Of_Range, /* 外部Flash存储参数越限 */

    eError_Comm_Main_Busy,        /* 串口发送忙 */
    eError_Comm_Main_Send_Failed, /* 发送失败 */
    eError_Comm_Main_Not_ACK,     /* 没有收到ACK */
    eError_Comm_Main_Wrong_ID,    /* 异常ID */
    eError_Comm_Main_Unknow_CMD,  /* 异常功能码 */
    eError_Comm_Main_Param_Error, /* 报文参数异常 */

    eError_Comm_Data_Busy,        /* 串口发送忙 */
    eError_Comm_Data_Send_Failed, /* 发送失败 */
    eError_Comm_Data_Not_ACK,     /* 没有收到ACK */
    eError_Comm_Data_Wrong_ID,    /* 异常ID */
    eError_Comm_Data_Unknow_CMD,  /* 异常功能码 */
    eError_Comm_Data_Param_Error, /* 报文参数异常 */

    eError_Comm_Out_Busy,        /* 串口发送忙 */
    eError_Comm_Out_Send_Failed, /* 发送失败 */
    eError_Comm_Out_Not_ACK,     /* 没有收到ACK */
    eError_Comm_Out_Wrong_ID,    /* 异常ID */
    eError_Comm_Out_Unknow_CMD,  /* 异常功能码 */
    eError_Comm_Out_Param_Error, /* 报文参数异常 */
} eError_Code;

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void error_Emit(eError_Code code);
void error_Handle(TickType_t xTick);

/* Private defines -----------------------------------------------------------*/

#endif

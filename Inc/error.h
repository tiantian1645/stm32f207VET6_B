/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __ERROR_H
#define __ERROR_H

/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/
#include "protocol.h"

/* Exported macro ------------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/
typedef enum {
    /* 调试类 */
    eError_Motor_Heater_Debug = 1, /* 上加热体电机错误调试 */
    eError_Motor_White_Debug = 2,  /* 白板电机错误调试 */
    eError_Motor_Tray_Debug = 3,   /* 托盘电机错误调试 */
    eError_Motor_Scan_Debug = 4,   /* 扫码电机错误调试 */
    eError_Scan_Debug = 5,         /* 扫码枪错误调试 */
    eError_Comm_Out_Resend_1 = 6,  /* 采样板串口第一次发送无回应 */
    eError_Comm_Out_Resend_2 = 7,  /* 采样板串口第二次发送无回应 */
    eError_Comm_Out_Lost_0 = 8,    /* 采样板串口采样过程中发送失败 */
    eError_Comm_Out_Lost_1 = 9,    /* 采样板串口采样过程中第一次发送无回应 */
    eError_Comm_Out_Lost_2 = 10,   /* 采样板串口采样过程中第二次发送无回应 */
    eError_Comm_Out_Lost_3 = 11,   /* 采样板串口采样过程中第三次发送无回应 */

    /* 提示类信息 */
    eError_Motor_Task_Busy = 100,       /* 电机任务忙 */
    eError_Motor_Notify_No_Read = 101,  /* 电机任务通知不能读 */
    eError_Storge_Task_Busy = 102,      /* 存储任务忙 */
    eError_ID_Card_Deal_Param = 103,    /* ID Code卡操作参数异常 */
    eError_Out_Flash_Deal_Param = 104,  /* 外部Flash操作参数异常 */
    eError_Comm_Main_Wrong_ID = 105,    /* 主串口异常ID */
    eError_Comm_Main_Unknow_CMD = 106,  /* 主串口异常功能码 */
    eError_Comm_Main_Param_Error = 107, /* 主串口报文参数异常 */
    eError_Comm_Data_Wrong_ID = 108,    /* 采样板串口异常ID */
    eError_Comm_Data_Unknow_CMD = 109,  /* 采样板串口异常功能码 */
    eError_Comm_Data_Param_Error = 110, /* 采样板串口报文参数异常 */
    eError_Comm_Out_Wrong_ID = 111,     /* 外串口异常ID */
    eError_Comm_Out_Unknow_CMD = 112,   /* 外串口异常功能码 */
    eError_Comm_Out_Param_Error = 113,  /* 外串口报文参数异常 */

    /* 执行异常 */
    eError_Motor_Heater_Timeout_Up = 200,             /* 上加热体电机运动超时 上升方向 */
    eError_Motor_Heater_Timeout_Down = 201,           /* 上加热体电机运动超时 下降方向 */
    eError_Motor_Heater_Status_Warui = 202,           /* 上加热体电机驱动状态异常 */
    eError_Motor_White_Timeout_PD = 203,              /* 白板电机运动超时 PD方向 */
    eError_Motor_White_Timeout_WH = 204,              /* 白板电机运动超时 白物质方向 */
    eError_Motor_White_Status_Warui = 205,            /* 白板电机驱动状态异常 */
    eError_Motor_Tray_Busy = 206,                     /* 托盘电机驱动忙 */
    eError_Motor_Tray_Timeout = 207,                  /* 托盘电机运动超时 */
    eError_Motor_Tray_Status_Warui = 208,             /* 托盘电机驱动状态异常 */
    eError_Motor_Scan_Busy = 209,                     /* 扫码电机驱动忙 */
    eError_Motor_Scan_Timeout = 210,                  /* 扫码电机运动超时 */
    eError_Motor_Scan_Status_Warui = 211,             /* 扫码电机驱动状态异常 */
    eError_Scan_Connect_Timeout = 218,                /* 扫码枪通讯超时 */
    eError_Scan_Config_Failed = 219,                  /* 扫码枪配置失败 */
    eError_ID_Card_Read_Failed = 220,                 /* ID Code卡读取失败 */
    eError_ID_Card_Write_Failed = 221,                /* ID Code卡写入失败 */
    eError_Out_Flash_Read_Failed = 222,               /* 外部Flash读取失败 */
    eError_Out_Flash_Write_Failed = 223,              /* 外部Flash写入失败 */
    eError_Out_Flash_Storge_Param_Out_Of_Range = 224, /* 外部Flash存储参数越限 */
    eError_Comm_Main_Busy = 225,                      /* 主串口发送忙 */
    eError_Comm_Main_Send_Failed = 226,               /* 主串口发送失败 */
    eError_Comm_Main_Not_ACK = 227,                   /* 主串口没有收到ACK */
    eError_Comm_Data_Busy = 228,                      /* 采样板串口发送忙 */
    eError_Comm_Data_Send_Failed = 229,               /* 采样板串口发送失败 */
    eError_Comm_Data_Not_ACK = 230,                   /* 采样板串口没有收到ACK */
    eError_Comm_Out_Busy = 231,                       /* 外串口发送忙 */
    eError_Comm_Out_Send_Failed = 232,                /* 外串口发送失败 */
    eError_Comm_Out_Not_ACK = 233,                    /* 外串口没有收到ACK */
    eError_Sample_Incomlete = 234,                    /* 采样未完成 */
    eError_Stary_Incomlete = 235,                     /* 杂散光测试未完成 */

    /* 周期性上送异常 */
    eError_Temperature_Top_Abnormal = 300, /* 上加热体温度异常 */
    eError_Temperature_Top_TooHigh = 301,  /* 上加热体温度过高 */
    eError_Temperature_Top_TooLow = 302,   /* 上加热体温度过低 */
    eError_Temperature_Btm_Abnormal = 303, /* 下加热体温度异常 */
    eError_Temperature_Btm_TooHigh = 304,  /* 下加热体温度过高 */
    eError_Temperature_Btm_TooLow = 305,   /* 下加热体温度过低 */

    /* 硬件故障 不可修复 */
    eError_ID_Card_Not_Insert = 400, /* ID Code卡未插卡 */
    eError_Out_Flash_Unknow = 401,   /* 外部Flash型号无法识别 */
} eError_Code;

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void error_Emit(eError_Code code);
void error_Emit_FromISR(eError_Code code);

/* Private defines -----------------------------------------------------------*/

#endif

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __ERROR_H
#define __ERROR_H

/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/
#include "protocol.h"

/* Exported macro ------------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/
typedef enum {
    eError_Peripheral_Motor_Heater, /* 上加热体电机 */
    eError_Peripheral_Motor_White,  /* 白板电机 */
    eError_Peripheral_Motor_Tray,   /* 托盘电机 */
    eError_Peripheral_Motor_Scan,   /* 扫码电机 */

    eError_Peripheral_Temp_T1,  /* 上加热体温度探头1 */
    eError_Peripheral_Temp_T2,  /* 上加热体温度探头2 */
    eError_Peripheral_Temp_T3,  /* 上加热体温度探头3 */
    eError_Peripheral_Temp_T4,  /* 上加热体温度探头4 */
    eError_Peripheral_Temp_T5,  /* 上加热体温度探头5 */
    eError_Peripheral_Temp_T6,  /* 上加热体温度探头6 */
    eError_Peripheral_Temp_B1,  /* 下加热体温度探头1 */
    eError_Peripheral_Temp_B2,  /* 下加热体温度探头2 */
    eError_Peripheral_Temp_Env, /* 环境温度探头 */

    eError_Peripheral_Heater_Top, /* 上加热体温控 */
    eError_Peripheral_Heater_Btm, /* 下加热体温控 */

    eError_Peripheral_Storge_Flash,   /* W25Q64 Flash存储芯片 */
    eError_Peripheral_Storge_ID_Card, /* ID Code 卡 */

    eError_Peripheral_COMM_Out,  /* 对外串口通信 */
    eError_Peripheral_COMM_Main, /* 主板通信 */
    eError_Peripheral_COMM_Data, /* 采集板通信 */

    eError_Peripheral_Scanner, /* 扫码枪 */
    eError_Peripheral_Fan,     /* 风扇 */

} eError_Peripheral;

typedef enum {
    eError_Motor_Busy,         /* 资源不可用 */
    eError_Motor_Timeout,      /* 电机运动超时 */
    eError_Motor_Status_Warui, /* 电机驱动异常 */
} eError_Motor;

typedef enum {
    eError_Temp_Nai,       /* ADC采样值无值 */
    eError_Temp_Too_Low,   /* 温度超过理论下限 */
    eError_Temp_Too_Hight, /* 温度超过理论上限 */
} eError_Temp;

typedef enum {
    eError_Heater_Block_Low, /* 升温失效 */
    eError_Heater_Over_Temp, /* 温度过高 */
} eError_Heater;

typedef enum {
    eError_Storge_Busy,        /* 资源不可用 */
    eError_Storge_Read_Error,  /* 读取失败 */
    eError_Storge_Write_Error, /* 写入失败 */
} eError_Storge;

typedef enum {
    eError_COMM_Busy,        /* 资源不可用 */
    eError_COMM_Send_Failed, /* 发送失败 */
    eError_COMM_Wrong_ACK,   /* 回应帧号不正确 */
    eError_COMM_Recv_None,   /* 无回应 */
} eError_COMM;

typedef enum {
    eError_Scanner_Recv_None, /* 通讯无接收 */
} eError_Scanner;

typedef enum {
    eError_Fan_Speed_None, /* 转速为零 */
    eError_Fan_Speed_Lost, /* 失速 */
} eError_Fan;

typedef struct {
    uint8_t peripheral; /* 外设类型 */
    uint8_t type;       /* 错误细节 */
} sError_Info;

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void error_Emit(eError_Peripheral pp, uint8_t detail);

/* Private defines -----------------------------------------------------------*/

#endif

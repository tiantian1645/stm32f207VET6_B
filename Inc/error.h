/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __ERROR_H
#define __ERROR_H

/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/
#include "protocol.h"

/* Exported macro ------------------------------------------------------------*/
#define ERROR_TYPE_DEBUG 0xFF

/* Exported types ------------------------------------------------------------*/
typedef enum {
    eError_Peripheral_Motor_Heater, /* 上加热体电机 */
    eError_Peripheral_Motor_White,  /* 白板电机 */
    eError_Peripheral_Motor_Tray,   /* 托盘电机 */
    eError_Peripheral_Motor_Scan,   /* 扫码电机 */

    eError_Peripheral_Temp_Top, /* 上加热体温度 */
    eError_Peripheral_Temp_Btm, /* 下加热体温度 */
    eError_Peripheral_Temp_Env, /* 环境温度 */

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
    eError_Motor_Busy = 0x01,         /* 资源不可用 */
    eError_Motor_Timeout = 0x02,      /* 电机运动超时 */
    eError_Motor_Status_Warui = 0x04, /* 电机驱动异常 */
} eError_Motor;

typedef enum {
    eError_Temp_Nai = 0x01,       /* 温度值无效 */
    eError_Temp_Too_Low = 0x02,   /* 温度持续过低 */
    eError_Temp_Too_Hight = 0x04, /* 温度持续过高 */
} eError_Temp;

typedef enum {
    eError_Storge_Busy = 0x01,        /* 资源不可用 */
    eError_Storge_Read_Error = 0x02,  /* 读取失败 */
    eError_Storge_Write_Error = 0x04, /* 写入失败 */
} eError_Storge;

typedef enum {
    eError_COMM_Busy = 0x01,        /* 资源不可用 */
    eError_COMM_Send_Failed = 0x02, /* 发送失败 */
    eError_COMM_Wrong_ACK = 0x04,   /* 回应帧号不正确 */
    eError_COMM_Recv_None = 0x08,   /* 无回应 */
} eError_COMM;

typedef enum {
    eError_Scanner_Conf_Failed = 0x01, /* 配置失败 */
} eError_Scanner;

typedef enum {
    eError_Fan_Speed_None = 0x01, /* 转速为零 */
    eError_Fan_Speed_Lost = 0x02, /* 失速 */
} eError_Fan;

typedef struct {
    uint8_t peripheral; /* 外设类型 */
    uint8_t type;       /* 错误细节 */
} sError_Info;

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void error_Emit(eError_Peripheral pp, uint8_t detail);
void error_Clear(eError_Peripheral pp);
void error_Handle(TickType_t xTick);

/* Private defines -----------------------------------------------------------*/

#endif

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include "serial.h"

/* Exported constants --------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/
typedef enum {
    PROTOCOL_DEVICE_ID_MAIN = 0x41,
    PROTOCOL_DEVICE_ID_CTRL = 0x45,
    PROTOCOL_DEVICE_ID_SAMP = 0x46,
    PROTOCOL_DEVICE_ID_TEST = 0x13,
} eProtocolDeviceID;

typedef enum {
    eProtocolEmitPack_Client_CMD_START = 0x01,   /* 开始测量帧 */
    eProtocolEmitPack_Client_CMD_ABRUPT = 0x02,  /* 仪器测量取消命令帧 */
    eProtocolEmitPack_Client_CMD_CONFIG = 0x03,  /* 测试项信息帧 */
    eProtocolEmitPack_Client_CMD_FORWARD = 0x04, /* 打开托盘帧 */
    eProtocolEmitPack_Client_CMD_REVERSE = 0x05, /* 关闭托盘命令帧 */
    eProtocolEmitPack_Client_CMD_READ_ID = 0x06, /* ID卡读取命令帧 */
    eProtocolEmitPack_Client_CMD_STATUS = 0x07,  /* 状态信息查询帧 (首帧) */
    eProtocolEmitPack_Client_CMD_TEST = 0x08,    /* 工装测试配置帧 */

    eProtocolEmitPack_Client_CMD_UPGRADE = 0x0F, /* 下位机升级命令帧 */

    eProtocolEmitPack_Client_CMD_SP_LED_GET = 0x32, /* 采样板LED电压读取 */
    eProtocolEmitPack_Client_CMD_SP_LED_SET = 0x33, /* 采样板LED电压设置*/

    eProtocolEmitPack_Client_CMD_FA_PD_SET = 0x34, /* 采样板PD测试*/
    eProtocolEmitPack_Client_CMD_FA_LED_SET = 0x35, /* 采样板LED设置*/

    eProtocolEmitPack_Client_CMD_Correct = 0xC0, /* 校正数据 */

    eProtocolEmitPack_Client_CMD_Debug_Motor = 0xD0,        /* 调试用 电机控制 */
    eProtocolEmitPack_Client_CMD_Debug_Correct = 0xD2,      /* 调试用 循环定标 */
    eProtocolEmitPack_Client_CMD_Debug_Heater = 0xD3,       /* 调试用 加热控制 */
    eProtocolEmitPack_Client_CMD_Debug_Flag = 0xD4,         /* 调试用 标志位设置 */
    eProtocolEmitPack_Client_CMD_Debug_Beep = 0xD5,         /* 调试用 蜂鸣器控制 */
    eProtocolEmitPack_Client_CMD_Debug_Flash_Read = 0xD6,   /* 调试用 外部Flash读 */
    eProtocolEmitPack_Client_CMD_Debug_Flash_Write = 0xD7,  /* 调试用 外部Flash写 */
    eProtocolEmitPack_Client_CMD_Debug_EEPROM_Read = 0xD8,  /* 调试用 EEPROM读 */
    eProtocolEmitPack_Client_CMD_Debug_EEPROM_Write = 0xD9, /* 调试用 EEPROM写 */
    eProtocolEmitPack_Client_CMD_Debug_Self_Check = 0xDA,   /* 调试用 自检测试 */
    eProtocolEmitPack_Client_CMD_Debug_Motor_Fun = 0xDB,    /* 调试用 电机任务功能 */
    eProtocolEmitPack_Client_CMD_Debug_System = 0xDC,       /* 调试用 系统控制 */
    eProtocolEmitPack_Client_CMD_Debug_Params = 0xDD,       /* 调试用 参数配置 */
    eProtocolEmitPack_Client_CMD_Debug_BL = 0xDE,           /* 调试用 升级Bootloader */
    eProtocolEmitPack_Client_CMD_Debug_Version = 0xDF,      /* 调试用 编译日期信息 */
} eProtocolEmitPack_Client_CMD_Type;

typedef enum {
    eProtocolRespPack_Client_ACK = 0xAA,        /* 应答帧 */
    eProtocolRespPack_Client_TMP = 0xA0,        /* 温度数据帧 */
    eProtocolRespPack_Client_DISH = 0xB0,       /* 托盘状态帧 */
    eProtocolRespPack_Client_ID_CARD = 0xB1,    /* ID卡信息包 */
    eProtocolRespPack_Client_BARCODE = 0xB2,    /* 扫码信息帧 */
    eProtocolRespPack_Client_SAMP_DATA = 0xB3,  /* 采集数据帧 */
    eProtocolRespPack_Client_ERR = 0xB5,        /* 仪器错误帧 */
    eProtocolRespPack_Client_SAMP_OVER = 0xB6,  /* 采样完成帧 */
    eProtocolRespPack_Client_VER = 0xB7,        /* 版本信息帧 */
    eProtocolRespPack_Client_Debug_Temp = 0xEE, /* 温度上送 调试用 */
    eProtocolRespPack_Client_LED_Get = 0x32,    /* 采样板LED电压读取 */
    eProtocolRespPack_Client_FA_PD = 0x34,      /* 采样板工装PD输出 */
} eProtocolRespPack_Client_Type;

typedef enum {
    eComm_Out,
    eComm_Main,
    eComm_Data,
} eProtocol_COMM_Index;

typedef void (*pfProtocolFun)(uint8_t * pInBuff, uint8_t length, uint8_t * pOutBuff, uint8_t * pOutLength);

typedef struct {
    TickType_t tick;
    uint8_t ack_idx;
} sProcol_COMM_ACK_Record;

typedef enum {
    eProtocol_Debug_Temperature = (1 << 0),
    eProtocol_Debug_ErrorReport = (1 << 1),
    eProtocol_Debug_SampleBarcode = (1 << 2),
    eProtocol_Debug_SampleMotorTray = (1 << 3),
    eProtocol_Debug_SampleRawData = (1 << 4),
    eProtocol_Debug_AgingLoop = (1 << 5),
    eProtocol_Debug_Factory_Temp = (1 << 6),
    eProtocol_Debug_Item_Num = (1 << 7),
} eProtocol_Debug_Item;

/* Exported defines ----------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
uint8_t protocol_Debug_Get(void);
uint8_t protocol_Debug_Temperature(void);
uint8_t protocol_Debug_ErrorReport(void);
uint8_t protocol_Debug_SampleBarcode(void);
uint8_t protocol_Debug_SampleMotorTray(void);
uint8_t protocol_Debug_SampleRawData(void);
uint8_t protocol_Debug_AgingLoop(void);
uint8_t protocol_Debug_Factory_Temp(void);
void protocol_Debug_Mark(eProtocol_Debug_Item item);
void protocol_Debug_Clear(eProtocol_Debug_Item item);

uint8_t gProtocol_ACK_IndexGet(eProtocol_COMM_Index index);
void gProtocol_ACK_IndexAutoIncrease(eProtocol_COMM_Index index);

uint8_t buildPackOrigin(eProtocol_COMM_Index index, uint8_t cmdType, uint8_t * pData, uint8_t dataLength);
unsigned char CRC8(unsigned char * p, uint16_t len);
unsigned char CRC8_Check(unsigned char * p, uint16_t len);
uint8_t protocol_has_head(uint8_t * pBuff, uint16_t length);
uint16_t protocol_has_tail(uint8_t * pBuff, uint16_t length);
uint8_t protocol_is_comp(uint8_t * pBuff, uint16_t length);
BaseType_t protocol_is_NeedWaitRACK(uint8_t * pData);

uint8_t protocol_Parse_Out_ISR(uint8_t * pInBuff, uint16_t length);
uint8_t protocol_Parse_Main_ISR(uint8_t * pInBuff, uint16_t length);
uint8_t protocol_Parse_Data_ISR(uint8_t * pInBuff, uint16_t length);

void protocol_Temp_Upload_Resume(void);
void protocol_Temp_Upload_Pause(void);
void protocol_Temp_Upload_Comm_Set(eProtocol_COMM_Index comm_index, uint8_t sw);
void protocol_Temp_Upload_Deal(void);
void protocol_Self_Check_Temp_ALL(void);
#endif

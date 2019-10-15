/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include "serial.h"

/* Exported constants --------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/
typedef enum {
    PROTOCOL_PARSE_OK = 0x0000,
    PROTOCOL_PARSE_CRC_ERROR = 0x0001,
    PROTOCOL_PARSE_ID_ERROR = 0x0002,
    PROTOCOL_PARSE_LENGTH_ERROR = 0x0004,
    PROTOCOL_PARSE_CMD_ERROR = 0x0008,
    PROTOCOL_PARSE_DATA_ERROR = 0x0010,
    PROTOCOL_PARSE_EMIT_ERROR = 0x0020,
} eProtocolParseResult;

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
    eProtocolEmitPack_Client_CMD_UPGRADE = 0x0F, /* 下位机升级命令帧 */
} eProtocolEmitPack_Client_CMD_Type;

typedef enum {
    eProtocoleRespPack_Client_ACK = 0xAA,       /* 应答帧 */
    eProtocoleRespPack_Client_TMP = 0xA0,       /* 温度数据帧 */
    eProtocoleRespPack_Client_DISH = 0xB0,      /* 托盘状态帧 */
    eProtocoleRespPack_Client_ID_CARD = 0xB1,   /* ID卡信息包 */
    eProtocoleRespPack_Client_BARCODE = 0xB2,   /* 扫码信息帧 */
    eProtocoleRespPack_Client_SAMP_DATA = 0xB3, /* 采集数据帧 */
    eProtocoleRespPack_Client_ERR = 0xB5,       /* 仪器错误帧 */
    eProtocoleRespPack_Client_SAMP_OVER = 0xB6, /* 采样完成帧 */
    eProtocoleRespPack_Client_VER = 0xB7,       /* 版本信息帧 */
} eProtocoleRespPack_Client_Type;

typedef enum {
    eComm_Out,
    eComm_Main,
    eComm_Data,
} eProtocol_COMM_Index;

typedef eProtocolParseResult (*pfProtocolFun)(uint8_t * pInBuff, uint8_t length, uint8_t * pOutBuff, uint8_t * pOutLength);

/* Exported defines ----------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
uint8_t buildPackOrigin(eProtocol_COMM_Index index, uint8_t cmdType, uint8_t * pData, uint8_t dataLength);
unsigned char CRC8(unsigned char * p, uint16_t len);
unsigned char CRC8_Check(unsigned char * p, uint16_t len);
uint8_t protocol_has_head(uint8_t * pBuff, uint16_t length);
uint16_t protocol_has_tail(uint8_t * pBuff, uint16_t length);
uint8_t protocol_is_comp(uint8_t * pBuff, uint16_t length);
BaseType_t protocol_is_NeedWaitRACK(uint8_t * pData);
eProtocolParseResult protocol_Parse_Out(uint8_t * pInBuff, uint8_t length);
eProtocolParseResult protocol_Parse_Main(uint8_t * pInBuff, uint8_t length);
eProtocolParseResult protocol_Parse_Data(uint8_t * pInBuff, uint8_t length);
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "protocol.h"
#include "comm_out.h"
#include "comm_main.h"
#include "comm_data.h"
#include "m_l6470.h"
#include "m_drv8824.h"
#include "barcode_scan.h"
#include "tray_run.h"
#include "heat_motor.h"
#include "white_motor.h"
#include "motor.h"
#include "beep.h"
#include "soft_timer.h"
#include "storge_task.h"
#include "temperature.h"
#include "spi_flash.h"
#include "innate_flash.h"

/* Extern variables ----------------------------------------------------------*/
extern TIM_HandleTypeDef htim9;
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim6;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;

/* Private includes ----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/
typedef struct {
    uint8_t ACK_Out;  /* 向对方发送回应确认帧号 */
    uint8_t ACK_Main; /* 向对方发送回应确认帧号 */
    uint8_t ACK_Data; /* 向对方发送回应确认帧号 */
} sProtocol_ACK_Record;

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
static const unsigned char CRC8Table[256] = {
    0x00, 0x5e, 0xbc, 0xe2, 0x61, 0x3f, 0xdd, 0x83, 0xc2, 0x9c, 0x7e, 0x20, 0xa3, 0xfd, 0x1f, 0x41, 0x9d, 0xc3, 0x21, 0x7f, 0xfc, 0xa2, 0x40, 0x1e, 0x5f, 0x01,
    0xe3, 0xbd, 0x3e, 0x60, 0x82, 0xdc, 0x23, 0x7d, 0x9f, 0xc1, 0x42, 0x1c, 0xfe, 0xa0, 0xe1, 0xbf, 0x5d, 0x03, 0x80, 0xde, 0x3c, 0x62, 0xbe, 0xe0, 0x02, 0x5c,
    0xdf, 0x81, 0x63, 0x3d, 0x7c, 0x22, 0xc0, 0x9e, 0x1d, 0x43, 0xa1, 0xff, 0x46, 0x18, 0xfa, 0xa4, 0x27, 0x79, 0x9b, 0xc5, 0x84, 0xda, 0x38, 0x66, 0xe5, 0xbb,
    0x59, 0x07, 0xdb, 0x85, 0x67, 0x39, 0xba, 0xe4, 0x06, 0x58, 0x19, 0x47, 0xa5, 0xfb, 0x78, 0x26, 0xc4, 0x9a, 0x65, 0x3b, 0xd9, 0x87, 0x04, 0x5a, 0xb8, 0xe6,
    0xa7, 0xf9, 0x1b, 0x45, 0xc6, 0x98, 0x7a, 0x24, 0xf8, 0xa6, 0x44, 0x1a, 0x99, 0xc7, 0x25, 0x7b, 0x3a, 0x64, 0x86, 0xd8, 0x5b, 0x05, 0xe7, 0xb9, 0x8c, 0xd2,
    0x30, 0x6e, 0xed, 0xb3, 0x51, 0x0f, 0x4e, 0x10, 0xf2, 0xac, 0x2f, 0x71, 0x93, 0xcd, 0x11, 0x4f, 0xad, 0xf3, 0x70, 0x2e, 0xcc, 0x92, 0xd3, 0x8d, 0x6f, 0x31,
    0xb2, 0xec, 0x0e, 0x50, 0xaf, 0xf1, 0x13, 0x4d, 0xce, 0x90, 0x72, 0x2c, 0x6d, 0x33, 0xd1, 0x8f, 0x0c, 0x52, 0xb0, 0xee, 0x32, 0x6c, 0x8e, 0xd0, 0x53, 0x0d,
    0xef, 0xb1, 0xf0, 0xae, 0x4c, 0x12, 0x91, 0xcf, 0x2d, 0x73, 0xca, 0x94, 0x76, 0x28, 0xab, 0xf5, 0x17, 0x49, 0x08, 0x56, 0xb4, 0xea, 0x69, 0x37, 0xd5, 0x8b,
    0x57, 0x09, 0xeb, 0xb5, 0x36, 0x68, 0x8a, 0xd4, 0x95, 0xcb, 0x29, 0x77, 0xf4, 0xaa, 0x48, 0x16, 0xe9, 0xb7, 0x55, 0x0b, 0x88, 0xd6, 0x34, 0x6a, 0x2b, 0x75,
    0x97, 0xc9, 0x4a, 0x14, 0xf6, 0xa8, 0x74, 0x2a, 0xc8, 0x96, 0x15, 0x4b, 0xa9, 0xf7, 0xb6, 0xe8, 0x0a, 0x54, 0xd7, 0x89, 0x6b, 0x35,
};

static sProtocol_ACK_Record gProtocol_ACK_Record = {1, 1, 1};
static eComm_Data_Sample gComm_Data_Sample_Buffer[6];

static uint8_t gProtocol_Temp_Upload_Comm_Ctl = 0;
static uint8_t gProtocol_Temp_Upload_Comm_Suspend = 0;
/* Private function prototypes -----------------------------------------------*/

/* Private user code ---------------------------------------------------------*/

/**
 * @brief  向对方发送回应确认帧号 读取
 * @param  index 串口索引
 * @retval 向对方发送回应确认帧号
 */
uint8_t gProtocol_ACK_IndexGet(eProtocol_COMM_Index index)
{
    switch (index) {
        case eComm_Out:
            return gProtocol_ACK_Record.ACK_Out;
        case eComm_Main:
            return gProtocol_ACK_Record.ACK_Main;
        case eComm_Data:
            return gProtocol_ACK_Record.ACK_Data;
    }
    return 1;
}

/**
 * @brief  向对方发送回应确认帧号 自增
 * @param  index 串口索引
 * @retval None
 */
void gProtocol_ACK_IndexAutoIncrease(eProtocol_COMM_Index index)
{
    switch (index) {
        case eComm_Out:
            ++gProtocol_ACK_Record.ACK_Out;
            if (gProtocol_ACK_Record.ACK_Out == 0) {
                gProtocol_ACK_Record.ACK_Out = 1;
            }
            break;
        case eComm_Main:
            ++gProtocol_ACK_Record.ACK_Main;
            if (gProtocol_ACK_Record.ACK_Main == 0) {
                gProtocol_ACK_Record.ACK_Main = 1;
            }
            break;
        case eComm_Data:
            ++gProtocol_ACK_Record.ACK_Data;
            if (gProtocol_ACK_Record.ACK_Data == 0) {
                gProtocol_ACK_Record.ACK_Data = 1;
            }
            break;
    }
}

/**
 * @brief  判断是否需要等待回应包
 * @param  pData       发送的数据包
 * @retval 向对方发送回应确认帧号
 */
BaseType_t protocol_is_NeedWaitRACK(uint8_t * pData)
{
    if (pData[5] == eProtocoleRespPack_Client_ACK || pData[5] == eProtocoleRespPack_Client_ERR) {
        return pdFALSE;
    }
    return pdTRUE;
}

/**
 * @brief  CRC8 循环冗余校验
 * @param  p   数据指针
 * @param  len 数据长度
 * @retval CRC8校验和
 */
unsigned char CRC8(unsigned char * p, uint16_t len)
{
    unsigned char crc8 = 0;
    for (; len > 0; len--) {
        crc8 = CRC8Table[crc8 ^ *p]; //查表得到CRC码
        p++;
    }
    return crc8;
}

/**
 * @brief  CRC8 检验
 * @param  p   数据指针
 * @param  len 数据长度
 * @retval CRC8校验结果
 */
unsigned char CRC8_Check(unsigned char * p, uint16_t len)
{
    if (CRC8(p, len) == 0) {
        return 1;
    } else {
        return 0;
    }
}

/**
 * @brief  串口 1 包头判定
 * @param  pBuff 数据指针
 * @param  length 数据长度
 * @retval 0 非包头 1 包头
 */
uint8_t protocol_has_head(uint8_t * pBuff, uint16_t length)
{
    if (pBuff[0] == 0x69 && pBuff[1] == 0xAA) {
        return 1;
    }
    return 0;
}

/**
 * @brief  串口 1 包尾检测
 * @param  pBuff 数据指针
 * @param  length 数据长度
 * @retval 包尾位置 无包尾时返回0
 */
uint16_t protocol_has_tail(uint8_t * pBuff, uint16_t length)
{
    if (pBuff[2] + 4 > length) { /* 长度不足 */
        return 0;
    }
    return pBuff[2] + 4; /* 应有长度 */
}

/**
 * @brief  串口 1 完整性判断
 * @param  pBuff 数据指针
 * @param  length 数据长度
 * @retval 包尾位置 无包尾时返回0
 */
uint8_t protocol_is_comp(uint8_t * pBuff, uint16_t length)
{
    if (CRC8(pBuff + 4, length - 5) == pBuff[length - 1]) { /* CRC8 校验完整性 */
        return 1;
    }
    return 0;
}

/**
 * @brief  上位机及采样板通信协议包构造函数 原地版本
 * @param  index  协议出口类型
 * @param  cmdType 命令字
 * @param  pData 数据指针
 * @param  dataLength 数据长度
 * @note   设备ID 主控模块->0x41 | 控制模块->0x45 | 采集模块->0x46 | 工装->0x13
 * @retval 数据包长度
 */
uint8_t buildPackOrigin(eProtocol_COMM_Index index, uint8_t cmdType, uint8_t * pData, uint8_t dataLength)
{
    uint8_t i;

    for (i = dataLength; i > 0; --i) {
        pData[i - 1 + 6] = pData[i - 1];
    }

    gProtocol_ACK_IndexAutoIncrease(index);

    pData[0] = 0x69;
    pData[1] = 0xAA;
    pData[2] = 3 + dataLength;
    pData[3] = gProtocol_ACK_IndexGet(index);
    pData[4] = PROTOCOL_DEVICE_ID_CTRL;
    pData[5] = cmdType;
    pData[6 + dataLength] = CRC8(pData + 4, 2 + dataLength);
    return dataLength + 7;
}

/**
 * @brief  发送回应包
 * @param  index  协议出口类型
 * @param  pInbuff 入站包指针
 * @retval 数据包长度
 */
eProtocolParseResult protocol_Parse_AnswerACK(eProtocol_COMM_Index index, uint8_t ack)
{
    uint8_t sendLength;
    uint8_t send_buff[12];

    send_buff[0] = ack;
    sendLength = buildPackOrigin(index, eProtocoleRespPack_Client_ACK, send_buff, 1); /* 构造回应包 */
    switch (index) {
        case eComm_Out:
            if (serialSendStartDMA(eSerialIndex_5, send_buff, sendLength, 50) != pdPASS || comm_Out_DMA_TX_Wait(30) != pdPASS) { /* 发送回应包并等待发送完成 */
                return PROTOCOL_PARSE_EMIT_ERROR;
            }
            break;
        case eComm_Main:
            if (serialSendStartDMA(eSerialIndex_1, send_buff, sendLength, 50) != pdPASS || comm_Main_DMA_TX_Wait(30) != pdPASS) { /* 发送回应包并等待发送完成 */
                return PROTOCOL_PARSE_EMIT_ERROR;
            }
            break;
        case eComm_Data:
            if (serialSendStartDMA(eSerialIndex_2, send_buff, sendLength, 50) != pdPASS || comm_Data_DMA_TX_Wait(30) != pdPASS) { /* 发送回应包并等待发送完成 */
                return PROTOCOL_PARSE_EMIT_ERROR;
            }
            break;
    }

    return PROTOCOL_PARSE_OK;
}

/**
 * @brief  协议处理 获取版本号
 * @note   4 字节单精度浮点数 堆栈上顺序
 * @param  pBuff 输出缓冲
 * @retval None
 */
void protocol_Get_Version(uint8_t * pBuff)
{
    union {
        float f;
        uint8_t u8s[4];
    } vu;
    vu.f = APP_VERSION;
    pBuff[0] = vu.u8s[0];
    pBuff[1] = vu.u8s[1];
    pBuff[2] = vu.u8s[2];
    pBuff[3] = vu.u8s[3];
}

/**
 * @brief  温度主动上送 从暂停中恢复
 * @param  None
 * @retval None
 */
void protocol_Temp_Upload_Resume(void)
{
    gProtocol_Temp_Upload_Comm_Suspend = 0;
}

/**
 * @brief  温度主动上送 暂停
 * @param  None
 * @retval None
 */
void protocol_Temp_Upload_Pause(void)
{
    gProtocol_Temp_Upload_Comm_Suspend = 1;
}

/**
 * @brief  温度主动上送 暂停标志检查
 * @param  None
 * @retval 1 暂停中 0 没暂停
 */
uint8_t protocol_Temp_Upload_Is_Suspend(void)
{
    if (gProtocol_Temp_Upload_Comm_Suspend > 0) {
        return 1;
    }
    return 0;
}

/**
 * @brief  温度主动上送 串口发送许可 设置
 * @param  comm_index 串口索引
 * @param  sw 操作  1 使能  0 失能
 * @retval None
 */
void protocol_Temp_Upload_Comm_Set(eProtocol_COMM_Index comm_index, uint8_t sw)
{
    if (sw > 0) {
        gProtocol_Temp_Upload_Comm_Ctl |= (1 << comm_index);
    } else {
        gProtocol_Temp_Upload_Comm_Ctl &= 0xFF - (1 << comm_index);
    }
}

/**
 * @brief  温度主动上送 串口发送许可 获取
 * @param  comm_index 串口索引
 * @retval 1 使能  0 失能
 */
uint8_t protocol_Temp_Upload_Comm_Get(eProtocol_COMM_Index comm_index)
{
    if (gProtocol_Temp_Upload_Comm_Ctl & (1 << comm_index)) {
        return 1;
    } else {
        return 0;
    }
}

/**
 * @brief  温度主动上送处理 错误信息处理
 * @param  temp_btm 下加热体温度
 * @param  temp_top 上加热体温度
 * @param  now 系统时刻
 * @retval None
 */
void protocol_Temp_Upload_Error_Deal(TickType_t now, float temp_btm, float temp_top)
{
    static TickType_t xTick_btm_Keep_low = 0, xTick_btm_Keep_Hight = 0, xTick_top_Keep_low = 0, xTick_top_Keep_Hight = 0;
    static TickType_t xTick_btm_Nai = 0, xTick_top_Nai = 0;

    if (temp_btm < 36.5) {                                                           /* 温度值低于36.5 */
        xTick_btm_Keep_Hight = now;                                                  /* 温度过高计数清零 */
        if (xTick_btm_Keep_Hight - xTick_btm_Keep_low > 600 * pdMS_TO_TICKS(1000)) { /* 过低持续次数大于 10 Min */
            error_Emit(eError_Peripheral_Temp_Btm, eError_Temp_Too_Low);             /* 报错 */
            xTick_btm_Keep_low = xTick_btm_Keep_Hight;                               /* 重复报错间隔 */
        }
    } else if (temp_btm > 37.5) {            /* 温度值高于37.5 */
        if (temp_btm == TEMP_INVALID_DATA) { /* 温度值为无效值 */
            if (now - xTick_btm_Nai > 60 * pdMS_TO_TICKS(1000) || now < 100) {
                error_Emit(eError_Peripheral_Temp_Btm, eError_Temp_Nai); /* 报错 */
                xTick_btm_Nai = now;
            }
        } else {
            xTick_btm_Keep_low = now;                                                   /* 温度过低计数清零 */
            if (xTick_btm_Keep_low - xTick_btm_Keep_Hight > 60 * pdMS_TO_TICKS(1000)) { /* 过高持续次数大于 1 Min */
                error_Emit(eError_Peripheral_Temp_Btm, eError_Temp_Too_Hight);          /* 报错 */
                xTick_btm_Keep_Hight = xTick_btm_Keep_low;                              /* 重复报错间隔 */
            }
        }
    } else {                                       /* 温度值为36.5～37.5 */
        xTick_btm_Keep_low = now;                  /* 温度过低计数清零 */
        xTick_btm_Keep_Hight = xTick_btm_Keep_low; /* 温度过高计数清零 */
        xTick_btm_Nai = now;					   /* 温度无效计数清零 */
    }

    if (temp_top < 36.5) {                                                           /* 温度值低于36.5 */
        xTick_top_Keep_Hight = now;                                                  /* 温度过高计数清零 */
        if (xTick_top_Keep_Hight - xTick_top_Keep_low > 600 * pdMS_TO_TICKS(1000)) { /* 过低持续次数大于120次 5 S * 120=10 Min */
            error_Emit(eError_Peripheral_Temp_Top, eError_Temp_Too_Low);             /* 报错 */
            xTick_top_Keep_low = xTick_top_Keep_Hight;                               /* 重复报错间隔 */
        }
    } else if (temp_top > 37.5) {            /* 温度值高于37.5 */
        if (temp_top == TEMP_INVALID_DATA) { /* 温度值为无效值 */
            if (now - xTick_top_Nai > 60 * pdMS_TO_TICKS(1000) || now < 100) {
                error_Emit(eError_Peripheral_Temp_Top, eError_Temp_Nai); /* 报错 */
                xTick_top_Nai = now;
            }
        } else {
            xTick_top_Keep_low = now;                                                   /* 温度过低计数清零 */
            if (xTick_top_Keep_low - xTick_top_Keep_Hight > 60 * pdMS_TO_TICKS(1000)) { /* 过高持续次数大于 1 Min */
                error_Emit(eError_Peripheral_Temp_Top, eError_Temp_Too_Hight);          /* 报错 */
                xTick_top_Keep_Hight = xTick_top_Keep_low;                              /* 重复报错间隔 */
            }
        }
    } else {                        /* 温度值为36.5～37.5 */
        xTick_top_Keep_low = now;   /* 温度过低计数清零 */
        xTick_top_Keep_Hight = now; /* 温度过高计数清零 */
        xTick_top_Nai = now;		/* 温度无效计数清零 */
    }
}

/**
 * @brief  温度主动上送处理 主板串口
 * @param  temp_btm 下加热体温度
 * @param  temp_top 上加热体温度
 * @retval None
 */
void protocol_Temp_Upload_Main_Deal(float temp_btm, float temp_top)
{
    uint8_t buffer[10], length;

    if (protocol_Temp_Upload_Comm_Get(eComm_Main) == 0) { /* 无需进行串口发送 */
        return;
    }

    buffer[0] = ((uint16_t)(temp_btm * 100)) & 0xFF;                                /* 小端模式 低8位 */
    buffer[1] = ((uint16_t)(temp_btm * 100)) >> 8;                                  /* 小端模式 高8位 */
    buffer[2] = ((uint16_t)(temp_top * 100)) & 0xFF;                                /* 小端模式 低8位 */
    buffer[3] = ((uint16_t)(temp_top * 100)) >> 8;                                  /* 小端模式 高8位 */
    length = buildPackOrigin(eComm_Main, eProtocoleRespPack_Client_TMP, buffer, 4); /* 构造数据包 */

    if (comm_Main_SendTask_Queue_GetWaiting() == 0) {                         /* 允许发送且发送队列内没有其他数据包 */
        if (temp_btm != TEMP_INVALID_DATA || temp_top != TEMP_INVALID_DATA) { /* 温度值都不是无效值 */
            comm_Main_SendTask_QueueEmitCover(buffer, length);                /* 提交到发送队列 */
        }
    }
}

/**
 * @brief  温度主动上送处理 外串口
 * @param  temp_btm 下加热体温度
 * @param  temp_top 上加热体温度
 * @retval None
 */
void protocol_Temp_Upload_Out_Deal(float temp_btm, float temp_top)
{
    uint8_t buffer[48], length, i;
    float temperature;

    if (protocol_Temp_Upload_Comm_Get(eComm_Out) == 0) { /* 无需进行串口发送 */
        return;
    }

    buffer[0] = ((uint16_t)(temp_btm * 100)) & 0xFF;                               /* 小端模式 低8位 */
    buffer[1] = ((uint16_t)(temp_btm * 100)) >> 8;                                 /* 小端模式 高8位 */
    buffer[2] = ((uint16_t)(temp_top * 100)) & 0xFF;                               /* 小端模式 低8位 */
    buffer[3] = ((uint16_t)(temp_top * 100)) >> 8;                                 /* 小端模式 高8位 */
    length = buildPackOrigin(eComm_Out, eProtocoleRespPack_Client_TMP, buffer, 4); /* 构造数据包  */

    if (comm_Out_SendTask_Queue_GetWaiting() == 0) {                          /* 允许发送且发送队列内没有其他数据包 */
        if (temp_btm != TEMP_INVALID_DATA || temp_top != TEMP_INVALID_DATA) { /* 温度值都不是无效值 */
            comm_Out_SendTask_QueueEmitCover(buffer, length);                 /* 提交到发送队列 */
        }
        for (i = eTemp_NTC_Index_0; i <= eTemp_NTC_Index_8; ++i) {
            temperature = temp_Get_Temp_Data(i);
            memcpy(buffer + 4 * i, &temperature, 4);
        }
        length = buildPackOrigin(eComm_Out, 0xEE, buffer, 36); /* 构造数据包  */
        comm_Out_SendTask_QueueEmitCover(buffer, length);      /* 提交到发送队列 */
    }
}

/**
 * @brief  温度主动上送处理
 * @param  None
 * @retval None
 */
void protocol_Temp_Upload_Deal(void)
{
    float temp_top, temp_btm;
    TickType_t now;
    static TickType_t xTick_Main = 0, xTick_Out = 0;

    if (protocol_Temp_Upload_Is_Suspend()) { /* 暂停标志位 */
        return;
    }

    temp_btm = temp_Get_Temp_Data_BTM(); /* 下加热体温度 */
    temp_top = temp_Get_Temp_Data_TOP(); /* 上加热体温度 */

    now = xTaskGetTickCount();
    protocol_Temp_Upload_Error_Deal(now, temp_btm, temp_top);

    if (now - xTick_Main > 5 * pdMS_TO_TICKS(1000)) {
        xTick_Main = now;
        protocol_Temp_Upload_Main_Deal(temp_btm, temp_top);
    }
    if (now - xTick_Out > 1 * pdMS_TO_TICKS(1000)) {
        xTick_Out = now;
        protocol_Temp_Upload_Out_Deal(temp_btm, temp_top);
    }
}

/**
 * @brief  外串口解析协议
 * @param  pInBuff 入站指针
 * @param  length 入站长度
 * @param  pOutBuff 出站指针
 * @retval 解析数据包结果
 */
eProtocolParseResult protocol_Parse_Out(uint8_t * pInBuff, uint8_t length)
{
    eProtocolParseResult error = PROTOCOL_PARSE_OK;
    uint16_t status = 0;
    int32_t step;
    uint8_t result;
    sMotor_Fun motor_fun;
    float temp;

    if (pInBuff[4] == PROTOCOL_DEVICE_ID_CTRL) { /* 回声现象 */
        return PROTOCOL_PARSE_OK;
    }

    protocol_Temp_Upload_Comm_Set(eComm_Out, 1);       /* 通讯接收成功 使能本串口温度上送 */
    if (pInBuff[5] == eProtocoleRespPack_Client_ACK) { /* 收到对方回应帧 */
        comm_Out_Send_ACK_Give(pInBuff[6]);            /* 通知串口发送任务 回应包收到 */
        return PROTOCOL_PARSE_OK;                      /* 直接返回 */
    }

    if (pInBuff[4] == PROTOCOL_DEVICE_ID_SAMP) {             /* ID为数据板的数据包 直接透传 调试用 */
        pInBuff[4] = PROTOCOL_DEVICE_ID_CTRL;                /* 修正装置ID */
        pInBuff[length - 1] = CRC8(pInBuff + 4, length - 5); /* 重新校正CRC */
        comm_Data_SendTask_QueueEmitCover(pInBuff, length);  /* 提交到采集板发送任务 */
        return PROTOCOL_PARSE_OK;
    }

    error = protocol_Parse_AnswerACK(eComm_Out, pInBuff[3]); /* 发送回应包 */
    switch (pInBuff[5]) {                                    /* 进一步处理 功能码 */
        case 0xD0:
            if (length == 12) {
                m_l6470_Index_Switch(eM_L6470_Index_0, portMAX_DELAY);
                step = (uint32_t)(pInBuff[7] << 24) + (uint32_t)(pInBuff[8] << 16) + (uint32_t)(pInBuff[9] << 8) + (uint32_t)(pInBuff[10] << 0);

                switch (pInBuff[6]) {
                    case 0x00:
                        dSPIN_Move(REV, step); /* 向驱动发送指令 */
                        status = dSPIN_Get_Status();
                        pInBuff[0] = status >> 8;
                        pInBuff[1] = status & 0xFF;
                        error |= comm_Out_SendTask_QueueEmitWithBuildCover(0xD0, pInBuff, 2);
                        break;
                    case 0x01:
                    default:
                        dSPIN_Move(FWD, step); /* 向驱动发送指令 */
                        status = dSPIN_Get_Status();
                        pInBuff[0] = status >> 8;
                        pInBuff[1] = status & 0xFF;
                        error |= comm_Out_SendTask_QueueEmitWithBuildCover(0xD0, pInBuff, 2);
                        break;
                }
                vTaskDelay(1000);
                m_l6470_release();
            } else if (length == 8) {
                switch ((pInBuff[6] % 7)) {
                    case 0:
                        barcode_Scan_By_Index(eBarcodeIndex_0);
                        break;
                    case 1:
                        barcode_Scan_By_Index(eBarcodeIndex_1);
                        break;
                    case 2:
                        barcode_Scan_By_Index(eBarcodeIndex_2);
                        break;
                    case 3:
                        barcode_Scan_By_Index(eBarcodeIndex_3);
                        break;
                    case 4:
                        barcode_Scan_By_Index(eBarcodeIndex_4);
                        break;
                    case 5:
                        barcode_Scan_By_Index(eBarcodeIndex_5);
                        break;
                    case 6:
                        barcode_Scan_By_Index(eBarcodeIndex_6);
                        break;
                    default:
                        break;
                }
            }
            return error;
        case 0xD1:
            if (length == 12) {
                m_l6470_Index_Switch(eM_L6470_Index_1, portMAX_DELAY);
                step = (uint32_t)(pInBuff[7] << 24) + (uint32_t)(pInBuff[8] << 16) + (uint32_t)(pInBuff[9] << 8) + (uint32_t)(pInBuff[10] << 0);
                switch (pInBuff[6]) {
                    case 0x00:
                        dSPIN_Move(REV, step); /* 向驱动发送指令 */
                        status = dSPIN_Get_Status();
                        pInBuff[0] = status >> 8;
                        pInBuff[1] = status & 0xFF;
                        error |= comm_Out_SendTask_QueueEmitWithBuildCover(0xD0, pInBuff, 2);
                        break;
                    case 0x01:
                    default:
                        dSPIN_Move(FWD, step); /* 向驱动发送指令 */
                        status = dSPIN_Get_Status();
                        pInBuff[0] = status >> 8;
                        pInBuff[1] = status & 0xFF;
                        error |= comm_Out_SendTask_QueueEmitWithBuildCover(0xD0, pInBuff, 2);
                        break;
                }
                m_l6470_release();
            } else if (length == 8) {
                switch ((pInBuff[6] % 3)) {
                    case 0:
                        tray_Move_By_Index(eTrayIndex_0, 2000);
                        break;
                    case 1:
                        tray_Move_By_Index(eTrayIndex_1, 2000);
                        break;
                    case 2:
                        tray_Move_By_Index(eTrayIndex_2, 2000);
                        break;
                    default:
                        break;
                }
            }
            return error;
        case 0xD2:
            if (length == 9) {
                barcode_serial_Test();
                return error;
            } else if (length == 8) {
                barcode_Test(pInBuff[6]);
            }

            barcode_Read_From_Serial(&result, pInBuff, 100, 2000);
            if (result > 0) {
                error |= comm_Out_SendTask_QueueEmitWithBuildCover(0xD2, pInBuff, result);
            }
            break;
        case 0xD3:
            if (pInBuff[6] == 0) {
                heat_Motor_Up();
            } else {
                heat_Motor_Down();
            }
            break;
        case 0xD4:
            if (pInBuff[6] == 0) {
                white_Motor_PD();
            } else {
                white_Motor_WH();
            }
            break;
        case 0xD5:
            if (length != 14) {
                beep_Start();
            } else {
                beep_Start_With_Conf(pInBuff[6] % 7, (pInBuff[7] << 8) + pInBuff[8], (pInBuff[9] << 8) + pInBuff[10], (pInBuff[11] << 8) + pInBuff[12]);
            }
            break;
        case 0xD6: /* SPI Flash 读测试 */
            result = storgeReadConfInfo((pInBuff[6] << 16) + (pInBuff[7] << 8) + pInBuff[8], (pInBuff[9] << 16) + (pInBuff[10] << 8) + pInBuff[11], 200);
            if (result == 0 && storgeTaskNotification(eStorgeNotifyConf_Read_Falsh, eComm_Out) == pdPASS) { /* 通知存储任务 */
                error = PROTOCOL_PARSE_OK;
            } else {
                error |= PROTOCOL_PARSE_EMIT_ERROR;
            }
            return error;
        case 0xD7: /* SPI Flash 写测试 */
            result = storgeWriteConfInfo((pInBuff[6] << 16) + (pInBuff[7] << 8) + pInBuff[8], &pInBuff[12],
                                         (pInBuff[9] << 16) + (pInBuff[10] << 8) + pInBuff[11], 200);
            if (result == 0 && storgeTaskNotification(eStorgeNotifyConf_Write_Falsh, eComm_Out) == pdPASS) { /* 通知存储任务 */
                error = PROTOCOL_PARSE_OK;
            } else {
                error |= PROTOCOL_PARSE_EMIT_ERROR;
            }
            return error;
        case 0xD8: /* EEPROM 读测试 */
            result = storgeReadConfInfo((pInBuff[6] << 16) + (pInBuff[7] << 8) + pInBuff[8], (pInBuff[9] << 16) + (pInBuff[10] << 8) + pInBuff[11], 200);
            if (result == 0 && storgeTaskNotification(eStorgeNotifyConf_Read_ID_Card, eComm_Out) == pdPASS) { /* 通知存储任务 */
                error = PROTOCOL_PARSE_OK;
            } else {
                error |= PROTOCOL_PARSE_EMIT_ERROR;
            }
            return error;
        case 0xD9: /* EEPROM 写测试 */
            result = storgeWriteConfInfo((pInBuff[6] << 16) + (pInBuff[7] << 8) + pInBuff[8], &pInBuff[12],
                                         (pInBuff[9] << 16) + (pInBuff[10] << 8) + pInBuff[11], 200);
            if (result == 0 && storgeTaskNotification(eStorgeNotifyConf_Write_ID_Card, eComm_Out) == pdPASS) { /* 通知存储任务 */
                error = PROTOCOL_PARSE_OK;
            } else {
                error |= PROTOCOL_PARSE_EMIT_ERROR;
            }
            return error;
        case 0xDA:
            if (length < 8 || pInBuff[6] == 0) {
                HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_3);
            } else if (pInBuff[6] == 1) {
                HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
            } else if (pInBuff[6] == 2) {
                HAL_TIM_PWM_Stop(&htim4, TIM_CHANNEL_4);
            } else if (pInBuff[6] == 3) {
                HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_4);
            }
            break;
        case 0xDB:
            motor_fun.fun_type = (eMotor_Fun)pInBuff[6]; /* 开始测试 */
            motor_Emit(&motor_fun, 0);                   /* 提交到电机队列 */
            break;
        case 0xDC:
            vTaskDelay(50);
            HAL_NVIC_SystemReset(); /* 重新启动 */
            break;
        case 0xDD:
            if (length == 9) {                                  /* 保存参数 */
                status = (pInBuff[6] << 0) + (pInBuff[7] << 8); /* 起始索引 */
                if (status == 0xFFFF) {
                    if (storgeTaskNotification(eStorgeNotifyConf_Dump_Params, eComm_Out) != pdPASS) {
                        error |= PROTOCOL_PARSE_EMIT_ERROR;
                    }
                }
            } else if (length == 11) {                                                                                          /* 读取参数 */
                result = storgeReadConfInfo((pInBuff[6] << 0) + (pInBuff[7] << 8), (pInBuff[8] << 0) + (pInBuff[9] << 8), 200); /* 配置 */
                if (result == 0 && storgeTaskNotification(eStorgeNotifyConf_Read_Parmas, eComm_Out) == pdPASS) {                /* 通知存储任务 */
                    error = PROTOCOL_PARSE_OK;
                } else {
                    error |= PROTOCOL_PARSE_EMIT_ERROR;
                }
            } else if (length > 11 && ((length - 11) % 4 == 0)) { /* 写入参数 */
                result = storgeWriteConfInfo((pInBuff[6] << 0) + (pInBuff[7] << 8), &pInBuff[10], 4 * ((pInBuff[8] << 0) + (pInBuff[9] << 8)), 200); /* 配置 */
                if (result == 0 && storgeTaskNotification(eStorgeNotifyConf_Write_Parmas, eComm_Out) == pdPASS) { /* 通知存储任务 */
                    error = PROTOCOL_PARSE_OK;
                } else {
                    error |= PROTOCOL_PARSE_EMIT_ERROR;
                }
            } else {
                error |= PROTOCOL_PARSE_LENGTH_ERROR;
            }
            break;
        case 0xDE:                                              /* 升级Bootloader */
            if ((pInBuff[10] << 0) + (pInBuff[11] << 8) == 0) { /* 数据长度为空 尾包 */
                result = Innate_Flash_Dump((pInBuff[6] << 0) + (pInBuff[7] << 8),
                                           (pInBuff[12] << 0) + (pInBuff[13] << 8) + (pInBuff[14] << 16) + (pInBuff[15] << 24));
                pInBuff[0] = result;
                error |= comm_Out_SendTask_QueueEmitWithBuildCover(0xDE, pInBuff, 1);
            } else {                                              /* 数据长度不为空 非尾包 */
                if ((pInBuff[8] << 0) + (pInBuff[9] << 8) == 0) { /* 起始包 */
                    result = Innate_Flash_Erase_Temp();           /* 擦除Flash */
                    if (result > 0) {                             /* 擦除失败 */
                        HAL_FLASH_Lock();                         /* 回锁 */
                        pInBuff[0] = result;
                        error |= comm_Out_SendTask_QueueEmitWithBuildCover(0xDE, pInBuff, 1);
                        break;
                    }
                }
                result =
                    Innate_Flash_Write(INNATE_FLASH_ADDR_TEMP + (pInBuff[8] << 0) + (pInBuff[9] << 8), pInBuff + 12, (pInBuff[10] << 0) + (pInBuff[11] << 8));
                pInBuff[0] = result;
                error |= comm_Out_SendTask_QueueEmitWithBuildCover(0xDE, pInBuff, 1);
            }
            break;
        case eProtocolEmitPack_Client_CMD_START:          /* 开始测量帧 0x01 */
            comm_Data_Sample_Send_Conf();                 /* 发送测试配置 */
            protocol_Temp_Upload_Pause();                 /* 暂停温度上送 */
            motor_fun.fun_type = eMotor_Fun_Sample_Start; /* 开始测试 */
            motor_Emit(&motor_fun, 0);                    /* 提交到电机队列 */
            break;
        case eProtocolEmitPack_Client_CMD_ABRUPT:    /* 仪器测量取消命令帧 0x02 */
            barcode_Interrupt_Flag_Mark();           /* 标记打断扫码 */
            comm_Data_Sample_Force_Stop();           /* 强行停止采样定时器 */
            motor_Sample_Info(eMotorNotifyValue_BR); /* 提交打断信息 */
            break;
        case eProtocolEmitPack_Client_CMD_CONFIG:     /* 测试项信息帧 0x03 */
            comm_Data_Sample_Apply_Conf(&pInBuff[6]); /* 保存测试配置 */
            break;
        case eProtocolEmitPack_Client_CMD_FORWARD: /* 打开托盘帧 0x04 */
            motor_fun.fun_type = eMotor_Fun_Out;   /* 配置电机动作套餐类型 出仓 */
            motor_Emit(&motor_fun, 0);             /* 交给电机任务 出仓 */
            break;
        case eProtocolEmitPack_Client_CMD_REVERSE: /* 关闭托盘命令帧 0x05 */
            motor_fun.fun_type = eMotor_Fun_In;    /* 配置电机动作套餐类型 进仓 */
            motor_Emit(&motor_fun, 0);             /* 交给电机任务 进仓 */
            break;
        case eProtocolEmitPack_Client_CMD_READ_ID:                                                            /* ID卡读取命令帧 0x06 */
            result = storgeReadConfInfo(0, 4096, 200);                                                        /* 暂无定义 按最大读取 */
            if (result == 0 && storgeTaskNotification(eStorgeNotifyConf_Read_ID_Card, eComm_Out) == pdPASS) { /* 通知存储任务 */
                error = PROTOCOL_PARSE_OK;
            } else {
                error |= PROTOCOL_PARSE_EMIT_ERROR;
            }
            break;
        case eProtocolEmitPack_Client_CMD_STATUS:                                                          /* 状态信息查询帧 (首帧) */
            temp = temp_Get_Temp_Data_BTM();                                                               /* 下加热体温度 */
            pInBuff[0] = ((uint16_t)(temp * 100)) & 0xFF;                                                  /* 小端模式 低8位 */
            pInBuff[1] = ((uint16_t)(temp * 100)) >> 8;                                                    /* 小端模式 高8位 */
            temp = temp_Get_Temp_Data_TOP();                                                               /* 上加热体温度 */
            pInBuff[2] = ((uint16_t)(temp * 100)) & 0xFF;                                                  /* 小端模式 低8位 */
            pInBuff[3] = ((uint16_t)(temp * 100)) >> 8;                                                    /* 小端模式 高8位 */
            error |= comm_Out_SendTask_QueueEmitWithBuildCover(eProtocoleRespPack_Client_TMP, pInBuff, 4); /* 温度信息 */

            protocol_Get_Version(pInBuff);
            error |= comm_Out_SendTask_QueueEmitWithBuildCover(eProtocoleRespPack_Client_VER, pInBuff, 4); /* 软件版本信息 */

            if (tray_Motor_Get_Status_Position() == 0) {
                pInBuff[0] = 1; /* 托盘处于测试位置 原点 */
            } else if (tray_Motor_Get_Status_Position() >= (eTrayIndex_2 / 4 - 50) && tray_Motor_Get_Status_Position() <= (eTrayIndex_2 / 4 + 50)) {
                pInBuff[0] = 2; /* 托盘处于出仓位置 误差范围 +-50步 */
            } else {
                pInBuff[0] = 0;
            }
            error |= comm_Out_SendTask_QueueEmitWithBuildCover(eProtocoleRespPack_Client_DISH, pInBuff, 1); /* 托盘状态信息 */
            break;
        case eProtocolEmitPack_Client_CMD_UPGRADE: /* 下位机升级命令帧 0x0F */
            if (spi_FlashWriteAndCheck_Word(0x0000, 0x87654321) == 0) {
                HAL_NVIC_SystemReset(); /* 重新启动 */
            } else {
                error_Emit(eError_Peripheral_Storge_Flash, eError_Storge_Write_Error);
            }
            break;
        default:
            error |= PROTOCOL_PARSE_CMD_ERROR;
            break;
    }
    return error;
}

/**
 * @brief  上位机解析协议
 * @param  pInBuff 入站指针
 * @param  length 入站长度
 * @param  pOutBuff 出站指针
 * @retval 解析数据包结果
 */
eProtocolParseResult protocol_Parse_Main(uint8_t * pInBuff, uint8_t length)
{
    eProtocolParseResult error = PROTOCOL_PARSE_OK;
    uint8_t result;
    sMotor_Fun motor_fun;
    float temp;

    if (pInBuff[4] == PROTOCOL_DEVICE_ID_CTRL) { /* 回声现象 */
        return PROTOCOL_PARSE_OK;
    }

    gComm_Main_Connected_Set_Enable();                 /* 标记通信成功 */
    if (pInBuff[5] == eProtocoleRespPack_Client_ACK) { /* 收到对方回应帧 */
        comm_Main_Send_ACK_Give(pInBuff[6]);           /* 通知串口发送任务 回应包收到 */
        return PROTOCOL_PARSE_OK;                      /* 直接返回 */
    }

    protocol_Temp_Upload_Comm_Set(eComm_Main, 1); /* 通讯接收成功 使能本串口温度上送 */

    error = protocol_Parse_AnswerACK(eComm_Main, pInBuff[3]); /* 发送回应包 */
    switch (pInBuff[5]) {                                     /* 进一步处理 功能码 */
        case eProtocolEmitPack_Client_CMD_START:              /* 开始测量帧 0x01 */
            comm_Data_Sample_Send_Conf();                     /* 发送测试配置 */
            protocol_Temp_Upload_Pause();                     /* 暂停温度上送 */
            motor_fun.fun_type = eMotor_Fun_Sample_Start;     /* 开始测试 */
            motor_Emit(&motor_fun, 0);                        /* 提交到电机队列 */
            break;
        case eProtocolEmitPack_Client_CMD_ABRUPT:    /* 仪器测量取消命令帧 0x02 */
            comm_Data_Sample_Force_Stop();           /* 强行停止采样定时器 */
            motor_Sample_Info(eMotorNotifyValue_BR); /* 提交打断信息 */
            break;
        case eProtocolEmitPack_Client_CMD_CONFIG:     /* 测试项信息帧 0x03 */
            comm_Data_Sample_Apply_Conf(&pInBuff[6]); /* 保存测试配置 */
            break;
        case eProtocolEmitPack_Client_CMD_FORWARD: /* 打开托盘帧 0x04 */
            motor_fun.fun_type = eMotor_Fun_Out;   /* 配置电机动作套餐类型 出仓 */
            motor_Emit(&motor_fun, 0);             /* 交给电机任务 出仓 */
            break;
        case eProtocolEmitPack_Client_CMD_REVERSE: /* 关闭托盘命令帧 0x05 */
            motor_fun.fun_type = eMotor_Fun_In;    /* 配置电机动作套餐类型 进仓 */
            motor_Emit(&motor_fun, 0);             /* 交给电机任务 进仓 */
            break;
        case eProtocolEmitPack_Client_CMD_READ_ID:                                                             /* ID卡读取命令帧 0x06 */
            result = storgeReadConfInfo(0, 4096, 200);                                                         /* 暂无定义 按最大读取 */
            if (result == 0 && storgeTaskNotification(eStorgeNotifyConf_Read_ID_Card, eComm_Main) == pdPASS) { /* 通知存储任务 */
                error = PROTOCOL_PARSE_OK;
            } else {
                error |= PROTOCOL_PARSE_EMIT_ERROR;
            }
            break;
        case eProtocolEmitPack_Client_CMD_STATUS:                                                           /* 状态信息查询帧 (首帧) */
            temp = temp_Get_Temp_Data_BTM();                                                                /* 下加热体温度 */
            pInBuff[0] = ((uint16_t)(temp * 100)) & 0xFF;                                                   /* 小端模式 低8位 */
            pInBuff[1] = ((uint16_t)(temp * 100)) >> 8;                                                     /* 小端模式 高8位 */
            temp = temp_Get_Temp_Data_TOP();                                                                /* 上加热体温度 */
            pInBuff[2] = ((uint16_t)(temp * 100)) & 0xFF;                                                   /* 小端模式 低8位 */
            pInBuff[3] = ((uint16_t)(temp * 100)) >> 8;                                                     /* 小端模式 高8位 */
            error |= comm_Main_SendTask_QueueEmitWithBuildCover(eProtocoleRespPack_Client_TMP, pInBuff, 4); /* 温度信息 */

            protocol_Get_Version(pInBuff);
            error |= comm_Main_SendTask_QueueEmitWithBuildCover(eProtocoleRespPack_Client_VER, pInBuff, 4); /* 软件版本信息 */

            if (tray_Motor_Get_Status_Position() == 0) {
                pInBuff[0] = 1; /* 托盘处于测试位置 原点 */
            } else if (tray_Motor_Get_Status_Position() >= (eTrayIndex_2 / 4 - 50) && tray_Motor_Get_Status_Position() <= (eTrayIndex_2 / 4 + 50)) {
                pInBuff[0] = 2; /* 托盘处于出仓位置 误差范围 +-50步 */
            } else {
                pInBuff[0] = 0;
            }
            error |= comm_Main_SendTask_QueueEmitWithBuildCover(eProtocoleRespPack_Client_DISH, pInBuff, 1); /* 托盘状态信息 */
            break;
        case eProtocolEmitPack_Client_CMD_UPGRADE: /* 下位机升级命令帧 0x0F */
            if (spi_FlashWriteAndCheck_Word(0x0000, 0x87654321) == 0) {
                HAL_NVIC_SystemReset(); /* 重新启动 */
            } else {
                error_Emit(eError_Peripheral_Storge_Flash, eError_Storge_Write_Error);
            }
            break;
        default:
            error |= PROTOCOL_PARSE_CMD_ERROR;
            break;
    }
    return error;
}

/**
 * @brief  采样板解析协议
 * @param  pInBuff 入站指针
 * @param  length 入站长度
 * @param  pOutBuff 出站指针
 * @retval 解析数据包结果
 */
eProtocolParseResult protocol_Parse_Data(uint8_t * pInBuff, uint8_t length)
{
    eProtocolParseResult error = PROTOCOL_PARSE_OK;
    uint8_t i, cnt;

    if (pInBuff[4] == PROTOCOL_DEVICE_ID_CTRL) { /* 回声现象 */
        return PROTOCOL_PARSE_OK;
    }

    if (pInBuff[5] == eProtocoleRespPack_Client_ACK) { /* 收到对方回应帧 */
        comm_Data_Send_ACK_Notify(pInBuff[6]);         /* 通知串口发送任务 回应包收到 */
        return PROTOCOL_PARSE_OK;                      /* 直接返回 */
    }

    error = protocol_Parse_AnswerACK(eComm_Data, pInBuff[3]); /* 发送回应包 */
    switch (pInBuff[5]) {                                     /* 进一步处理 功能码 */
        case eComm_Data_Inbound_CMD_DATA:                     /* 采集数据帧 */
            if (pInBuff[7] < 1 || pInBuff[7] > 6) {           /* 检查通道编码 */
                break;
            }
            gComm_Data_Sample_Buffer[pInBuff[7] - 1].num = pInBuff[6];           /* 数据个数 u16 */
            gComm_Data_Sample_Buffer[pInBuff[7] - 1].channel = pInBuff[7];       /* 通道编码 */
            for (i = 0; i < gComm_Data_Sample_Buffer[pInBuff[7] - 1].num; ++i) { /* 具体数据 */
                gComm_Data_Sample_Buffer[pInBuff[7] - 1].data[i] = pInBuff[8 + (i * 2)] + (pInBuff[9 + (i * 2)] << 8);
            }
            cnt = pInBuff[6] * 2 + 2;
            comm_Main_SendTask_QueueEmitWithBuild(eProtocoleRespPack_Client_SAMP_DATA, &pInBuff[6], cnt, 20);
            comm_Out_SendTask_QueueEmitWithModify(pInBuff + 6, length, 0);
            break;
        case eComm_Data_Inbound_CMD_OVER:     /* 采集数据完成帧 */
            comm_Data_Sample_Complete_Deal(); /* 释放采样完成信号量 */
            break;
        default:
            error |= PROTOCOL_PARSE_CMD_ERROR;
            break;
    }
    return error;
}

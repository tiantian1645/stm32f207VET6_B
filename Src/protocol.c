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
#include "heater.h"

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

static uint8_t gProtocol_Temp_Upload_Comm_Ctl = 0;
static uint8_t gProtocol_Temp_Upload_Comm_Suspend = 0;

static uint8_t gProtocol_Debug_Flag = eProtocol_Debug_ErrorReport;
/* Private function prototypes -----------------------------------------------*/
static uint8_t protocol_Is_Debug(eProtocol_Debug_Item item);

/* Private user code ---------------------------------------------------------*/

/**
 * @brief  调试模式标志 获取
 * @param  item 项目组别
 * @retval 0 非调试 1 调试
 * @note   参考 eProtocol_Debug_Item
 */
static uint8_t protocol_Is_Debug(eProtocol_Debug_Item item)
{
    return (gProtocol_Debug_Flag & item);
}

/**
 * @brief  调试模式标志 获取
 * @retval 0 非调试 1 调试
 */
uint8_t protocol_Debug_Get(void)
{
    return gProtocol_Debug_Flag;
}

/**
 * @brief  调试标志 温度
 * @param  None
 * @retval 1 使能 0 失能
 */
uint8_t protocol_Debug_Temperature(void)
{
    return protocol_Is_Debug(eProtocol_Debug_Temperature);
}

/**
 * @brief  调试标志 错误上送
 * @param  None
 * @retval 1 使能 0 失能
 */
uint8_t protocol_Debug_ErrorReport(void)
{
    return protocol_Is_Debug(eProtocol_Debug_ErrorReport);
}

/**
 * @brief  调试标志 采样时扫码
 * @param  None
 * @retval 1 使能 0 失能
 */
uint8_t protocol_Debug_SampleBarcode(void)
{
    return protocol_Is_Debug(eProtocol_Debug_SampleBarcode);
}

/**
 * @brief  调试标志 采样时托盘动作
 * @param  None
 * @retval 1 使能 0 失能
 */
uint8_t protocol_Debug_SampleMotorTray(void)
{
    return protocol_Is_Debug(eProtocol_Debug_SampleMotorTray);
}

/**
 * @brief  调试标志 采样数据校正映射选择
 * @param  None
 * @retval 1 使能 0 失能
 */
uint8_t protocol_Debug_SampleRawData(void)
{
    return protocol_Is_Debug(eProtocol_Debug_SampleRawData);
}

/**
 * @brief  调试模式标志 置位
 * @param  item 项目组别
 * @retval None
 * @note   参考 eProtocol_Debug_Item
 */
void protocol_Debug_Mark(eProtocol_Debug_Item item)
{
    gProtocol_Debug_Flag |= item;
}

/**
 * @brief  调试模式标志 清零
 * @param  item 项目组别
 * @retval None
 * @note   参考 eProtocol_Debug_Item
 */
void protocol_Debug_Clear(eProtocol_Debug_Item item)
{
    gProtocol_Debug_Flag &= (eProtocol_Debug_Item_Num - 1 - item);
}

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
    if (pData[5] == eProtocolRespPack_Client_ACK || pData[5] == eProtocolRespPack_Client_ERR) {
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

    if (dataLength > 0) {
        for (i = dataLength; i > 0; --i) {
            pData[i - 1 + 6] = pData[i - 1];
        }
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
 * @retval None
 */
void protocol_Parse_AnswerACK(eProtocol_COMM_Index index, uint8_t ack)
{
    uint8_t sendLength;
    uint8_t send_buff[12];

    send_buff[0] = ack;
    sendLength = buildPackOrigin(index, eProtocolRespPack_Client_ACK, send_buff, 1); /* 构造回应包 */
    switch (index) {
        case eComm_Out:
            if (serialSendStartDMA(eSerialIndex_5, send_buff, sendLength, 50) != pdPASS || comm_Out_DMA_TX_Wait(30) != pdPASS) { /* 发送回应包并等待发送完成 */
                error_Emit(eError_Comm_Out_Send_Failed);
            }
            break;
        case eComm_Main:
            if (serialSendStartDMA(eSerialIndex_1, send_buff, sendLength, 50) != pdPASS || comm_Main_DMA_TX_Wait(30) != pdPASS) { /* 发送回应包并等待发送完成 */
                error_Emit(eError_Comm_Main_Send_Failed);
            }
            break;
        case eComm_Data:
            if (serialSendStartDMA(eSerialIndex_2, send_buff, sendLength, 50) != pdPASS || comm_Data_DMA_TX_Wait(30) != pdPASS) { /* 发送回应包并等待发送完成 */
                error_Emit(eError_Comm_Data_Send_Failed);
            }
            break;
    }
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
    float setpoint = 37;

    setpoint = heater_BTM_Conf_Get(eHeater_PID_Conf_Set_Point);                      /* 获取目标温度 */
    if (temp_btm < setpoint - 0.5) {                                                 /* 温度值低于36.5 */
        xTick_btm_Keep_Hight = now;                                                  /* 温度过高计数清零 */
        if (xTick_btm_Keep_Hight - xTick_btm_Keep_low > 600 * pdMS_TO_TICKS(1000)) { /* 过低持续次数大于 10 Min */
            error_Emit(eError_Temperature_Btm_TooLow);                               /* 报错 */
            xTick_btm_Keep_low = xTick_btm_Keep_Hight;                               /* 重复报错间隔 */
        }
    } else if (temp_btm > setpoint + 0.5) {  /* 温度值高于37.5 */
        if (temp_btm == TEMP_INVALID_DATA) { /* 温度值为无效值 */
            if (now - xTick_btm_Nai > 60 * pdMS_TO_TICKS(1000) || now < 100) {
                error_Emit(eError_Temperature_Btm_Abnormal); /* 报错 */
                xTick_btm_Nai = now;
            }
        } else {
            xTick_btm_Keep_low = now;                                                   /* 温度过低计数清零 */
            if (xTick_btm_Keep_low - xTick_btm_Keep_Hight > 60 * pdMS_TO_TICKS(1000)) { /* 过高持续时间大于 1 Min */
                error_Emit(eError_Temperature_Btm_TooHigh);                             /* 报错 */
                xTick_btm_Keep_Hight = xTick_btm_Keep_low;                              /* 重复报错间隔 */
            }
        }
    } else {                                       /* 温度值为36.5～37.5 */
        xTick_btm_Keep_low = now;                  /* 温度过低计数清零 */
        xTick_btm_Keep_Hight = xTick_btm_Keep_low; /* 温度过高计数清零 */
        xTick_btm_Nai = now;                       /* 温度无效计数清零 */
    }

    setpoint = heater_TOP_Conf_Get(eHeater_PID_Conf_Set_Point);                      /* 获取目标温度 */
    if (temp_top < setpoint - 0.5) {                                                 /* 温度值低于36.5 */
        xTick_top_Keep_Hight = now;                                                  /* 温度过高计数清零 */
        if (xTick_top_Keep_Hight - xTick_top_Keep_low > 600 * pdMS_TO_TICKS(1000)) { /* 过低持续时间 10 Min */
            error_Emit(eError_Temperature_Top_TooLow);                               /* 报错 */
            xTick_top_Keep_low = xTick_top_Keep_Hight;                               /* 重复报错间隔 */
        }
    } else if (temp_top > setpoint + 0.5) {  /* 温度值高于37.5 */
        if (temp_top == TEMP_INVALID_DATA) { /* 温度值为无效值 */
            if (now - xTick_top_Nai > 60 * pdMS_TO_TICKS(1000) || now < 100) {
                error_Emit(eError_Temperature_Top_Abnormal); /* 报错 */
                xTick_top_Nai = now;
            }
        } else {
            xTick_top_Keep_low = now;                                                   /* 温度过低计数清零 */
            if (xTick_top_Keep_low - xTick_top_Keep_Hight > 60 * pdMS_TO_TICKS(1000)) { /* 过高持续时间大于 1 Min */
                error_Emit(eError_Temperature_Top_TooHigh);                             /* 报错 */
                xTick_top_Keep_Hight = xTick_top_Keep_low;                              /* 重复报错间隔 */
            }
        }
    } else {                        /* 温度值为36.5～37.5 */
        xTick_top_Keep_low = now;   /* 温度过低计数清零 */
        xTick_top_Keep_Hight = now; /* 温度过高计数清零 */
        xTick_top_Nai = now;        /* 温度无效计数清零 */
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

    buffer[0] = ((uint16_t)(temp_btm * 100 + 0.5)) & 0xFF;                         /* 小端模式 低8位 */
    buffer[1] = ((uint16_t)(temp_btm * 100 + 0.5)) >> 8;                           /* 小端模式 高8位 */
    buffer[2] = ((uint16_t)(temp_top * 100 + 0.5)) & 0xFF;                         /* 小端模式 低8位 */
    buffer[3] = ((uint16_t)(temp_top * 100 + 0.5)) >> 8;                           /* 小端模式 高8位 */
    length = buildPackOrigin(eComm_Main, eProtocolRespPack_Client_TMP, buffer, 4); /* 构造数据包 */

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

    buffer[0] = ((uint16_t)(temp_btm * 100 + 0.5)) & 0xFF;                        /* 小端模式 低8位 */
    buffer[1] = ((uint16_t)(temp_btm * 100 + 0.5)) >> 8;                          /* 小端模式 高8位 */
    buffer[2] = ((uint16_t)(temp_top * 100 + 0.5)) & 0xFF;                        /* 小端模式 低8位 */
    buffer[3] = ((uint16_t)(temp_top * 100 + 0.5)) >> 8;                          /* 小端模式 高8位 */
    length = buildPackOrigin(eComm_Out, eProtocolRespPack_Client_TMP, buffer, 4); /* 构造数据包  */

    if (comm_Out_SendTask_Queue_GetWaiting() == 0) {                          /* 允许发送且发送队列内没有其他数据包 */
        if (temp_btm != TEMP_INVALID_DATA || temp_top != TEMP_INVALID_DATA) { /* 温度值都不是无效值 */
            comm_Out_SendTask_QueueEmitCover(buffer, length);                 /* 提交到发送队列 */
        }
        for (i = eTemp_NTC_Index_0; i <= eTemp_NTC_Index_8; ++i) {
            temperature = temp_Get_Temp_Data(i);
            memcpy(buffer + 4 * i, &temperature, 4);
        }
        if (protocol_Debug_Temperature()) {                                                       /* 使能温度调试 */
            length = buildPackOrigin(eComm_Out, eProtocolRespPack_Client_Debug_Temp, buffer, 36); /* 构造数据包  */
            comm_Out_SendTask_QueueEmitCover(buffer, length);                                     /* 提交到发送队列 */
        }
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

    if (protocol_Debug_Temperature()) {
        if (now - xTick_Out > 0.5 * pdMS_TO_TICKS(1000)) {
            xTick_Out = now;
            protocol_Temp_Upload_Out_Deal(temp_btm, temp_top);
        }
    } else {
        if (now - xTick_Out > 5 * pdMS_TO_TICKS(1000)) {
            xTick_Out = now;
            protocol_Temp_Upload_Out_Deal(temp_btm, temp_top);
        }
    }
}

/**
 * @brief  自检测试 温度判断
 * @param  pBuffer 数据指针
 * @retval 0 正常 1 异常 2 过高 3 过低
 **/
void protocol_Self_Check_Temp_TOP(uint8_t * pBuffer)
{
    float temp;
    uint8_t result = 0, i;

    temp = temp_Get_Temp_Data_TOP(); /* 读取温度值 */
    if (temp == TEMP_INVALID_DATA) { /* 排除无效值 */
        result = 1;
    } else if (temp > 37.3) { /* 温度过高 */
        result = 2;
    } else if (temp < 36.7) { /* 温度过低 */
        result = 3;
    } else {
        result = 0; /* 正常范围 */
    }

    for (i = 0; i < 6; ++i) {
        temp = temp_Get_Temp_Data(i); /* 读取温度值 */
        memcpy(pBuffer + 2 + 4 * i, (uint8_t *)(&temp), 4);
    }

    pBuffer[0] = 1;
    pBuffer[1] = result;
    comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 26);
}

/**
 * @brief  自检测试 温度判断
 * @param  pBuffer 数据指针
 * @retval 0 正常 1 异常 2 过高 3 过低
 **/
void protocol_Self_Check_Temp_BTM(uint8_t * pBuffer)
{
    float temp;
    uint8_t result = 0, i;

    temp = temp_Get_Temp_Data_BTM(); /* 读取温度值 */
    if (temp == TEMP_INVALID_DATA) { /* 排除无效值 */
        result = 1;
    } else if (temp > 37.3) { /* 温度过高 */
        result = 2;
    } else if (temp < 36.7) { /* 温度过低 */
        result = 3;
    } else {
        result = 0; /* 正常范围 */
    }

    for (i = 0; i < 2; ++i) {
        temp = temp_Get_Temp_Data(6 + i); /* 读取温度值 */
        memcpy(pBuffer + 2 + 4 * i, (uint8_t *)(&temp), 4);
    }

    pBuffer[0] = 2;
    pBuffer[1] = result;
    comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 10);
}

/**
 * @brief  自检测试 温度判断
 * @param  pBuffer 数据指针
 * @retval 0 正常 1 异常 2 过高 3 过低
 **/
void protocol_Self_Check_Temp_ENV(uint8_t * pBuffer)
{
    float temp;
    uint8_t result = 0, i;

    temp = temp_Get_Temp_Data_ENV(); /* 读取温度值 */
    if (temp == TEMP_INVALID_DATA) { /* 排除无效值 */
        result = 1;
    } else if (temp > 46) { /* 温度过高 */
        result = 2;
    } else if (temp < 16) { /* 温度过低 */
        result = 3;
    } else {
        result = 0; /* 正常范围 */
    }

    for (i = 0; i < 1; ++i) {
        temp = temp_Get_Temp_Data(8 + i); /* 读取温度值 */
        memcpy(pBuffer + 2 + 4 * i, (uint8_t *)(&temp), 4);
    }

    pBuffer[0] = 3;
    pBuffer[1] = result;
    comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 6);
}

/**
 * @brief  外串口解析协议
 * @param  pInBuff 入站指针
 * @param  length 入站长度
 * @param  pOutBuff 出站指针
 * @retval None
 */
void protocol_Parse_Out(uint8_t * pInBuff, uint8_t length)
{
    uint16_t status = 0;
    int32_t step;
    uint8_t result;
    sMotor_Fun motor_fun;
    float temp;

    if (pInBuff[4] == PROTOCOL_DEVICE_ID_CTRL) { /* 回声现象 */
        return;
    }

    protocol_Temp_Upload_Comm_Set(eComm_Out, 1);      /* 通讯接收成功 使能本串口温度上送 */
    if (pInBuff[5] == eProtocolRespPack_Client_ACK) { /* 收到对方回应帧 */
        comm_Out_Send_ACK_Give(pInBuff[6]);           /* 通知串口发送任务 回应包收到 */
        return;                                       /* 直接返回 */
    }

    if (pInBuff[4] == PROTOCOL_DEVICE_ID_SAMP) {             /* ID为数据板的数据包 直接透传 调试用 */
        pInBuff[4] = PROTOCOL_DEVICE_ID_CTRL;                /* 修正装置ID */
        pInBuff[length - 1] = CRC8(pInBuff + 4, length - 5); /* 重新校正CRC */
        comm_Data_SendTask_QueueEmitCover(pInBuff, length);  /* 提交到采集板发送任务 */
        return;
    }

    protocol_Parse_AnswerACK(eComm_Out, pInBuff[3]);   /* 发送回应包 */
    switch (pInBuff[5]) {                              /* 进一步处理 功能码 */
        case eProtocolEmitPack_Client_CMD_Debug_Motor: /* 电机调试 */
            switch (pInBuff[6]) {                      /* 电机索引 */
                case 0:                                /* 扫码电机 */
                    if (length == 13) {                /* 驱动层调试 */
                        m_l6470_Index_Switch(eM_L6470_Index_0, portMAX_DELAY);
                        step = *((uint32_t *)(&pInBuff[8]));
                        if (pInBuff[7] == 0) {
                            dSPIN_Move(REV, step); /* 向驱动发送指令 */
                        } else {
                            dSPIN_Move(FWD, step); /* 向驱动发送指令 */
                        }
                        step = 0;
                        do {
                            vTaskDelay(100);
                        } while (dSPIN_Busy_HW() && ++step <= 50);
                        status = dSPIN_Get_Status();
                        pInBuff[0] = 0;
                        memcpy(pInBuff + 1, (uint8_t *)(&status), 2);
                        status = barcode_Motor_Read_Position();
                        memcpy(pInBuff + 3, (uint8_t *)(&status), 4);
                        comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Motor, pInBuff, 7);
                        m_l6470_release();
                    } else if (length == 10) {                       /* 应用层调试 */
                        barcode_Scan_Bantch(pInBuff[7], pInBuff[8]); /* 位置掩码 扫码使能掩码 */
                    } else if (length == 9) {
                        switch (pInBuff[7]) {
                            case 0xFE:
                                gMotorPRessureStopBits_Set(eMotor_Fun_PRE_BARCODE, 0);
                                motor_fun.fun_type = eMotor_Fun_PRE_BARCODE;
                                motor_Emit(&motor_fun, 3000);
                                break;
                            case 0xFF:
                                gMotorPRessureStopBits_Set(eMotor_Fun_PRE_BARCODE, 1);
                                break;
                        }
                    }
                    break;
                case 1:                 /* 托盘电机 */
                    if (length == 13) { /* 驱动层调试 */
                        m_l6470_Index_Switch(eM_L6470_Index_1, portMAX_DELAY);
                        step = *((uint32_t *)(&pInBuff[8]));
                        if (pInBuff[7] == 0) {
                            dSPIN_Move(REV, step); /* 向驱动发送指令 */
                        } else {
                            dSPIN_Move(FWD, step); /* 向驱动发送指令 */
                        }
                        step = 0;
                        do {
                            vTaskDelay(100);
                        } while (dSPIN_Busy_HW() && ++step <= 50);
                        status = dSPIN_Get_Status();
                        pInBuff[0] = 1;
                        memcpy(pInBuff + 1, (uint8_t *)(&status), 2);
                        status = tray_Motor_Read_Position();
                        memcpy(pInBuff + 3, (uint8_t *)(&status), 4);
                        comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Motor, pInBuff, 7);
                        m_l6470_release();
                    } else if (length == 9) { /* 应用层调试 */
                        switch ((pInBuff[7])) {
                            case 0:
                                tray_Move_By_Index(eTrayIndex_0, 2000);
                                break;
                            case 1:
                                tray_Move_By_Index(eTrayIndex_1, 2000);
                                break;
                            case 2:
                                tray_Move_By_Index(eTrayIndex_2, 2000);
                                break;
                            case 0xFE:
                                gMotorPRessureStopBits_Set(eMotor_Fun_PRE_TRAY, 0);
                                motor_fun.fun_type = eMotor_Fun_PRE_TRAY;
                                motor_Emit(&motor_fun, 3000);
                                break;
                            case 0xFF:
                                gMotorPRessureStopBits_Set(eMotor_Fun_PRE_TRAY, 1);
                                break;
                            default:
                                break;
                        }
                    }
                    break;
                case 2: /* 上加热体电机 */
                    if (length == 9) {
                        switch (pInBuff[7]) {
                            case 0:
                                heat_Motor_Up();
                                break;
                            case 1:
                                heat_Motor_Down();
                                break;
                            case 0xFE:
                                gMotorPRessureStopBits_Set(eMotor_Fun_PRE_HEATER, 0);
                                motor_fun.fun_type = eMotor_Fun_PRE_HEATER;
                                motor_Emit(&motor_fun, 3000);
                                break;
                            case 0xFF:
                                gMotorPRessureStopBits_Set(eMotor_Fun_PRE_HEATER, 1);
                                break;
                            default:
                                break;
                        }
                    }
                    break;
                case 3: /* 白板电机 */
                    if (length == 9) {
                        switch (pInBuff[7]) {
                            case 0:
                                white_Motor_PD();
                                break;
                            case 1:
                                white_Motor_WH();
                                break;
                            case 0xFE:
                                gMotorPRessureStopBits_Set(eMotor_Fun_PRE_WHITE, 0);
                                motor_fun.fun_type = eMotor_Fun_PRE_WHITE;
                                motor_Emit(&motor_fun, 3000);
                                break;
                            case 0xFF:
                                gMotorPRessureStopBits_Set(eMotor_Fun_PRE_WHITE, 1);
                                break;
                            default:
                                break;
                        }
                    }
                    break;
                case 4:
                    if (length == 9) {
                        switch (pInBuff[7]) {
                            case 0xFE:
                                gMotorPRessureStopBits_Set(eMotor_Fun_PRE_ALL, 0);
                                motor_fun.fun_type = eMotor_Fun_PRE_ALL;
                                motor_Emit(&motor_fun, 3000);
                                break;
                            case 0xFF:
                                gMotorPRessureStopBits_Set(eMotor_Fun_PRE_ALL, 1);
                                break;
                            default:
                                break;
                        }
                    }
                    break;
                case 0xFF:
                    gMotorPRessureStopBits_Clear();
                    break;
                default:
                    break;
            }
            break;
        case eProtocolEmitPack_Client_CMD_Debug_Scan:
            if (length == 7) {
                barcode_Read_From_Serial(&result, pInBuff, 100, 2000);
                if (result > 0) {
                    comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Scan, pInBuff, result);
                }
            } else if (length == 8) {
                if (gComm_Data_Correct_Flag_Check()) {          /* 处于定标状态 */
                    motor_Sample_Info(eMotorNotifyValue_CRRCT); /* 通知电机任务定标进仓 */
                } else {                                        /* 未处于定标状态 */
                    motor_fun.fun_type = eMotor_Fun_Correct;    /* 启动定标 */
                    if (motor_Emit(&motor_fun, 3000) == 0) {    /* 提交成功 */
                        gComm_Data_Correct_Flag_Mark();         /* 标记进入定标状态 */
                    }
                }
            } else if (length == 9) {
                barcode_Test(pInBuff[6] + pInBuff[7] * 256);
            } else if (length == 73) {
                barcode_Scan_Decode_Correct_Info(pInBuff + 6, length - 7);
            } else if (length == 26) {
                stroge_Conf_CC_O_Data(pInBuff + 6);
            } else {
                stroge_Conf_CC_O_Data_From_B3(pInBuff + 6); /* 修改测量点 */
            }
            break;
        case eProtocolEmitPack_Client_CMD_Debug_Heater:
            switch (length) {
                case 7:  /* 无参数 读取加热使能状态*/
                default: /* 兜底 */
                    pInBuff[0] = 0;
                    if (heater_BTM_Output_Is_Live()) { /* 下加热体存货状态 */
                        pInBuff[0] |= (1 << 0);
                    }
                    if (heater_TOP_Output_Is_Live()) { /* 上加热体存活状态 */
                        pInBuff[0] |= (1 << 1);
                    }
                    comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Heater, pInBuff, 1);
                    break;
                case 8:                            /* 一个参数 配置加热使能状态 */
                    if (pInBuff[6] & (1 << 0)) {   /* 下加热体 */
                        heater_BTM_Output_Start(); /* 下加热体使能 */
                    } else {
                        heater_BTM_Output_Stop(); /* 下加热体失能 */
                    }
                    if (pInBuff[6] & (1 << 1)) {   /* 上加热体 */
                        heater_TOP_Output_Start(); /* 上加热体使能 */
                    } else {
                        heater_TOP_Output_Stop(); /* 上加热体失能 */
                    }
                    break;
                case 10:                   /* 3个参数 读取PID参数 */
                    if (pInBuff[6] == 0) { /* 下加热体 */
                        pInBuff[0] = pInBuff[6];
                        pInBuff[1] = pInBuff[7];
                        pInBuff[2] = pInBuff[8];
                        for (result = 0; result < pInBuff[2]; ++result) {
                            temp = heater_BTM_Conf_Get(pInBuff[7] + result);
                            memcpy(pInBuff + 3 + 4 * result, (uint8_t *)(&temp), 4);
                        }
                        comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Heater, pInBuff, 3 + 4 * pInBuff[2]);
                    } else { /* 上加热体 */
                        pInBuff[0] = pInBuff[6];
                        pInBuff[1] = pInBuff[7];
                        pInBuff[2] = pInBuff[8];
                        for (result = 0; result < pInBuff[2]; ++result) {
                            temp = heater_TOP_Conf_Get(pInBuff[7] + result);
                            memcpy(pInBuff + 3 + 4 * result, (uint8_t *)(&temp), 4);
                        }
                        comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Heater, pInBuff, 3 + 4 * pInBuff[2]);
                    }
                    break;
                case 26:                   /* 26个参数 修改PID参数 */
                    if (pInBuff[6] == 0) { /* 下加热体 */
                        for (result = 0; result < pInBuff[8]; ++result) {
                            temp = *(float *)(pInBuff + 9 + 4 * result);
                            heater_BTM_Conf_Set(pInBuff[7] + result, temp);
                        }
                    } else { /* 上加热体 */
                        for (result = 0; result < pInBuff[8]; ++result) {
                            temp = *(float *)(pInBuff + 9 + 4 * result);
                            heater_TOP_Conf_Set(pInBuff[7] + result, temp);
                        }
                    }
                    break;
            }
            break;
        case eProtocolEmitPack_Client_CMD_Debug_Flag: /* 调试开关 */
            if (length == 7) {
                pInBuff[0] = protocol_Debug_Get();
                comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Flag, pInBuff, 1);
            } else if (length == 9) {
                if (pInBuff[7] == 0) {
                    protocol_Debug_Clear(pInBuff[6]);
                } else {
                    protocol_Debug_Mark(pInBuff[6]);
                }
            } else {
                // comm_Data_Sample_Data_Correct((pInBuff[6] < 1 || pInBuff[6] > 6) ? (1) : (pInBuff[6]), pInBuff + 1, pInBuff);
                // comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Flag, pInBuff + 1, pInBuff[0]);
                result = comm_Data_Sample_Data_Fetch(pInBuff[6], pInBuff + 1, pInBuff);
                comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Flag, pInBuff + 1, pInBuff[0]);
            }
            break;
        case eProtocolEmitPack_Client_CMD_Debug_Beep: /* 蜂鸣器控制 */
            if (length != 14) {
                beep_Start();
            } else {
                beep_Start_With_Conf(pInBuff[6] % 7, (pInBuff[7] << 8) + pInBuff[8], (pInBuff[9] << 8) + pInBuff[10], (pInBuff[11] << 8) + pInBuff[12]);
            }
            break;
        case eProtocolEmitPack_Client_CMD_Debug_Flash_Read: /* SPI Flash 读测试 */
            result = storgeReadConfInfo((pInBuff[6] << 16) + (pInBuff[7] << 8) + pInBuff[8], (pInBuff[9] << 16) + (pInBuff[10] << 8) + pInBuff[11], 200);
            if (result == 0) {
                storgeTaskNotification(eStorgeNotifyConf_Read_Flash, eComm_Out); /* 通知存储任务 */
            }
            break;
        case eProtocolEmitPack_Client_CMD_Debug_Flash_Write: /* SPI Flash 写测试 */
            result = storgeWriteConfInfo((pInBuff[6] << 16) + (pInBuff[7] << 8) + pInBuff[8], &pInBuff[12],
                                         (pInBuff[9] << 16) + (pInBuff[10] << 8) + pInBuff[11], 200);
            if (result == 0) {
                storgeTaskNotification(eStorgeNotifyConf_Write_Flash, eComm_Out); /* 通知存储任务 */
            }
            break;
        case eProtocolEmitPack_Client_CMD_Debug_EEPROM_Read: /* EEPROM 读测试 */
            result = storgeReadConfInfo((pInBuff[6] << 16) + (pInBuff[7] << 8) + pInBuff[8], (pInBuff[9] << 16) + (pInBuff[10] << 8) + pInBuff[11], 200);
            if (result == 0) {
                storgeTaskNotification(eStorgeNotifyConf_Read_ID_Card, eComm_Out); /* 通知存储任务 */
            }
            break;
        case eProtocolEmitPack_Client_CMD_Debug_EEPROM_Write: /* EEPROM 写测试 */
            result = storgeWriteConfInfo((pInBuff[6] << 16) + (pInBuff[7] << 8) + pInBuff[8], &pInBuff[12],
                                         (pInBuff[9] << 16) + (pInBuff[10] << 8) + pInBuff[11], 200);
            if (result == 0) {
                storgeTaskNotification(eStorgeNotifyConf_Write_ID_Card, eComm_Out); /* 通知存储任务 */
            }
            break;
        case eProtocolEmitPack_Client_CMD_Debug_Motor_Fun: /* 电机功能 */
            motor_fun.fun_type = (eMotor_Fun)pInBuff[6];   /* 开始测试 */
            pInBuff[0] = motor_Emit(&motor_fun, 0);        /* 提交到电机队列 */
            comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Motor_Fun, pInBuff, 1);
            break;
        case eProtocolEmitPack_Client_CMD_Debug_Self_Check: /* 自检测试 */
            if (length == 7) {
                protocol_Self_Check_Temp_TOP(pInBuff);
                protocol_Self_Check_Temp_BTM(pInBuff);
                protocol_Self_Check_Temp_ENV(pInBuff);
                motor_fun.fun_type = eMotor_Fun_Self_Check;                    /* 整体自检测试 */
                motor_Emit(&motor_fun, 0);                                     /* 提交到电机队列 */
                storgeTaskNotification(eStorgeNotifyConf_Test_All, eComm_Out); /* 通知存储任务 */
            } else if (length == 8) {                                          /* 单向测试结果 */
                switch (pInBuff[6]) {
                    case 1: /* 上加热体温度结果 */
                        protocol_Self_Check_Temp_TOP(pInBuff);
                        break;
                    case 2: /* 下加热体温度结果 */
                        protocol_Self_Check_Temp_BTM(pInBuff);
                        break;
                    case 3: /* 环境温度结果 */
                        protocol_Self_Check_Temp_ENV(pInBuff);
                        break;
                    case 4:                                                              /* 外部Flash */
                        storgeTaskNotification(eStorgeNotifyConf_Test_Flash, eComm_Out); /* 通知存储任务 */
                        break;
                    case 5:                                                                /* ID Code 卡 */
                        storgeTaskNotification(eStorgeNotifyConf_Test_ID_Card, eComm_Out); /* 通知存储任务 */
                        break;
                    default:
                        motor_fun.fun_type = eMotor_Fun_Self_Check_Motor_White - 6 + pInBuff[6]; /* 整体自检测试 单项 */
                        motor_Emit(&motor_fun, 0);                                               /* 提交到电机队列 */
                        break;
                }
            }
            break;
        case eProtocolEmitPack_Client_CMD_Debug_System: /* 系统控制 */
            if (length == 7) {
                HAL_NVIC_SystemReset(); /* 重新启动 */
            } else if (length == 8) {
                gComm_Data_Sample_Period_Set(pInBuff[6] % 20);
            } else {
                error_Emit(eError_Comm_Out_Param_Error);
            }
            break;
        case eProtocolEmitPack_Client_CMD_Debug_Params:         /* 参数设置 */
            if (length == 9) {                                  /* 保存参数 */
                status = (pInBuff[6] << 0) + (pInBuff[7] << 8); /* 起始索引 */
                if (status == 0xFFFF) {
                    storgeTaskNotification(eStorgeNotifyConf_Dump_Params, eComm_Out);
                }
            } else if (length == 11) {                                                                                          /* 读取参数 */
                result = storgeReadConfInfo((pInBuff[6] << 0) + (pInBuff[7] << 8), (pInBuff[8] << 0) + (pInBuff[9] << 8), 200); /* 配置 */
                if (result == 0) {
                    storgeTaskNotification(eStorgeNotifyConf_Read_Parmas, eComm_Out); /* 通知存储任务 */
                }
            } else if (length > 11 && ((length - 11) % 4 == 0)) { /* 写入参数 */
                result = storgeWriteConfInfo((pInBuff[6] << 0) + (pInBuff[7] << 8), &pInBuff[10], 4 * ((pInBuff[8] << 0) + (pInBuff[9] << 8)), 200); /* 配置 */
                if (result == 0) {
                    storgeTaskNotification(eStorgeNotifyConf_Write_Parmas, eComm_Out); /* 通知存储任务 */
                }
            } else {
                error_Emit(eError_Comm_Out_Param_Error);
            }
            break;
        case eProtocolEmitPack_Client_CMD_Debug_BL:             /* 升级Bootloader */
            if ((pInBuff[10] << 0) + (pInBuff[11] << 8) == 0) { /* 数据长度为空 尾包 */
                result = Innate_Flash_Dump((pInBuff[6] << 0) + (pInBuff[7] << 8),
                                           (pInBuff[12] << 0) + (pInBuff[13] << 8) + (pInBuff[14] << 16) + (pInBuff[15] << 24));
                pInBuff[0] = result;
                comm_Out_SendTask_QueueEmitWithBuildCover(0xDE, pInBuff, 1);
            } else {                                              /* 数据长度不为空 非尾包 */
                if ((pInBuff[8] << 0) + (pInBuff[9] << 8) == 0) { /* 起始包 */
                    result = Innate_Flash_Erase_Temp();           /* 擦除Flash */
                    if (result > 0) {                             /* 擦除失败 */
                        HAL_FLASH_Lock();                         /* 回锁 */
                        pInBuff[0] = result;
                        comm_Out_SendTask_QueueEmitWithBuildCover(0xDE, pInBuff, 1);
                        break;
                    }
                }
                result =
                    Innate_Flash_Write(INNATE_FLASH_ADDR_TEMP + (pInBuff[8] << 0) + (pInBuff[9] << 8), pInBuff + 12, (pInBuff[10] << 0) + (pInBuff[11] << 8));
                pInBuff[0] = result;
                comm_Out_SendTask_QueueEmitWithBuildCover(0xDE, pInBuff, 1);
            }
            break;
        case eProtocolEmitPack_Client_CMD_Debug_Version:
            memcpy(pInBuff, (uint8_t *)(UID_BASE), 12);
            memcpy(pInBuff + 12, (uint8_t *)__TIME__, strlen(__TIME__));
            memcpy(pInBuff + 12 + strlen(__TIME__), (uint8_t *)__DATE__, strlen(__DATE__));
            comm_Out_SendTask_QueueEmitWithBuildCover(0xDF, pInBuff, 12 + strlen(__TIME__) + strlen(__DATE__));
            break;
        case eProtocolEmitPack_Client_CMD_START:          /* 开始测量帧 0x01 */
            gComm_Data_Sample_Max_Point_Clear();          /* 清除最大点数 */
            protocol_Temp_Upload_Pause();                 /* 暂停温度上送 */
            motor_fun.fun_type = eMotor_Fun_Sample_Start; /* 开始测试 */
            motor_Emit(&motor_fun, 0);                    /* 提交到电机队列 */
            break;
        case eProtocolEmitPack_Client_CMD_ABRUPT:    /* 仪器测量取消命令帧 0x02 */
            barcode_Interrupt_Flag_Mark();           /* 标记打断扫码 */
            comm_Data_Sample_Force_Stop();           /* 强行停止采样定时器 */
            motor_Sample_Info(eMotorNotifyValue_BR); /* 提交打断信息 */
            break;
        case eProtocolEmitPack_Client_CMD_CONFIG:    /* 测试项信息帧 0x03 */
            comm_Data_Sample_Send_Conf(&pInBuff[6]); /* 发送测试配置 */
            break;
        case eProtocolEmitPack_Client_CMD_FORWARD: /* 打开托盘帧 0x04 */
            motor_fun.fun_type = eMotor_Fun_Out;   /* 配置电机动作套餐类型 出仓 */
            motor_Emit(&motor_fun, 0);             /* 交给电机任务 出仓 */
            break;
        case eProtocolEmitPack_Client_CMD_REVERSE: /* 关闭托盘命令帧 0x05 */
            motor_fun.fun_type = eMotor_Fun_In;    /* 配置电机动作套餐类型 进仓 */
            motor_Emit(&motor_fun, 0);             /* 交给电机任务 进仓 */
            break;
        case eProtocolEmitPack_Client_CMD_READ_ID:     /* ID卡读取命令帧 0x06 */
            result = storgeReadConfInfo(0, 4096, 200); /* 暂无定义 按最大读取 */
            if (result == 0) {
                storgeTaskNotification(eStorgeNotifyConf_Read_ID_Card, eComm_Out); /* 通知存储任务 */
            }
            break;
        case eProtocolEmitPack_Client_CMD_STATUS:                                                /* 状态信息查询帧 (首帧) 0x07 */
            temp = temp_Get_Temp_Data_BTM();                                                     /* 下加热体温度 */
            pInBuff[0] = ((uint16_t)(temp * 100)) & 0xFF;                                        /* 小端模式 低8位 */
            pInBuff[1] = ((uint16_t)(temp * 100)) >> 8;                                          /* 小端模式 高8位 */
            temp = temp_Get_Temp_Data_TOP();                                                     /* 上加热体温度 */
            pInBuff[2] = ((uint16_t)(temp * 100)) & 0xFF;                                        /* 小端模式 低8位 */
            pInBuff[3] = ((uint16_t)(temp * 100)) >> 8;                                          /* 小端模式 高8位 */
            comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolRespPack_Client_TMP, pInBuff, 4); /* 温度信息 */

            protocol_Get_Version(pInBuff);
            comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolRespPack_Client_VER, pInBuff, 4); /* 软件版本信息 */

            if (tray_Motor_Get_Status_Position() == 0) { /* 托盘状态信息 */
                pInBuff[0] = 1;                          /* 托盘处于测试位置 原点 */
            } else if (tray_Motor_Get_Status_Position() >= (eTrayIndex_2 / 4 - 50) && tray_Motor_Get_Status_Position() <= (eTrayIndex_2 / 4 + 50)) {
                pInBuff[0] = 2; /* 托盘处于出仓位置 误差范围 +-50步 */
            } else {
                pInBuff[0] = 0;
            }
            comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolRespPack_Client_DISH, pInBuff, 1); /* 托盘状态信息 */
            break;
        case eProtocolEmitPack_Client_CMD_TEST:         /* 工装测试配置帧 0x08 */
            comm_Data_Sample_Send_Conf_TV(&pInBuff[6]); /* 保存测试配置 */
            break;
        case eProtocolEmitPack_Client_CMD_UPGRADE: /* 下位机升级命令帧 0x0F */
            if (spi_FlashWriteAndCheck_Word(0x0000, 0x87654321) == 0) {
                HAL_NVIC_SystemReset(); /* 重新启动 */
            } else {
                error_Emit(eError_Out_Flash_Write_Failed);
            }
            break;
        default:
            error_Emit(eError_Comm_Out_Unknow_CMD);
            break;
    }
    return;
}

/**
 * @brief  上位机解析协议
 * @param  pInBuff 入站指针
 * @param  length 入站长度
 * @param  pOutBuff 出站指针
 * @retval 解析数据包结果
 */
void protocol_Parse_Main(uint8_t * pInBuff, uint8_t length)
{
    uint8_t result;
    sMotor_Fun motor_fun;
    float temp;

    if (pInBuff[4] == PROTOCOL_DEVICE_ID_CTRL) { /* 回声现象 */
        return;
    }

    gComm_Main_Connected_Set_Enable();                /* 标记通信成功 */
    if (pInBuff[5] == eProtocolRespPack_Client_ACK) { /* 收到对方回应帧 */
        comm_Main_Send_ACK_Give(pInBuff[6]);          /* 通知串口发送任务 回应包收到 */
        return;                                       /* 直接返回 */
    }

    protocol_Temp_Upload_Comm_Set(eComm_Main, 1); /* 通讯接收成功 使能本串口温度上送 */

    protocol_Parse_AnswerACK(eComm_Main, pInBuff[3]);     /* 发送回应包 */
    switch (pInBuff[5]) {                                 /* 进一步处理 功能码 */
        case eProtocolEmitPack_Client_CMD_START:          /* 开始测量帧 0x01 */
            gComm_Data_Sample_Max_Point_Clear();          /* 清除最大点数 */
            protocol_Temp_Upload_Pause();                 /* 暂停温度上送 */
            motor_fun.fun_type = eMotor_Fun_Sample_Start; /* 开始测试 */
            motor_Emit(&motor_fun, 0);                    /* 提交到电机队列 */
            break;
        case eProtocolEmitPack_Client_CMD_ABRUPT:    /* 仪器测量取消命令帧 0x02 */
            comm_Data_Sample_Force_Stop();           /* 强行停止采样定时器 */
            motor_Sample_Info(eMotorNotifyValue_BR); /* 提交打断信息 */
            break;
        case eProtocolEmitPack_Client_CMD_CONFIG:    /* 测试项信息帧 0x03 */
            comm_Data_Sample_Send_Conf(&pInBuff[6]); /* 发送测试配置 */
            break;
        case eProtocolEmitPack_Client_CMD_FORWARD: /* 打开托盘帧 0x04 */
            motor_fun.fun_type = eMotor_Fun_Out;   /* 配置电机动作套餐类型 出仓 */
            motor_Emit(&motor_fun, 0);             /* 交给电机任务 出仓 */
            break;
        case eProtocolEmitPack_Client_CMD_REVERSE: /* 关闭托盘命令帧 0x05 */
            motor_fun.fun_type = eMotor_Fun_In;    /* 配置电机动作套餐类型 进仓 */
            motor_Emit(&motor_fun, 0);             /* 交给电机任务 进仓 */
            break;
        case eProtocolEmitPack_Client_CMD_READ_ID:     /* ID卡读取命令帧 0x06 */
            result = storgeReadConfInfo(0, 4096, 200); /* 暂无定义 按最大读取 */
            if (result == 0) {
                storgeTaskNotification(eStorgeNotifyConf_Read_ID_Card, eComm_Main); /* 通知存储任务 */
            }
            break;
        case eProtocolEmitPack_Client_CMD_STATUS:                                                 /* 状态信息查询帧 (首帧) */
            temp = temp_Get_Temp_Data_BTM();                                                      /* 下加热体温度 */
            pInBuff[0] = ((uint16_t)(temp * 100)) & 0xFF;                                         /* 小端模式 低8位 */
            pInBuff[1] = ((uint16_t)(temp * 100)) >> 8;                                           /* 小端模式 高8位 */
            temp = temp_Get_Temp_Data_TOP();                                                      /* 上加热体温度 */
            pInBuff[2] = ((uint16_t)(temp * 100)) & 0xFF;                                         /* 小端模式 低8位 */
            pInBuff[3] = ((uint16_t)(temp * 100)) >> 8;                                           /* 小端模式 高8位 */
            comm_Main_SendTask_QueueEmitWithBuildCover(eProtocolRespPack_Client_TMP, pInBuff, 4); /* 温度信息 */
            protocol_Get_Version(pInBuff);
            comm_Main_SendTask_QueueEmitWithBuildCover(eProtocolRespPack_Client_VER, pInBuff, 4); /* 软件版本信息 */

            if (tray_Motor_Get_Status_Position() == 0) { /* 托盘状态 */
                pInBuff[0] = 1;                          /* 托盘处于测试位置 原点 */
            } else if (tray_Motor_Get_Status_Position() >= (eTrayIndex_2 / 4 - 50) && tray_Motor_Get_Status_Position() <= (eTrayIndex_2 / 4 + 50)) {
                pInBuff[0] = 2; /* 托盘处于出仓位置 误差范围 +-50步 */
            } else {
                pInBuff[0] = 0;
            }
            comm_Main_SendTask_QueueEmitWithBuildCover(eProtocolRespPack_Client_DISH, pInBuff, 1); /* 托盘状态信息 */
            break;
        case eProtocolEmitPack_Client_CMD_UPGRADE: /* 下位机升级命令帧 0x0F */
            if (spi_FlashWriteAndCheck_Word(0x0000, 0x87654321) == 0) {
                HAL_NVIC_SystemReset(); /* 重新启动 */
            } else {
                error_Emit(eError_Out_Flash_Write_Failed);
            }
            break;
        default:
            error_Emit(eError_Comm_Main_Unknow_CMD);
            break;
    }
    return;
}

/**
 * @brief  采样板解析协议
 * @param  pInBuff 入站指针
 * @param  length 入站长度
 * @param  pOutBuff 出站指针
 * @retval 解析数据包结果
 */
void protocol_Parse_Data(uint8_t * pInBuff, uint8_t length)
{
    static uint8_t last_ack = 0;
    uint8_t data_length;

    if (pInBuff[4] == PROTOCOL_DEVICE_ID_CTRL) { /* 回声现象 */
        return;
    }

    if (pInBuff[5] == eProtocolRespPack_Client_ACK) { /* 收到对方回应帧 */
        comm_Data_Send_ACK_Give(pInBuff[6]);          /* 通知串口发送任务 回应包收到 */
        return;                                       /* 直接返回 */
    }

    protocol_Parse_AnswerACK(eComm_Data, pInBuff[3]); /* 发送回应包 */
    if (last_ack == pInBuff[3]) {                     /* 收到与上一帧号相同帧 */
        return;                                       /* 不做处理 */
    }
    last_ack = pInBuff[3]; /* 记录上一帧号 */

    switch (pInBuff[5]) {                                                                                                   /* 进一步处理 功能码 */
        case eComm_Data_Inbound_CMD_DATA:                                                                                   /* 采集数据帧 */
            if (gComm_Data_Correct_Flag_Check()) {                                                                          /* 处于定标状态 */
                stroge_Conf_CC_O_Data_From_B3(pInBuff + 6);                                                                 /* 修改测量点 */
                comm_Out_SendTask_QueueEmitWithBuild(eProtocolRespPack_Client_SAMP_DATA, &pInBuff[6], length - 7, 20);      /* 转发至外串口 */
            } else {                                                                                                        /* 未出于定标状态 */
                if (protocol_Debug_SampleRawData()) {                                                                       /* 选择原始数据 */
                    comm_Main_SendTask_QueueEmitWithBuild(eProtocolRespPack_Client_SAMP_DATA, &pInBuff[6], length - 7, 20); /* 构造数据包 */
                    comm_Out_SendTask_QueueEmitWithModify(pInBuff + 6, length, 0);                                          /* 修改帧号ID后转发 */
                } else {                                                                                                    /* 经过校正映射 */
                    comm_Data_Sample_Data_Commit(pInBuff[7], pInBuff, length);                                              /* 采样数据记录 */
                    comm_Data_Sample_Data_Correct(pInBuff[7], pInBuff, &data_length);                                       /* 投影校正 */
                    comm_Main_SendTask_QueueEmitWithBuild(eProtocolRespPack_Client_SAMP_DATA, pInBuff, data_length, 20);    /* 构造数据包 */
                    comm_Out_SendTask_QueueEmitWithModify(pInBuff, data_length + 7, 0);                                     /* 修改帧号ID后转发 */
                }
            }
            break;
        case eComm_Data_Inbound_CMD_OVER:                /* 采集数据完成帧 */
            if (comm_Data_Stary_Test_Is_Running()) {     /* 判断是否处于杂散光测试中 */
                motor_Sample_Info(eMotorNotifyValue_SP); /* 通知电机任务杂散光测试完成 */
            } else {                                     /* 非杂散光测试 */
                motor_Sample_Info(eMotorNotifyValue_TG); /* 通知电机任务采样完成 */
            }
            break;
        case eComm_Data_Inbound_CMD_ERROR: /* 采集板错误信息帧 */
            comm_Main_SendTask_QueueEmitWithBuildCover(eProtocolRespPack_Client_ERR, &pInBuff[6], 2);
            break;
        default:
            error_Emit(eError_Comm_Data_Unknow_CMD);
            break;
    }
    return;
}

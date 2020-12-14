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
#define SELF_CHECK_BTM_MAX 38
#define SELF_CHECK_BTM_MIN 36

#define SELF_CHECK_TOP_MAX 38
#define SELF_CHECK_TOP_MIN 36

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

static uint8_t gProtocol_Debug_Flag = eProtocol_Debug_ErrorReport | eProtocol_Debug_SampleBarcode | eProtocol_Debug_SampleMotorTray;

static uint8_t gProtocol_Out_ACK_Pack_Buffer[8];
static uint8_t gProtocol_Main_ACK_Pack_Buffer[8];
static uint8_t gProtocol_Data_ACK_Pack_Buffer[8];

/* Private function prototypes -----------------------------------------------*/
static uint8_t protocol_Is_Debug(eProtocol_Debug_Item item);
static void protocol_Parse_Out_Fun_ISR(uint8_t * pInBuff, uint16_t length);
static void protocol_Parse_Main_Fun_ISR(uint8_t * pInBuff, uint16_t length);
static void protocol_Parse_Data_Fun_ISR(uint8_t * pInBuff, uint16_t length);

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
 * @brief  调试标志 老化测试
 * @param  None
 * @retval 1 使能 0 失能
 */
uint8_t protocol_Debug_AgingLoop(void)
{
    return protocol_Is_Debug(eProtocol_Debug_AgingLoop);
}

/**
 * @brief  调试标志 工装温度
 * @param  None
 * @retval 1 使能 0 失能
 */
uint8_t protocol_Debug_Factory_Temp(void)
{
    return protocol_Is_Debug(eProtocol_Debug_Factory_Temp);
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

    if (temp_btm < 29) {                                                             /* 温度值低于29 */
        xTick_btm_Keep_Hight = now;                                                  /* 温度过高计数清零 */
        if (xTick_btm_Keep_Hight - xTick_btm_Keep_low > 600 * pdMS_TO_TICKS(1000)) { /* 过低持续次数大于 10 Min */
            error_Emit(eError_Temperature_Btm_TooLow);                               /* 报错 */
            xTick_btm_Keep_low = xTick_btm_Keep_Hight;                               /* 重复报错间隔 */
        }
    } else if (temp_btm > 45) {              /* 温度值高于45 */
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
    } else {                                       /* 温度值在控制范围内 */
        xTick_btm_Keep_low = now;                  /* 温度过低计数清零 */
        xTick_btm_Keep_Hight = xTick_btm_Keep_low; /* 温度过高计数清零 */
        xTick_btm_Nai = now;                       /* 温度无效计数清零 */
    }

    if (temp_top < 29) {                                                             /* 温度值低于29 */
        xTick_top_Keep_Hight = now;                                                  /* 温度过高计数清零 */
        if (xTick_top_Keep_Hight - xTick_top_Keep_low > 600 * pdMS_TO_TICKS(1000)) { /* 过低持续时间 10 Min */
            error_Emit(eError_Temperature_Top_TooLow);                               /* 报错 */
            xTick_top_Keep_low = xTick_top_Keep_Hight;                               /* 重复报错间隔 */
        }
    } else if (temp_top > 45) {              /* 温度值高于45 */
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
    } else {                        /* 温度值在控制范围内 */
        xTick_top_Keep_low = now;   /* 温度过低计数清零 */
        xTick_top_Keep_Hight = now; /* 温度过高计数清零 */
        xTick_top_Nai = now;        /* 温度无效计数清零 */
    }
}

/**
 * @brief  温度主动上送处理 主板串口
 * @param  temp_btm 下加热体温度
 * @param  temp_top 上加热体温度
 * @param  temp_env 环境温度
 * @retval None
 */
void protocol_Temp_Upload_Main_Deal(float temp_btm, float temp_top, float temp_env)
{
    uint8_t buffer[10], length;

    // if (protocol_Temp_Upload_Comm_Get(eComm_Main) == 0) { /* 无需进行串口发送 */
    //     return;
    // }
    if (comm_Main_SendTask_Queue_GetWaiting() > 1) { /* 发送队列内有其他数据包 */
        return;
    }

    if (temp_btm != TEMP_INVALID_DATA || temp_top != TEMP_INVALID_DATA) {              /* 温度值都不是无效值 */
        buffer[0] = ((uint16_t)(temp_btm * 100 + 0.5)) & 0xFF;                         /* 小端模式 低8位 */
        buffer[1] = ((uint16_t)(temp_btm * 100 + 0.5)) >> 8;                           /* 小端模式 高8位 */
        buffer[2] = ((uint16_t)(temp_top * 100 + 0.5)) & 0xFF;                         /* 小端模式 低8位 */
        buffer[3] = ((uint16_t)(temp_top * 100 + 0.5)) >> 8;                           /* 小端模式 高8位 */
        length = buildPackOrigin(eComm_Main, eProtocolRespPack_Client_TMP, buffer, 4); /* 构造数据包 */
        comm_Main_SendTask_QueueEmitCover(buffer, length);                             /* 提交到发送队列 */
    }

    if (temp_env != TEMP_INVALID_DATA) {                                                   /* 温度值不是无效值 */
        buffer[0] = ((uint16_t)(temp_env * 100 + 0.5)) & 0xFF;                             /* 小端模式 低8位 */
        buffer[1] = ((uint16_t)(temp_env * 100 + 0.5)) >> 8;                               /* 小端模式 高8位 */
        length = buildPackOrigin(eComm_Main, eProtocolRespPack_Client_ENV_TMP, buffer, 2); /* 构造数据包 */
        comm_Main_SendTask_QueueEmitCover(buffer, length);                                 /* 提交到发送队列 */
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
    uint8_t buffer[64], length, i;
    float temperature;
    uint16_t adc_raw;

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
        if (protocol_Debug_Temperature()) { /* 使能温度调试 */
            for (i = eTemp_NTC_Index_0; i <= eTemp_NTC_Index_8; ++i) {
                temperature = temp_Get_Temp_Data(i);
                memcpy(buffer + 4 * i, &temperature, 4);
            }
            for (i = eTemp_NTC_Index_0; i <= eTemp_NTC_Index_8; ++i) {
                adc_raw = gTempADC_Results_Get_By_Index(i);
                memcpy(buffer + 36 + 2 * i, (uint8_t *)(&adc_raw), 2);
            }
            length = buildPackOrigin(eComm_Out, eProtocolRespPack_Client_Debug_Temp, buffer, 54); /* 构造数据包  */
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
    float temp_top, temp_btm, temp_env;
    TickType_t now;
    static TickType_t xTick_Main = 0, xTick_Out = 0;

    temp_btm = temp_Get_Temp_Data_BTM(); /* 下加热体温度 */
    temp_top = temp_Get_Temp_Data_TOP(); /* 上加热体温度 */
    temp_env = temp_Get_Temp_Data_ENV(); /* 环境温度 */

    now = xTaskGetTickCount();
    if (protocol_Temp_Upload_Is_Suspend() == 0) { /* 暂停上送标志 */
        protocol_Temp_Upload_Error_Deal(now, temp_btm, temp_top);
        if (now - xTick_Main > 5 * pdMS_TO_TICKS(1000)) {
            xTick_Main = now;
            protocol_Temp_Upload_Main_Deal(temp_btm, temp_top, temp_env);
        }
    }

    if (protocol_Debug_Factory_Temp()) {
        if (now - xTick_Out > 2 * pdMS_TO_TICKS(1000)) {
            xTick_Out = now;
            protocol_Self_Check_Temp_ALL();
        }
    } else {
        if (protocol_Debug_Temperature()) {
            if (now - xTick_Out > 0.2 * pdMS_TO_TICKS(1000)) {
                xTick_Out = now;
                protocol_Temp_Upload_Out_Deal(temp_btm, temp_top);
            }
        } else if (protocol_Temp_Upload_Is_Suspend() == 0) { /* 暂停上送标志 */
            {
                if (now - xTick_Out > 5 * pdMS_TO_TICKS(1000)) {
                    xTick_Out = now;
                    protocol_Temp_Upload_Out_Deal(temp_btm, temp_top);
                }
            }
        }
    }
}

/**
 * @brief  自检测试 温度判断
 * @param  pBuffer 数据指针
 * @retval 0 正常 1 异常 2 过高 3 过低
 **/
void protocol_Self_Check_Temp_TOP(uint8_t * pBuffer, eProtocol_COMM_Index idx)
{
    float temp;
    uint8_t result = 0, i;

    temp = temp_Get_Temp_Data_TOP(); /* 读取温度值 */
    if (temp == TEMP_INVALID_DATA) { /* 排除无效值 */
        result = 1;
    } else if (temp > SELF_CHECK_TOP_MAX) { /* 温度过高 */
        result = 2;
    } else if (temp < SELF_CHECK_TOP_MIN) { /* 温度过低 */
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
    if (idx == eComm_Out) {
        comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 26);
    } else if (idx == eComm_Main) {
        comm_Main_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 26);
    }
}

/**
 * @brief  自检测试 温度判断 中断版本
 * @param  pBuffer 数据指针
 * @retval 0 正常 1 异常 2 过高 3 过低
 **/
void protocol_Self_Check_Temp_TOP_FromISR(uint8_t * pBuffer, eProtocol_COMM_Index idx)
{
    float temp;
    uint8_t result = 0, i;

    temp = temp_Get_Temp_Data_TOP(); /* 读取温度值 */
    if (temp == TEMP_INVALID_DATA) { /* 排除无效值 */
        result = 1;
    } else if (temp > SELF_CHECK_TOP_MAX) { /* 温度过高 */
        result = 2;
    } else if (temp < SELF_CHECK_TOP_MIN) { /* 温度过低 */
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
    if (idx == eComm_Out) {
        comm_Out_SendTask_QueueEmitWithBuild_FromISR(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 26);
    } else if (idx == eComm_Main) {
        comm_Main_SendTask_QueueEmitWithBuild_FromISR(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 26);
    }
}

/**
 * @brief  自检测试 温度判断
 * @param  pBuffer 数据指针
 * @retval 0 正常 1 异常 2 过高 3 过低
 **/
void protocol_Self_Check_Temp_BTM(uint8_t * pBuffer, eProtocol_COMM_Index idx)
{
    float temp;
    uint8_t result = 0, i;

    temp = temp_Get_Temp_Data_BTM(); /* 读取温度值 */
    if (temp == TEMP_INVALID_DATA) { /* 排除无效值 */
        result = 1;
    } else if (temp > SELF_CHECK_BTM_MAX) { /* 温度过高 */
        result = 2;
    } else if (temp < SELF_CHECK_BTM_MIN) { /* 温度过低 */
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
    if (idx == eComm_Out) {
        comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 10);
    } else if (idx == eComm_Main) {
        comm_Main_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 10);
    }
}

/**
 * @brief  自检测试 温度判断
 * @param  pBuffer 数据指针
 * @retval 0 正常 1 异常 2 过高 3 过低
 **/
void protocol_Self_Check_Temp_BTM_FromISR(uint8_t * pBuffer, eProtocol_COMM_Index idx)
{
    float temp;
    uint8_t result = 0, i;

    temp = temp_Get_Temp_Data_BTM(); /* 读取温度值 */
    if (temp == TEMP_INVALID_DATA) { /* 排除无效值 */
        result = 1;
    } else if (temp > SELF_CHECK_BTM_MAX) { /* 温度过高 */
        result = 2;
    } else if (temp < SELF_CHECK_BTM_MIN) { /* 温度过低 */
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
    if (idx == eComm_Out) {
        comm_Out_SendTask_QueueEmitWithBuild_FromISR(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 10);
    } else if (idx == eComm_Main) {
        comm_Main_SendTask_QueueEmitWithBuild_FromISR(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 10);
    }
}

/**
 * @brief  自检测试 温度判断
 * @param  pBuffer 数据指针
 * @retval 0 正常 1 异常 2 过高 3 过低
 **/
void protocol_Self_Check_Temp_ENV(uint8_t * pBuffer, eProtocol_COMM_Index idx)
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
    if (idx == eComm_Out) {
        comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 6);
    } else if (idx == eComm_Main) {
        comm_Main_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 6);
    }
}

/**
 * @brief  自检测试 温度判断
 * @param  pBuffer 数据指针
 * @retval 0 正常 1 异常 2 过高 3 过低
 **/
void protocol_Self_Check_Temp_ENV_FromISR(uint8_t * pBuffer, eProtocol_COMM_Index idx)
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
    if (idx == eComm_Out) {
        comm_Out_SendTask_QueueEmitWithBuild_FromISR(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 6);
    } else if (idx == eComm_Main) {
        comm_Main_SendTask_QueueEmitWithBuild_FromISR(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 6);
    }
}

/**
 * @brief  温度主动上送处理 工装模式
 * @param  None
 * @retval None
 */
void protocol_Self_Check_Temp_ALL(void)
{
    uint8_t buffer[40];

    protocol_Self_Check_Temp_TOP(buffer, eComm_Out);
    protocol_Self_Check_Temp_BTM(buffer, eComm_Out);
    protocol_Self_Check_Temp_ENV(buffer, eComm_Out);
}

/**
 * @brief  外串口解析协议 预过滤处理
 * @param  pInBuff 入站指针
 * @param  length 入站长度
 * @param  pOutBuff 出站指针
 * @retval None
 */
uint8_t protocol_Parse_Out_ISR(uint8_t * pInBuff, uint16_t length)
{
    static uint8_t last_ack = 0;
    BaseType_t result = pdFALSE;

    if (pInBuff[4] == PROTOCOL_DEVICE_ID_CTRL) { /* 回声现象 */
        return 0;
    }
    protocol_Temp_Upload_Comm_Set(eComm_Out, 1); /* 通讯接收成功 使能本串口温度上送 */

    if (pInBuff[5] == eProtocolRespPack_Client_ACK) { /* 收到对方回应帧 */
        comm_Out_Send_ACK_Give_From_ISR(pInBuff[6]);  /* 通知串口发送任务 回应包收到 */
        return 0;                                     /* 直接返回 */
    }

    if (pInBuff[4] == PROTOCOL_DEVICE_ID_SAMP) {               /* ID为数据板的数据包 直接透传 调试用 */
        pInBuff[4] = PROTOCOL_DEVICE_ID_CTRL;                  /* 修正装置ID */
        pInBuff[length - 1] = CRC8(pInBuff + 4, length - 5);   /* 重新校正CRC */
        comm_Data_SendTask_QueueEmit_FromISR(pInBuff, length); /* 提交到采集板发送任务 */
        return 0;
    }

    if (comm_Out_DMA_TX_Enter_From_ISR() == pdPASS) {  /* 确保发送完成信号量被释放 */
        gProtocol_Out_ACK_Pack_Buffer[0] = pInBuff[3]; /* 回应ACK号 */
        result = serialSendStartIT(COMM_OUT_SERIAL_INDEX, gProtocol_Out_ACK_Pack_Buffer,
                                   buildPackOrigin(eComm_Out, eProtocolRespPack_Client_ACK, &gProtocol_Out_ACK_Pack_Buffer[0], 1));
    }
    if (result == pdFALSE) {                                 /* 中断发送失败 */
        comm_Out_SendTask_ACK_QueueEmitFromISR(&pInBuff[3]); /* 投入发送任务处理 */
    }

    if (last_ack == pInBuff[3]) { /* 收到与上一帧号相同帧 */
        return 0;                 /* 不做处理 */
    }
    last_ack = pInBuff[3]; /* 记录上一帧号 */

    protocol_Parse_Out_Fun_ISR(pInBuff, length);
    return 0;
}

/**
 * @brief  外串口解析协议 中断版本
 * @param  pInBuff 入站指针
 * @param  length 入站长度
 * @retval None
 */
static void protocol_Parse_Out_Fun_ISR(uint8_t * pInBuff, uint16_t length)
{
    uint8_t result;
    uint16_t status = 0;
    float temp;
    sMotor_Fun motor_fun;

    switch (pInBuff[5]) {                              /* 进一步处理 功能码 */
        case eProtocolEmitPack_Client_CMD_Debug_Motor: /* 电机调试 */
            switch (pInBuff[6]) {                      /* 电机索引 */
                case 0:                                /* 扫码电机 */
                    if (length == 10) {                /* 应用层调试 */
                        motor_fun.fun_type = eMotor_Fun_Debug_Scan;
                        motor_fun.fun_param_1 = (pInBuff[7] << 8) + pInBuff[8];
                        motor_Emit_FromISR(&motor_fun);
                    } else if (length == 9) {
                        switch (pInBuff[7]) {
                            case 0xFE:
                                gMotorPressureStopBits_Set(eMotor_Fun_PRE_BARCODE, 0);
                                motor_fun.fun_type = eMotor_Fun_PRE_BARCODE;
                                motor_Emit_FromISR(&motor_fun);
                                break;
                            case 0xFF:
                                gMotorPressureStopBits_Set(eMotor_Fun_PRE_BARCODE, 1);
                                break;
                        }
                    }
                    break;
                case 1:                /* 托盘电机 */
                    if (length == 9) { /* 应用层调试 */
                        switch ((pInBuff[7])) {
                            case 0:
                                motor_fun.fun_type = eMotor_Fun_In;
                                motor_Emit_FromISR(&motor_fun);
                                break;
                            case 1:
                                motor_fun.fun_type = eMotor_Fun_Debug_Tray_Scan;
                                motor_Emit_FromISR(&motor_fun);
                                break;
                            case 2:
                                motor_fun.fun_type = eMotor_Fun_Out;
                                motor_Emit_FromISR(&motor_fun);
                                break;
                            case 0xFE:
                                gMotorPressureStopBits_Set(eMotor_Fun_PRE_TRAY, 0);
                                motor_fun.fun_type = eMotor_Fun_PRE_TRAY;
                                motor_Emit_FromISR(&motor_fun);
                                break;
                            case 0xFF:
                                gMotorPressureStopBits_Set(eMotor_Fun_PRE_TRAY, 1);
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
                            case 1:
                                motor_fun.fun_type = eMotor_Fun_Debug_Heater;
                                motor_fun.fun_param_1 = pInBuff[7];
                                motor_Emit_FromISR(&motor_fun);
                                break;
                            case 0xFE:
                                gMotorPressureStopBits_Set(eMotor_Fun_PRE_HEATER, 0);
                                motor_fun.fun_type = eMotor_Fun_PRE_HEATER;
                                motor_Emit_FromISR(&motor_fun);
                                break;
                            case 0xFF:
                                gMotorPressureStopBits_Set(eMotor_Fun_PRE_HEATER, 1);
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
                            case 1:
                                motor_fun.fun_type = eMotor_Fun_Debug_White;
                                motor_fun.fun_param_1 = pInBuff[7];
                                motor_Emit_FromISR(&motor_fun);
                                break;
                            case 0xFE:
                                gMotorPressureStopBits_Set(eMotor_Fun_PRE_WHITE, 0);
                                motor_fun.fun_type = eMotor_Fun_PRE_WHITE;
                                motor_Emit_FromISR(&motor_fun);
                                break;
                            case 0xFF:
                                gMotorPressureStopBits_Set(eMotor_Fun_PRE_WHITE, 1);
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
                                gMotorPressureStopBits_Set(eMotor_Fun_PRE_ALL, 0);
                                motor_fun.fun_type = eMotor_Fun_PRE_ALL;
                                motor_Emit_FromISR(&motor_fun);
                                break;
                            case 0xFF:
                                gMotorPressureStopBits_Set(eMotor_Fun_PRE_ALL, 1);
                                break;
                            default:
                                break;
                        }
                    }
                    break;
                case 0xFF:
                    gMotorPressureStopBits_Clear();
                    break;
                default:
                    break;
            }
            break;
        case eProtocolEmitPack_Client_CMD_Debug_Correct:
            if (length == 8) {
                gComm_Data_Correct_Flag_Mark();                   /* 标记进入定标状态 */
                motor_fun.fun_type = eMotor_Fun_Correct;          /* 电机执行定标 */
                motor_fun.fun_param_1 = pInBuff[6];               /* 定标段索引偏移 */
                if (motor_Emit_FromISR(&motor_fun) == 0) {        /* 成功提交 */
                    gMotor_Sampl_Comm_Set(eMotor_Sampl_Comm_Out); /* 标记来源为外串口 */
                }
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
                    comm_Out_SendTask_QueueEmitWithBuild_FromISR(eProtocolEmitPack_Client_CMD_Debug_Heater, pInBuff, 1);
                    break;
                case 8: /* 一个参数 配置加热使能状态 */
                    switch (pInBuff[6]) {
                        case 0:
                        case 1:
                        case 2:
                        case 3:
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
                        case 4:
                            pInBuff[0] = 4;
                            heater_Overshoot_Get_All(eHeater_BTM, pInBuff + 1);
                            comm_Out_SendTask_QueueEmitWithBuild_FromISR(eProtocolEmitPack_Client_CMD_Debug_Heater, pInBuff, 29);
                            break;
                        case 5:
                            pInBuff[0] = 5;
                            heater_Overshoot_Get_All(eHeater_TOP, pInBuff + 1);
                            comm_Out_SendTask_QueueEmitWithBuild_FromISR(eProtocolEmitPack_Client_CMD_Debug_Heater, pInBuff, 29);
                            break;
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
                        comm_Out_SendTask_QueueEmitWithBuild_FromISR(eProtocolEmitPack_Client_CMD_Debug_Heater, pInBuff, 3 + 4 * pInBuff[2]);
                    } else { /* 上加热体 */
                        pInBuff[0] = pInBuff[6];
                        pInBuff[1] = pInBuff[7];
                        pInBuff[2] = pInBuff[8];
                        for (result = 0; result < pInBuff[2]; ++result) {
                            temp = heater_TOP_Conf_Get(pInBuff[7] + result);
                            memcpy(pInBuff + 3 + 4 * result, (uint8_t *)(&temp), 4);
                        }
                        comm_Out_SendTask_QueueEmitWithBuild_FromISR(eProtocolEmitPack_Client_CMD_Debug_Heater, pInBuff, 3 + 4 * pInBuff[2]);
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
                case 20:
                    switch (pInBuff[6]) {
                        case 4:
                            heater_Overshoot_Set_All(eHeater_BTM, pInBuff + 7);
                            break;
                        case 5:
                            heater_Overshoot_Set_All(eHeater_TOP, pInBuff + 7);
                            break;
                        default:
                            error_Emit_FromISR(eError_Comm_Out_Param_Error);
                            break;
                    }
                    break;
            }
            break;
        case eProtocolEmitPack_Client_CMD_Debug_Flag: /* 调试开关 */
            if (length == 7) {
                pInBuff[0] = protocol_Debug_Get();
                comm_Out_SendTask_QueueEmitWithBuild_FromISR(eProtocolEmitPack_Client_CMD_Debug_Flag, pInBuff, 1);
            } else if (length == 8) {
                gMotor_Aging_Sleep_Set(pInBuff[6]);
            } else if (length == 9) {
                if (pInBuff[7] == 0) {
                    protocol_Debug_Clear(pInBuff[6]);
                } else {
                    protocol_Debug_Mark(pInBuff[6]);
                }
            } else {
                result = comm_Data_Sample_Data_Fetch(pInBuff[6], pInBuff + 1, pInBuff);
                comm_Out_SendTask_QueueEmitWithBuild_FromISR(eProtocolEmitPack_Client_CMD_Debug_Flag, pInBuff + 1, pInBuff[0]);
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
            result = storgeReadConfInfo_FromISR((pInBuff[6] << 16) + (pInBuff[7] << 8) + pInBuff[8], (pInBuff[9] << 16) + (pInBuff[10] << 8) + pInBuff[11]);
            if (result == 0) {
                storgeTaskNotification_FromISR(eStorgeNotifyConf_Read_Flash, eComm_Out); /* 通知存储任务 */
            }
            break;
        case eProtocolEmitPack_Client_CMD_Debug_Flash_Write: /* SPI Flash 写测试 */
            result = storgeWriteConfInfo_FromISR((pInBuff[6] << 16) + (pInBuff[7] << 8) + pInBuff[8], &pInBuff[12],
                                                 (pInBuff[9] << 16) + (pInBuff[10] << 8) + pInBuff[11]);
            if (result == 0) {
                storgeTaskNotification_FromISR(eStorgeNotifyConf_Write_Flash, eComm_Out); /* 通知存储任务 */
            }
            break;
        case eProtocolEmitPack_Client_CMD_Debug_EEPROM_Read: /* EEPROM 读测试 */
            result = storgeReadConfInfo_FromISR((pInBuff[6] << 16) + (pInBuff[7] << 8) + pInBuff[8], (pInBuff[9] << 16) + (pInBuff[10] << 8) + pInBuff[11]);
            if (result == 0) {
                storgeTaskNotification_FromISR(eStorgeNotifyConf_Read_ID_Card, eComm_Out); /* 通知存储任务 */
            }
            break;
        case eProtocolEmitPack_Client_CMD_Debug_EEPROM_Write: /* EEPROM 写测试 */
            result = storgeWriteConfInfo_FromISR((pInBuff[6] << 16) + (pInBuff[7] << 8) + pInBuff[8], &pInBuff[12],
                                                 (pInBuff[9] << 16) + (pInBuff[10] << 8) + pInBuff[11]);
            if (result == 0) {
                storgeTaskNotification_FromISR(eStorgeNotifyConf_Write_ID_Card, eComm_Out); /* 通知存储任务 */
            }
            break;
        case eProtocolEmitPack_Client_CMD_Debug_Self_Check: /* 自检测试 */
            if (length == 7) {
                protocol_Self_Check_Temp_TOP_FromISR(pInBuff, eComm_Out);
                protocol_Self_Check_Temp_BTM_FromISR(pInBuff, eComm_Out);
                protocol_Self_Check_Temp_ENV_FromISR(pInBuff, eComm_Out);
                motor_fun.fun_type = eMotor_Fun_Self_Check;                            /* 整体自检测试 */
                motor_Emit_FromISR(&motor_fun);                                        /* 提交到电机队列 */
                storgeTaskNotification_FromISR(eStorgeNotifyConf_Test_All, eComm_Out); /* 通知存储任务 */
            } else if (length == 8) {                                                  /* 单向测试结果 */
                switch (pInBuff[6]) {
                    case 1: /* 上加热体温度结果 */
                        protocol_Self_Check_Temp_TOP_FromISR(pInBuff, eComm_Out);
                        break;
                    case 2: /* 下加热体温度结果 */
                        protocol_Self_Check_Temp_BTM_FromISR(pInBuff, eComm_Out);
                        break;
                    case 3: /* 环境温度结果 */
                        protocol_Self_Check_Temp_ENV_FromISR(pInBuff, eComm_Out);
                        break;
                    case 4:                                                                      /* 外部Flash */
                        storgeTaskNotification_FromISR(eStorgeNotifyConf_Test_Flash, eComm_Out); /* 通知存储任务 */
                        break;
                    case 5:                                                                        /* ID Code 卡 */
                        storgeTaskNotification_FromISR(eStorgeNotifyConf_Test_ID_Card, eComm_Out); /* 通知存储任务 */
                        break;
                    case 0x0B:                                                                   /* PD */
                        motor_fun.fun_param_1 = 0x07;                                            /* 默认全部波长 */
                        motor_fun.fun_type = eMotor_Fun_Self_Check_Motor_White - 6 + pInBuff[6]; /* 整体自检测试 单项 */
                        motor_Emit_FromISR(&motor_fun);                                          /* 提交到电机队列 */
                        break;
                    case 0xFA: /* 生产板厂检测项目 */
                        protocol_Self_Check_Temp_ENV_FromISR(pInBuff, eComm_Out);
                        motor_fun.fun_type = eMotor_Fun_Self_Check_FA;                           /* 自检测试 生产板厂 */
                        motor_Emit_FromISR(&motor_fun);                                          /* 提交到电机队列 */
                        storgeTaskNotification_FromISR(eStorgeNotifyConf_Test_Flash, eComm_Out); /* 通知存储任务 */
                        break;
                    default:
                        motor_fun.fun_type = eMotor_Fun_Self_Check_Motor_White - 6 + pInBuff[6]; /* 整体自检测试 单项 */
                        motor_Emit_FromISR(&motor_fun);                                          /* 提交到电机队列 */
                        break;
                }
            } else if (length == 9) {
                motor_fun.fun_type = eMotor_Fun_Self_Check_Motor_White - 6 + pInBuff[6]; /* 整体自检测试 单项 */
                motor_fun.fun_param_1 = pInBuff[7];                                      /* 自检参数 PD灯掩码 */
                motor_Emit_FromISR(&motor_fun);                                          /* 提交到电机队列 */
            }
            break;
        case eProtocolEmitPack_Client_CMD_Debug_Motor_Fun: /* 电机功能 */
            motor_fun.fun_type = (eMotor_Fun)pInBuff[6];   /* 开始测试 */
            pInBuff[0] = motor_Emit_FromISR(&motor_fun);   /* 提交到电机队列 */
            comm_Out_SendTask_QueueEmitWithBuild_FromISR(eProtocolEmitPack_Client_CMD_Debug_Motor_Fun, pInBuff, 1);
            break;
        case eProtocolEmitPack_Client_CMD_Debug_System: /* 系统控制 */
            if (length == 7) {                          /* 无参数 重启 */
                comm_Data_Board_Reset();                /* 重置采样板 */
                HAL_NVIC_SystemReset();                 /* 重新启动 */
            } else if (length == 8) {                   /* 单一参数 杂散光测试 灯BP */
                comm_Data_GPIO_Init();                  /* 初始化通讯管脚 */
                if (pInBuff[6] == 0) {
                    motor_fun.fun_type = eMotor_Fun_Stary_Test; /* 杂散光测试 */
                    motor_Emit_FromISR(&motor_fun);             /* 提交到任务队列 */
                } else if (pInBuff[6] == 1) {
                    motor_fun.fun_type = eMotor_Fun_Lamp_BP; /* 灯BP */
                    motor_Emit_FromISR(&motor_fun);          /* 提交到任务队列 */
                } else if (pInBuff[6] == 2) {
                    motor_fun.fun_type = eMotor_Fun_SP_LED; /* LED校正 */
                    motor_Emit_FromISR(&motor_fun);         /* 提交到任务队列 */
                } else if (pInBuff[6] == 3) {               /* 读取杂散光 */
                    comm_Data_Conf_Offset_Get_FromISR();
                }
            } else {
                error_Emit_FromISR(eError_Comm_Out_Param_Error);
            }
            break;
        case eProtocolEmitPack_Client_CMD_Debug_Params:         /* 参数设置 */
            if (length == 9) {                                  /* 保存参数 */
                status = (pInBuff[6] << 0) + (pInBuff[7] << 8); /* 起始索引 */
                if (status == 0xFFFF) {
                    storgeTaskNotification_FromISR(eStorgeNotifyConf_Dump_Params, eComm_Out);
                }
            } else if (length == 11) {                                                                                             /* 读取参数 */
                result = storgeReadConfInfo_FromISR((pInBuff[6] << 0) + (pInBuff[7] << 8), (pInBuff[8] << 0) + (pInBuff[9] << 8)); /* 配置 */
                if (result == 0) {
                    storgeTaskNotification_FromISR(eStorgeNotifyConf_Read_Parmas, eComm_Out); /* 通知存储任务 */
                }
            } else if (length > 11 && ((length - 11) % 4 == 0)) { /* 写入参数 */
                result =
                    storgeWriteConfInfo_FromISR((pInBuff[6] << 0) + (pInBuff[7] << 8), &pInBuff[10], 4 * ((pInBuff[8] << 0) + (pInBuff[9] << 8))); /* 配置 */
                if (result == 0) {
                    storgeTaskNotification_FromISR(eStorgeNotifyConf_Write_Parmas, eComm_Out); /* 通知存储任务 */
                }
            } else {
                error_Emit_FromISR(eError_Comm_Out_Param_Error);
            }
            break;
        case eProtocolEmitPack_Client_CMD_Debug_BL:             /* 升级Bootloader */
            if ((pInBuff[10] << 0) + (pInBuff[11] << 8) == 0) { /* 数据长度为空 尾包 */
                result = Innate_Flash_Dump((pInBuff[6] << 0) + (pInBuff[7] << 8),
                                           (pInBuff[12] << 0) + (pInBuff[13] << 8) + (pInBuff[14] << 16) + (pInBuff[15] << 24));
                pInBuff[0] = result;
                comm_Out_SendTask_QueueEmitWithBuild_FromISR(0xDE, pInBuff, 1);
            } else {                                              /* 数据长度不为空 非尾包 */
                if ((pInBuff[8] << 0) + (pInBuff[9] << 8) == 0) { /* 起始包 */
                    result = Innate_Flash_Erase_Temp();           /* 擦除Flash */
                    if (result > 0) {                             /* 擦除失败 */
                        HAL_FLASH_Lock();                         /* 回锁 */
                        pInBuff[0] = result;
                        comm_Out_SendTask_QueueEmitWithBuild_FromISR(0xDE, pInBuff, 1);
                        break;
                    }
                }
                result =
                    Innate_Flash_Write(INNATE_FLASH_ADDR_TEMP + (pInBuff[8] << 0) + (pInBuff[9] << 8), pInBuff + 12, (pInBuff[10] << 0) + (pInBuff[11] << 8));
                pInBuff[0] = result;
                comm_Out_SendTask_QueueEmitWithBuild_FromISR(0xDE, pInBuff, 1);
            }
            break;
        case eProtocolEmitPack_Client_CMD_Debug_Version:
            memcpy(pInBuff, (uint8_t *)(UID_BASE), 12);
            memcpy(pInBuff + 12, (uint8_t *)__TIME__, strlen(__TIME__));
            memcpy(pInBuff + 12 + strlen(__TIME__), (uint8_t *)__DATE__, strlen(__DATE__));
            comm_Out_SendTask_QueueEmitWithBuild_FromISR(0xDF, pInBuff, 12 + strlen(__TIME__) + strlen(__DATE__));
            break;
        case eProtocolEmitPack_Client_CMD_START:           /* 开始测量帧 0x01 */
            gComm_Data_Sample_Max_Point_Clear();           /* 清除最大点数 */
            protocol_Temp_Upload_Pause();                  /* 暂停温度上送 */
            comm_Data_GPIO_Init();                         /* 初始化通讯管脚 */
            if (protocol_Debug_AgingLoop()) {              /* 老化测试 */
                motor_fun.fun_type = eMotor_Fun_AgingLoop; /* 老化测试 */
            } else {
                motor_fun.fun_type = eMotor_Fun_Sample_Start; /* 普通测试 */
            }
            if (motor_Emit_FromISR(&motor_fun) == 0) {        /* 提交到电机队列 */
                comm_Data_Sample_Send_Clear_Conf_FromISR();   /* 清除采样板上配置信息 */
                gMotor_Sampl_Comm_Set(eMotor_Sampl_Comm_Out); /* 标记来源为外串口 */
            }
            break;
        case eProtocolEmitPack_Client_CMD_ABRUPT:                   /* 仪器测量取消命令帧 0x02 */
            if (gMotor_Sampl_Comm_Get() != eMotor_Sampl_Comm_Out) { /* 与启动测试命令来源不同 */
                break;
            }
            barcode_Interrupt_Flag_Mark();                    /* 标记打断扫码 */
            comm_Data_Sample_Force_Stop_FromISR();            /* 强行停止采样定时器 */
            motor_Sample_Info_From_ISR(eMotorNotifyValue_BR); /* 提交打断信息 */
            break;
        case eProtocolEmitPack_Client_CMD_CONFIG:            /* 测试项信息帧 0x03 */
            comm_Data_Sample_Send_Conf_FromISR(&pInBuff[6]); /* 发送测试配置 */
            break;
        case eProtocolEmitPack_Client_CMD_FORWARD: /* 打开托盘帧 0x04 */
            motor_fun.fun_type = eMotor_Fun_Out;   /* 配置电机动作套餐类型 出仓 */
            motor_Emit_FromISR(&motor_fun);        /* 交给电机任务 出仓 */
            break;
        case eProtocolEmitPack_Client_CMD_REVERSE: /* 关闭托盘命令帧 0x05 */
            motor_fun.fun_type = eMotor_Fun_In;    /* 配置电机动作套餐类型 进仓 */
            motor_Emit_FromISR(&motor_fun);        /* 交给电机任务 进仓 */
            break;
        case eProtocolEmitPack_Client_CMD_READ_ID:        /* ID卡读取命令帧 0x06 */
            result = storgeReadConfInfo_FromISR(0, 4096); /* 暂无定义 按最大读取 */
            if (result == 0) {
                storgeTaskNotification_FromISR(eStorgeNotifyConf_Read_ID_Card, eComm_Out); /* 通知存储任务 */
            }
            break;
        case eProtocolEmitPack_Client_CMD_STATUS:                                                   /* 状态信息查询帧 (首帧) 0x07 */
            temp = temp_Get_Temp_Data_BTM();                                                        /* 下加热体温度 */
            pInBuff[0] = ((uint16_t)(temp * 100)) & 0xFF;                                           /* 小端模式 低8位 */
            pInBuff[1] = ((uint16_t)(temp * 100)) >> 8;                                             /* 小端模式 高8位 */
            temp = temp_Get_Temp_Data_TOP();                                                        /* 上加热体温度 */
            pInBuff[2] = ((uint16_t)(temp * 100)) & 0xFF;                                           /* 小端模式 低8位 */
            pInBuff[3] = ((uint16_t)(temp * 100)) >> 8;                                             /* 小端模式 高8位 */
            comm_Out_SendTask_QueueEmitWithBuild_FromISR(eProtocolRespPack_Client_TMP, pInBuff, 4); /* 温度信息 */

            protocol_Get_Version(pInBuff);
            comm_Out_SendTask_QueueEmitWithBuild_FromISR(eProtocolRespPack_Client_VER, pInBuff, 4); /* 软件版本信息 */

            if (tray_Motor_Get_Status_Position() == 0) { /* 托盘状态信息 */
                pInBuff[0] = 1;                          /* 托盘处于测试位置 原点 */
            } else if (tray_Motor_Get_Status_Position() >= (eTrayIndex_2 / 4 - 50) && tray_Motor_Get_Status_Position() <= (eTrayIndex_2 / 4 + 50)) {
                pInBuff[0] = 2; /* 托盘处于出仓位置 误差范围 +-50步 */
            } else {
                pInBuff[0] = 0;
            }
            comm_Out_SendTask_QueueEmitWithBuild_FromISR(eProtocolRespPack_Client_DISH, pInBuff, 1); /* 托盘状态信息 */
            break;
        case eProtocolEmitPack_Client_CMD_TEST:                 /* 工装测试配置帧 0x08 */
            comm_Data_Sample_Send_Conf_TV_FromISR(&pInBuff[6]); /* 保存测试配置 */
            break;
        case eProtocolEmitPack_Client_CMD_UPGRADE: /* 下位机升级命令帧 0x0F */
            if (spi_FlashWriteAndCheck_Word(0x0000, 0x87654321) == 0) {
                heater_BTM_Output_Stop();
                heater_TOP_Output_Stop();
                HAL_NVIC_SystemReset(); /* 重新启动 */
            } else {
                error_Emit_FromISR(eError_Out_Flash_Write_Failed);
            }
            break;
        case eProtocolEmitPack_Client_CMD_SP_LED_GET:
            comm_Data_Conf_LED_Voltage_Get_FromISR();
            break;
        case eProtocolEmitPack_Client_CMD_SP_LED_SET:
            comm_Data_Conf_LED_Voltage_Set_FromISR(&pInBuff[6]);
            break;
        case eProtocolEmitPack_Client_CMD_FA_PD_SET:
            comm_Data_Conf_FA_PD_Set_FromISR(&pInBuff[6]);
            break;
        case eProtocolEmitPack_Client_CMD_FA_LED_SET:
            if (length != 7 + 2) {
                error_Emit_FromISR(eError_Comm_Out_Param_Error);
                break;
            }
            comm_Data_Conf_FA_LED_Set_FromISR(&pInBuff[6]);
            break;
        case eProtocolEmitPack_Client_CMD_SP_WHITE_MAGNIFY_GET:
            comm_Data_Conf_White_Magnify_Get_FromISR();
            break;
        case eProtocolEmitPack_Client_CMD_SP_WHITE_MAGNIFY_SET:
            comm_Data_Conf_White_Magnify_Set_FromISR(&pInBuff[6]);
            break;
        case eProtocolEmitPack_Client_CMD_SAMPLE_VER:
        case eProtocolEmitPack_Client_CMD_BL_INSTR:
        case eProtocolEmitPack_Client_CMD_BL_DATA:
            comm_Data_Transit_FromISR(pInBuff, length);
            break;
        default:
            error_Emit_FromISR(eError_Comm_Out_Unknow_CMD);
            break;
    }
}

/**
 * @brief  上位机解析协议 预过滤处理
 * @param  pInBuff 入站指针
 * @param  length 入站长度
 * @param  pOutBuff 出站指针
 * @retval None
 */
uint8_t protocol_Parse_Main_ISR(uint8_t * pInBuff, uint16_t length)
{
    static uint8_t last_ack = 0;
    BaseType_t result = pdFALSE;

    if (pInBuff[4] == PROTOCOL_DEVICE_ID_CTRL) { /* 回声现象 */
        return 0;
    }
    protocol_Temp_Upload_Comm_Set(eComm_Main, 1); /* 通讯接收成功 使能本串口温度上送 */

    if (pInBuff[5] == eProtocolRespPack_Client_ACK) { /* 收到对方回应帧 */
        comm_Main_Send_ACK_Give_From_ISR(pInBuff[6]); /* 通知串口发送任务 回应包收到 */
        return 0;                                     /* 直接返回 */
    }

    if (comm_Main_DMA_TX_Enter_From_ISR() == pdPASS) {  /* 确保发送完成信号量被释放 */
        gProtocol_Main_ACK_Pack_Buffer[0] = pInBuff[3]; /* 回应ACK号 */
        result = serialSendStartIT(COMM_MAIN_SERIAL_INDEX, gProtocol_Main_ACK_Pack_Buffer,
                                   buildPackOrigin(eComm_Main, eProtocolRespPack_Client_ACK, &gProtocol_Main_ACK_Pack_Buffer[0], 1));
    }
    if (result == pdFALSE) {                                  /* 中断发送失败 */
        comm_Main_SendTask_ACK_QueueEmitFromISR(&pInBuff[3]); /* 投入发送任务处理 */
    }

    if (last_ack == pInBuff[3]) { /* 收到与上一帧号相同帧 */
        return 0;                 /* 不做处理 */
    }
    last_ack = pInBuff[3]; /* 记录上一帧号 */

    protocol_Parse_Main_Fun_ISR(pInBuff, length);
    return 0;
}

/**
 * @brief  上位机解析协议 中断版本
 * @param  pInBuff 入站指针
 * @param  length 入站长度
 * @retval 解析数据包结果
 */
static void protocol_Parse_Main_Fun_ISR(uint8_t * pInBuff, uint16_t length)
{
    uint8_t result;
    sMotor_Fun motor_fun;
    float temp;

    switch (pInBuff[5]) {                                      /* 进一步处理 功能码 */
        case eProtocolEmitPack_Client_CMD_START:               /* 开始测量帧 0x01 */
            gComm_Data_Sample_Max_Point_Clear();               /* 清除最大点数 */
            protocol_Temp_Upload_Pause();                      /* 暂停温度上送 */
            comm_Data_GPIO_Init();                             /* 初始化通讯管脚 */
            motor_fun.fun_type = eMotor_Fun_Sample_Start;      /* 开始测试 */
            if (motor_Emit_FromISR(&motor_fun) == 0) {         /* 提交到电机队列 */
                comm_Data_Sample_Send_Clear_Conf_FromISR();    /* 清除采样板上配置信息 */
                gMotor_Sampl_Comm_Set(eMotor_Sampl_Comm_Main); /* 标记来源为主串口 */
            }
            break;
        case eProtocolEmitPack_Client_CMD_ABRUPT:                    /* 仪器测量取消命令帧 0x02 */
            if (gMotor_Sampl_Comm_Get() != eMotor_Sampl_Comm_Main) { /* 与启动测试命令来源不同 */
                break;
            }
            barcode_Interrupt_Flag_Mark();                    /* 标记打断扫码 */
            comm_Data_Sample_Force_Stop_FromISR();            /* 强行停止采样定时器 */
            motor_Sample_Info_From_ISR(eMotorNotifyValue_BR); /* 提交打断信息 */
            break;
        case eProtocolEmitPack_Client_CMD_CONFIG:                    /* 测试项信息帧 0x03 */
            if (gMotor_Sampl_Comm_Get() != eMotor_Sampl_Comm_Main) { /* 与启动测试命令来源不同 */
                break;
            }
            comm_Data_Sample_Send_Conf_FromISR(&pInBuff[6]); /* 发送测试配置 */
            break;
        case eProtocolEmitPack_Client_CMD_FORWARD: /* 打开托盘帧 0x04 */
            motor_fun.fun_type = eMotor_Fun_Out;   /* 配置电机动作套餐类型 出仓 */
            motor_Emit_FromISR(&motor_fun);        /* 交给电机任务 出仓 */
            break;
        case eProtocolEmitPack_Client_CMD_REVERSE: /* 关闭托盘命令帧 0x05 */
            motor_fun.fun_type = eMotor_Fun_In;    /* 配置电机动作套餐类型 进仓 */
            motor_Emit_FromISR(&motor_fun);        /* 交给电机任务 进仓 */
            break;
        case eProtocolEmitPack_Client_CMD_READ_ID:        /* ID卡读取命令帧 0x06 */
            result = storgeReadConfInfo_FromISR(0, 4096); /* 暂无定义 按最大读取 */
            if (result == 0) {
                storgeTaskNotification_FromISR(eStorgeNotifyConf_Read_ID_Card, eComm_Main); /* 通知存储任务 */
            }
            break;
        case eProtocolEmitPack_Client_CMD_STATUS:                                                    /* 状态信息查询帧 (首帧) */
            temp = temp_Get_Temp_Data_BTM();                                                         /* 下加热体温度 */
            pInBuff[0] = ((uint16_t)(temp * 100)) & 0xFF;                                            /* 小端模式 低8位 */
            pInBuff[1] = ((uint16_t)(temp * 100)) >> 8;                                              /* 小端模式 高8位 */
            temp = temp_Get_Temp_Data_TOP();                                                         /* 上加热体温度 */
            pInBuff[2] = ((uint16_t)(temp * 100)) & 0xFF;                                            /* 小端模式 低8位 */
            pInBuff[3] = ((uint16_t)(temp * 100)) >> 8;                                              /* 小端模式 高8位 */
            comm_Main_SendTask_QueueEmitWithBuild_FromISR(eProtocolRespPack_Client_TMP, pInBuff, 4); /* 温度信息 */
            protocol_Get_Version(pInBuff);
            comm_Main_SendTask_QueueEmitWithBuild_FromISR(eProtocolRespPack_Client_VER, pInBuff, 4); /* 软件版本信息 */

            if (tray_Motor_Get_Status_Position() == 0) { /* 托盘状态 */
                pInBuff[0] = 1;                          /* 托盘处于测试位置 原点 */
            } else if (tray_Motor_Get_Status_Position() >= (eTrayIndex_2 / 4 - 50) && tray_Motor_Get_Status_Position() <= (eTrayIndex_2 / 4 + 50)) {
                pInBuff[0] = 2; /* 托盘处于出仓位置 误差范围 +-50步 */
            } else {
                pInBuff[0] = 0;
            }
            comm_Main_SendTask_QueueEmitWithBuild_FromISR(eProtocolRespPack_Client_DISH, pInBuff, 1); /* 托盘状态信息 */
            break;
        case eProtocolEmitPack_Client_CMD_TEST:                 /* 工装测试配置帧 0x08 */
            comm_Data_Sample_Send_Conf_TV_FromISR(&pInBuff[6]); /* 保存测试配置 */
            break;
        case eProtocolEmitPack_Client_CMD_UPGRADE: /* 下位机升级命令帧 0x0F */
            if (spi_FlashWriteAndCheck_Word(0x0000, 0x87654321) == 0) {
                heater_BTM_Output_Stop();
                heater_TOP_Output_Stop();
                HAL_NVIC_SystemReset(); /* 重新启动 */
            } else {
                error_Emit_FromISR(eError_Out_Flash_Write_Failed);
            }
            break;
        case eProtocolEmitPack_Client_CMD_SP_LED_GET:
            comm_Data_Conf_LED_Voltage_Get_FromISR();
            break;
        case eProtocolEmitPack_Client_CMD_SP_LED_SET:
            comm_Data_Conf_LED_Voltage_Set_FromISR(&pInBuff[6]);
            break;
        case eProtocolEmitPack_Client_CMD_Debug_Self_Check: /* 自检测试 */
            if (length == 7) {
                protocol_Self_Check_Temp_TOP_FromISR(pInBuff, eComm_Main);
                protocol_Self_Check_Temp_BTM_FromISR(pInBuff, eComm_Main);
                protocol_Self_Check_Temp_ENV_FromISR(pInBuff, eComm_Main);
                motor_fun.fun_type = eMotor_Fun_Self_Check;                             /* 整体自检测试 */
                motor_Emit_FromISR(&motor_fun);                                         /* 提交到电机队列 */
                storgeTaskNotification_FromISR(eStorgeNotifyConf_Test_All, eComm_Main); /* 通知存储任务 */
            } else if (length == 8) {                                                   /* 单向测试结果 */
                switch (pInBuff[6]) {
                    case 1: /* 上加热体温度结果 */
                        protocol_Self_Check_Temp_TOP_FromISR(pInBuff, eComm_Main);
                        break;
                    case 2: /* 下加热体温度结果 */
                        protocol_Self_Check_Temp_BTM_FromISR(pInBuff, eComm_Main);
                        break;
                    case 3: /* 环境温度结果 */
                        protocol_Self_Check_Temp_ENV_FromISR(pInBuff, eComm_Main);
                        break;
                    case 4:                                                                       /* 外部Flash */
                        storgeTaskNotification_FromISR(eStorgeNotifyConf_Test_Flash, eComm_Main); /* 通知存储任务 */
                        break;
                    case 5:                                                                         /* ID Code 卡 */
                        storgeTaskNotification_FromISR(eStorgeNotifyConf_Test_ID_Card, eComm_Main); /* 通知存储任务 */
                        break;
                    case 0x0B:                                                                   /* PD */
                        motor_fun.fun_param_1 = 0x07;                                            /* 默认全部波长 */
                        motor_fun.fun_type = eMotor_Fun_Self_Check_Motor_White - 6 + pInBuff[6]; /* 整体自检测试 单项 */
                        motor_Emit_FromISR(&motor_fun);                                          /* 提交到电机队列 */
                        break;
                    default:
                        motor_fun.fun_type = eMotor_Fun_Self_Check_Motor_White - 6 + pInBuff[6]; /* 整体自检测试 单项 */
                        motor_Emit_FromISR(&motor_fun);                                          /* 提交到电机队列 */
                        break;
                }
            } else if (length == 9) {
                motor_fun.fun_type = eMotor_Fun_Self_Check_Motor_White - 6 + pInBuff[6]; /* 整体自检测试 单项 */
                motor_fun.fun_param_1 = pInBuff[7];                                      /* 自检参数 PD灯掩码 */
                motor_Emit_FromISR(&motor_fun);                                          /* 提交到电机队列 */
            }
            break;
        case eProtocolEmitPack_Client_CMD_Debug_Correct:
            if (length == 8) {
                gComm_Data_Correct_Flag_Mark();                    /* 标记进入定标状态 */
                motor_fun.fun_type = eMotor_Fun_Correct;           /* 电机执行定标 */
                motor_fun.fun_param_1 = pInBuff[6];                /* 定标段索引偏移 */
                if (motor_Emit_FromISR(&motor_fun) == 0) {         /* 成功提交 */
                    gMotor_Sampl_Comm_Set(eMotor_Sampl_Comm_Main); /* 标记来源为外串口 */
                }
            }
            break;
        case eProtocolEmitPack_Client_CMD_Debug_System: /* 系统控制 */
            if (length == 7) {                          /* 无参数 重启 */
                comm_Data_Board_Reset();                /* 重置采样板 */
                HAL_NVIC_SystemReset();                 /* 重新启动 */
            } else if (length == 8) {                   /* 单一参数 杂散光测试 灯BP */
                comm_Data_GPIO_Init();                  /* 初始化通讯管脚 */
                if (pInBuff[6] == 0) {
                    motor_fun.fun_type = eMotor_Fun_Stary_Test; /* 杂散光测试 */
                    motor_Emit_FromISR(&motor_fun);             /* 提交到任务队列 */
                } else if (pInBuff[6] == 1) {
                    motor_fun.fun_type = eMotor_Fun_Lamp_BP; /* 灯BP */
                    motor_Emit_FromISR(&motor_fun);          /* 提交到任务队列 */
                } else if (pInBuff[6] == 2) {
                    motor_fun.fun_type = eMotor_Fun_SP_LED; /* LED校正 */
                    motor_Emit_FromISR(&motor_fun);         /* 提交到任务队列 */
                } else if (pInBuff[6] == 3) {               /* 读取杂散光 */
                    comm_Data_Conf_Offset_Get_FromISR();
                }
            } else {
                error_Emit_FromISR(eError_Comm_Out_Param_Error);
            }
            break;
        case eProtocolEmitPack_Client_CMD_SAMPLE_VER:
        case eProtocolEmitPack_Client_CMD_BL_INSTR:
        case eProtocolEmitPack_Client_CMD_BL_DATA:
            comm_Data_Transit_FromISR(pInBuff, length);
            break;
        default:
            error_Emit_FromISR(eError_Comm_Main_Unknow_CMD);
            break;
    }
    return;
}

/**
 * @brief  采样板解析协议 预过滤处理
 * @param  pInBuff 入站指针
 * @param  length 入站长度
 * @param  pOutBuff 出站指针
 * @retval None
 */
uint8_t protocol_Parse_Data_ISR(uint8_t * pInBuff, uint16_t length)
{
    static uint8_t last_ack = 0;
    BaseType_t result = pdFALSE;

    if (pInBuff[4] == PROTOCOL_DEVICE_ID_CTRL) { /* 回声现象 */
        return 0;
    }

    if (pInBuff[5] == eProtocolRespPack_Client_ACK) { /* 收到对方回应帧 */
        comm_Data_Send_ACK_Give_From_ISR(pInBuff[6]); /* 通知串口发送任务 回应包收到 */
        return 0;                                     /* 直接返回 */
    }

    if (comm_Data_DMA_TX_Enter_From_ISR() == pdPASS) {  /* 确保发送完成信号量被释放 */
        gProtocol_Data_ACK_Pack_Buffer[0] = pInBuff[3]; /* 回应ACK号 */
        result = serialSendStartIT(COMM_DATA_SERIAL_INDEX, gProtocol_Data_ACK_Pack_Buffer,
                                   buildPackOrigin(eComm_Data, eProtocolRespPack_Client_ACK, &gProtocol_Data_ACK_Pack_Buffer[0], 1));
    }
    if (result == pdFALSE) {                                  /* 中断发送失败 */
        comm_Data_SendTask_ACK_QueueEmitFromISR(&pInBuff[3]); /* 投入发送任务处理 */
    }

    if (last_ack == pInBuff[3]) { /* 收到与上一帧号相同帧 */
        return 0;                 /* 不做处理 */
    }
    last_ack = pInBuff[3]; /* 记录上一帧号 */

    protocol_Parse_Data_Fun_ISR(pInBuff, length);
    return 0;
}

/**
 * @brief  采样板解析协议
 * @param  pInBuff 入站指针
 * @param  length 入站长度
 * @param  pOutBuff 出站指针
 * @retval 解析数据包结果
 */
static void protocol_Parse_Data_Fun_ISR(uint8_t * pInBuff, uint16_t length)
{
    uint8_t data_length;
    uint16_t i;
    eComm_Data_Sample_Data type;

    switch (pInBuff[5]) {                                                                                                  /* 进一步处理 功能码 */
        case eComm_Data_Inbound_CMD_DATA:                                                                                  /* 采集数据帧 */
            if (gComm_Data_Correct_Flag_Check()) {                                                                         /* 处于定标状态 */
                stroge_Conf_CC_O_Data_From_B3(pInBuff + 6, length - 9);                                                    /* 修改测量点 */
                comm_Out_SendTask_QueueEmitWithBuild_FromISR(eProtocolRespPack_Client_SAMP_DATA, &pInBuff[6], length - 7); /* 转发至外串口 */
            } else if (comm_Data_SP_LED_Is_Running() || gComm_Data_SelfCheck_PD_Flag_Get()) { /* 处于LED校正状态 或 自检测试 单项 PD */
                comm_Data_Sample_Data_Commit(pInBuff[7], pInBuff, length - 9, 0);             /* 不允许替换 先白板后反应区 */
            } else {                                                                          /* 未出于定标状态 */
                type = comm_Data_Sample_Data_Commit(pInBuff[7], pInBuff, length - 9, 1);      /* 采样数据记录 */
                if (type == eComm_Data_Sample_Data_MIX) {                                     /* 混合数据类型 */
                    length += pInBuff[6] * 2;                                                 /* 补充长度  uin16_t */
                    if (comm_Main_SendTask_Queue_GetFree_FromISR() > 1) {
                        comm_Main_SendTask_QueueEmitWithBuild_FromISR(eProtocolRespPack_Client_SAMP_DATA, &pInBuff[6], length - 7); /* 构造数据包 */
                        comm_Out_SendTask_QueueEmitWithModify_FromISR(pInBuff + 6, length); /* 修改帧号ID后转发 */
                    } else {
                        comm_Out_SendTask_QueueEmitWithBuild_FromISR(eProtocolRespPack_Client_SAMP_DATA, &pInBuff[6], length - 7); /* 构造数据包 */
                    }
                    break;
                }
                if (protocol_Debug_SampleRawData() || type != eComm_Data_Sample_Data_U16 || gComm_Data_Lamp_BP_Flag_Check()) { /* 选择原始数据 */
                    if (comm_Main_SendTask_Queue_GetFree_FromISR() > 1) {
                        comm_Main_SendTask_QueueEmitWithBuild_FromISR(eProtocolRespPack_Client_SAMP_DATA, &pInBuff[6], length - 7); /* 构造数据包 */
                        comm_Out_SendTask_QueueEmitWithModify_FromISR(pInBuff + 6, length); /* 修改帧号ID后转发 */
                    } else {
                        comm_Out_SendTask_QueueEmitWithBuild_FromISR(eProtocolRespPack_Client_SAMP_DATA, &pInBuff[6], length - 7); /* 构造数据包 */
                    }

                    if (type != eComm_Data_Sample_Data_U16 || gComm_Data_Lamp_BP_Flag_Check()) { /* u32类型 或 处于灯BP状态 */
                        break;
                    }
                    for (i = 0; i < length; ++i) { /* 修正偏移 */
                        pInBuff[i] = pInBuff[i + 6];
                    }
                }                                                                 /* 经过校正映射 */
                comm_Data_Sample_Data_Correct(pInBuff[7], pInBuff, &data_length); /* 投影校正 */
                if (comm_Main_SendTask_Queue_GetFree_FromISR() > 1) {
                    comm_Main_SendTask_QueueEmitWithBuild_FromISR(eProtocolRespPack_Client_SAMP_DATA, pInBuff, data_length); /* 构造数据包 */
                    comm_Out_SendTask_QueueEmitWithModify_FromISR(pInBuff, data_length + 7);                                 /* 修改帧号ID后转发 */
                } else {
                    comm_Out_SendTask_QueueEmitWithBuild_FromISR(eProtocolRespPack_Client_SAMP_DATA, pInBuff, data_length); /* 构造数据包 */
                }
            }
            break;
        case eComm_Data_Inbound_CMD_OVER:                         /* 采集数据完成帧 */
            if (comm_Data_Stary_Test_Is_Running()) {              /* 判断是否处于杂散光测试中 */
                motor_Sample_Info_From_ISR(eMotorNotifyValue_SP); /* 通知电机任务杂散光测试完成 */
            }
            break;
        case eComm_Data_Inbound_CMD_ERROR: /* 采集板错误信息帧 */
            comm_Main_SendTask_QueueEmitWithBuild_FromISR(eProtocolRespPack_Client_ERR, &pInBuff[6], 2);
            comm_Out_SendTask_QueueEmitWithModify_FromISR(pInBuff + 6, 2 + 7); /* 修改帧号ID后转发 */
            break;
        case eComm_Data_Inbound_CMD_LED_GET:
            comm_Out_SendTask_QueueEmitWithBuild_FromISR(eProtocolRespPack_Client_LED_Get, &pInBuff[6], length - 7); /* 转发至外串口 */
            break;
        case eComm_Data_Inbound_CMD_FA_DEBUG:
            comm_Out_SendTask_QueueEmitWithBuild_FromISR(eProtocolRespPack_Client_FA_PD, &pInBuff[6], length - 7); /* 转发至外串口 */
            break;
        case eComm_Data_Inbound_CMD_OFFSET_GET:
            comm_Main_SendTask_QueueEmitWithBuild_FromISR(eProtocolRespPack_Client_Offset_Get, &pInBuff[6], length - 7); /* 构造数据包 */
            comm_Out_SendTask_QueueEmitWithModify_FromISR(pInBuff + 6, length);                                          /* 转发至外串口 */
            break;
        case eComm_Data_Inbound_CMD_WHITE_MAGNIFY_GET:
            comm_Main_SendTask_QueueEmitWithBuild_FromISR(eProtocolRespPack_Client_WHITE_MAGNIFY_Get, &pInBuff[6], length - 7); /* 构造数据包 */
            comm_Out_SendTask_QueueEmitWithModify_FromISR(pInBuff + 6, length);                                                 /* 转发至外串口 */
            break;
        case eComm_Data_Inbound_CMD_GET_VERSION:
        case eComm_Data_Inbound_CMD_BL_INSTR:
        case eComm_Data_Inbound_CMD_BL_DATA:
            if (comm_Main_SendTask_Queue_GetWaiting_FromISR() < COMM_MAIN_SEND_QUEU_LENGTH - 6) {   /* 避免阻塞主串口发送队列 */
                comm_Main_SendTask_QueueEmitWithBuild_FromISR(pInBuff[5], &pInBuff[6], length - 7); /* 构造数据包 */
                comm_Out_SendTask_QueueEmitWithModify_FromISR(pInBuff + 6, length);                 /* 转发至外串口 */
            } else {
                comm_Out_SendTask_QueueEmitWithBuild_FromISR(pInBuff[5], &pInBuff[6], length - 7); /* 构造数据包 */
            }
            break;
        default:
            error_Emit_FromISR(eError_Comm_Data_Unknow_CMD);
            break;
    }
    return;
}

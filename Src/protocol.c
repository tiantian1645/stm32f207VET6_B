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
static eComm_Data_Sample gComm_Data_Sample_Buffer;

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
    return 0;
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
    if (pData[5] == eProtocoleRespPack_Client_ACK) {
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
    if (length == 237) {
        return 0;
    }
    return 0;
}

/**
 * @brief  上位机及采样板通信协议包构造函数 原地版本
 * @oaram  index  协议出口类型
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
 * @oaram  index  协议出口类型
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
            if (comm_Out_SendTask_QueueEmit(send_buff, sendLength, 50) != pdPASS) { /* 提交回应包 */
                return PROTOCOL_PARSE_EMIT_ERROR;
            }
            break;
        case eComm_Main:
            if (comm_Main_SendTask_QueueEmit(send_buff, sendLength, 50) != pdPASS) { /* 提交回应包 */
                return PROTOCOL_PARSE_EMIT_ERROR;
            }
            break;
        case eComm_Data:
            if (comm_Data_SendTask_QueueEmit(send_buff, sendLength, 50) != pdPASS) { /* 提交回应包 */
                return PROTOCOL_PARSE_EMIT_ERROR;
            }
            break;
    }

    return PROTOCOL_PARSE_OK;
}

/**
 * @brief  协议处理 向其他任务提交信息
 * @oaram  index  协议出口类型
 * @param  pInbuff 入站包指针
 * @retval 数据包长度
 */
eProtocolParseResult protocol_Parse_Emit(eProtocol_COMM_Index index, uint8_t * pInBuff, uint8_t length)
{
    eProtocolParseResult error = PROTOCOL_PARSE_OK;

    error = protocol_Parse_AnswerACK(index, pInBuff[3]); /* 发送回应包 */
    switch (pInBuff[5]) {
        case eProtocolEmitPack_Client_CMD_START: /* 开始测量帧 */
            break;
        case eProtocolEmitPack_Client_CMD_ABRUPT: /* 仪器测量取消命令帧 */
            break;
        case eProtocolEmitPack_Client_CMD_CONFIG: /* 测试项信息帧 */
            break;
        case eProtocolEmitPack_Client_CMD_FORWARD: /* 打开托盘帧 */
            break;
        case eProtocolEmitPack_Client_CMD_REVERSE: /* 关闭托盘命令帧 */
            break;
        case eProtocolEmitPack_Client_CMD_READ_ID: /* ID卡读取命令帧 */
            break;
        case eProtocolEmitPack_Client_CMD_UPGRADE: /* 下位机升级命令帧 */
            break;
        default:
            error |= PROTOCOL_PARSE_CMD_ERROR;
            break;
    }
    return error;
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

    if (pInBuff[5] == eProtocoleRespPack_Client_ACK) { /* 收到对方回应帧 */
        comm_Out_Send_ACK_Notify(pInBuff[6]);          /* 通知串口发送任务 回应包收到 */
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
                heat_Motor_Run(eMotorDir_FWD, 3000);
            } else {
                heat_Motor_Run(eMotorDir_REV, 3000);
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
            if (result == 0 && storgeTaskNotification(eStorgeHardwareType_Flash, eStorgeRWType_Read, eComm_Out) == pdPASS) { /* 通知存储任务 */
                error = PROTOCOL_PARSE_OK;
            } else {
                error |= PROTOCOL_PARSE_EMIT_ERROR;
            }
            return error;
        case 0xD7: /* SPI Flash 写测试 */
            result = storgeWriteConfInfo((pInBuff[6] << 16) + (pInBuff[7] << 8) + pInBuff[8], &pInBuff[12],
                                         (pInBuff[9] << 16) + (pInBuff[10] << 8) + pInBuff[11], 200);
            if (result == 0 && storgeTaskNotification(eStorgeHardwareType_Flash, eStorgeRWType_Write, eComm_Out) == pdPASS) { /* 通知存储任务 */
                error = PROTOCOL_PARSE_OK;
            } else {
                error |= PROTOCOL_PARSE_EMIT_ERROR;
            }
            return error;
        case 0xD8: /* EEPROM 读测试 */
            result = storgeReadConfInfo((pInBuff[6] << 16) + (pInBuff[7] << 8) + pInBuff[8], (pInBuff[9] << 16) + (pInBuff[10] << 8) + pInBuff[11], 200);
            if (result == 0 && storgeTaskNotification(eStorgeHardwareType_EEPROM, eStorgeRWType_Read, eComm_Out) == pdPASS) { /* 通知存储任务 */
                error = PROTOCOL_PARSE_OK;
            } else {
                error |= PROTOCOL_PARSE_EMIT_ERROR;
            }
            return error;
        case 0xD9: /* EEPROM 写测试 */
            result = storgeWriteConfInfo((pInBuff[6] << 16) + (pInBuff[7] << 8) + pInBuff[8], &pInBuff[12],
                                         (pInBuff[9] << 16) + (pInBuff[10] << 8) + pInBuff[11], 200);
            if (result == 0 && storgeTaskNotification(eStorgeHardwareType_EEPROM, eStorgeRWType_Write, eComm_Out) == pdPASS) { /* 通知存储任务 */
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
        case eProtocolEmitPack_Client_CMD_START:          /* 开始测量帧 0x01 */
            comm_Data_Sample_Send_Conf();                 /* 发送测试配置 */
            soft_timer_Temp_Pause();                      /* 暂停温度上送 */
            motor_fun.fun_type = eMotor_Fun_Sample_Start; /* 开始测试 */
            motor_Emit(&motor_fun, 0);                    /* 提交到电机队列 */
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
        case eProtocolEmitPack_Client_CMD_READ_ID: /* ID卡读取命令帧 0x06 */
                                                   /* TO DO */
            break;
        case eProtocolEmitPack_Client_CMD_UPGRADE: /* 下位机升级命令帧 0x0F */
                                                   /* TO DO */
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
    if (pInBuff[5] == eProtocoleRespPack_Client_ACK) { /* 收到对方回应帧 */
        comm_Main_Send_ACK_Notify(pInBuff[6]);         /* 通知串口发送任务 回应包收到 */
        return PROTOCOL_PARSE_OK;                      /* 直接返回 */
    }

    /* 其他命令帧 */
    return protocol_Parse_Emit(eComm_Main, pInBuff, length); /* 后续详细处理 */
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
    uint8_t i;

    if (pInBuff[5] == eProtocoleRespPack_Client_ACK) { /* 收到对方回应帧 */
        comm_Data_Send_ACK_Notify(pInBuff[6]);         /* 通知串口发送任务 回应包收到 */
        return PROTOCOL_PARSE_OK;                      /* 直接返回 */
    }

    error = protocol_Parse_AnswerACK(eComm_Data, pInBuff[3]); /* 发送回应包 */
    switch (pInBuff[5]) {                                     /* 进一步处理 功能码 */
        case eComm_Data_Inbound_CMD_DATA:                     /* 采集数据帧 */
            gComm_Data_Sample_Buffer.num = pInBuff[6];        /* 数据个数 u16 */
            gComm_Data_Sample_Buffer.channel = pInBuff[7];    /* 通道编码 */
            for (i = 0; i < pInBuff[6]; ++i) {                /* 具体数据 */
                gComm_Data_Sample_Buffer.data[i] = pInBuff[8 + (i * 2)] + (pInBuff[9 + (i * 2)] << 8);
            }
            comm_Out_SendTask_QueueEmitWithBuild(eProtocoleRespPack_Client_SAMP_DATA, &pInBuff[6], (gComm_Data_Sample_Buffer.num * 2) + 2, 20); /* 调试输出 */
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

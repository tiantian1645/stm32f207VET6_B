/**
 * @file    se2707.c
 * @brief   se2707 扫码枪通讯协议
 * @example
 *      buffer[10] = {0x04, 0xA3, 0x04, 0x00};
 *
 *      checksum = se2707_checksum_gen(buffer, 4);
 *      buffer[4] = checksum >> 8;
 *      buffer[5] = checksum & 0xFF;
 *
 *      se2707_checksum_check(buffer, 2) == 2;
 *      se2707_checksum_check(buffer, 6) == 0;
 *      buffer[5] = 0x23;
 *      se2707_checksum_check(buffer, 6) == 1;
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "se2707.h"

/* Extern variables ----------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/
/* CMD list */
typedef enum {
    ABORT_MACRO_PDF = 0x11,
    AIM_OFF = 0xC4,
    AIM_ON = 0xC5,
    BATCH_DATA = 0xD6,
    BATCH_REQUEST = 0xD5,
    BEEP = 0xE6,
    CAPABILITIES_REQUEST = 0xD3,
    CAPABILITIES_REPLY = 0xD4,
    CHANGE_ALL_CODE_TYPES = 0xC9,
    CMD_ACK = 0xD0,
    CMD_ACK_ACTION = 0xD8,
    CMD_NAK = 0xD1,
    CUSTOM_DEFAULTS = 0x12,
    DECODE_DATA = 0xF3,
    EVENT = 0xF6,
    FLUSH_MACRO_PDF = 0x10,
    FLUSH_QUEUE = 0xD2,
    ILLUMINATION_OFF = 0xC0,
    ILLUMINATION_ON = 0xC1,
    IMAGE_DATA = 0xB1,
    IMAGER_MODE = 0xF7,
    LED_OFF = 0xE8,
    LED_ON = 0xE7,
    PAGER_MOTOR_ACTIVATION = 0xCA,
    PARAM_DEFAULTS = 0xC8,
    PARAM_REQUEST = 0xC7,
    PARAM_SEND = 0xC6,
    REPLY_REVISION = 0xA4,
    REQUEST_REVISION = 0xA3,
    SCAN_DISABLE = 0xEA,
    SCAN_ENABLE = 0xE9,
    SLEEP = 0xEB,
    SSI_MGMT_COMMAND = 0x80,
    START_SESSION = 0xE4,
    STOP_SESSION = 0xE5,
    VIDEO_DATA = 0xB4,
    WAKEUP = 0,
} eSE2707_CMD;

/* Private define ------------------------------------------------------------*/
/* 2进制补码配置 */
#define SE2707_COMP_LENGTH 16
#define SE2707_COMP_MANDAN (1 << SE2707_COMP_LENGTH)

/* 延时函数定义 */
#define SE2707_DELAY HAL_Delay /* vTaskDelay */

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Private constants ---------------------------------------------------------*/
const uint8_t cSe2707_ACK_PACK[] = {0x04, 0xD0, 0x00, 0x00, 0xFF, 0x2C};

/* Private function prototypes -----------------------------------------------*/

/* Private user code ---------------------------------------------------------*/

/**
 * @brief  2进制补码 校验码生成函数 04 A3 04 00 --> FF 55
 * @param  pData 数据指针
 * @param  length 数据长度 扫码枪中数据长度最大为255 全长超过256
 * @retval 2进制补码
 */
uint16_t se2707_checksum_gen(uint8_t * pData, uint16_t length)
{
    uint16_t sum = 0;

    if (length == 0) {
        return 0;
    }

    while (length-- > 0) {
        sum += *pData++;
    }
    return SE2707_COMP_MANDAN - sum;
}

/**
 * @brief  2进制补码 校验码校验函数 04 A3 04 00 FF 55 --> 0 04 A3 04 00 FF 55 66 --> 1
 * @param  pData 数据指针
 * @param  length 数据长度 扫码枪中数据长度最大为255 全长超过256
 * @retval 0 无误 1 错误
 */
uint8_t se2707_checksum_check(uint8_t * pData, uint16_t length)
{
    uint16_t sum = 0;

    if (length <= 2) {
        return 2;
    }

    while (length-- > 2) {
        sum += *pData++;
    }

    sum += (*pData++) << 8;
    sum += *pData++;
    if (sum == 0) {
        return 0;
    }
    return 1;
}

/**
 * @brief  se2707 ssi 协议组包
 * @param  cmd 命令字 eSE2707_CMD
 * @param  status 状态字
 * @param  pPayload 实际负载区指针
 * @param  payload_length 实际负载区长度
 * @param  pResult 结果存放指针
 * @retval 结果长度
 */
uint16_t se2707_build_pack(eSE2707_CMD cmd, eSE2707_Set_Param_status status, uint8_t * pPayload, uint8_t payload_length, uint8_t * pResult)
{
    uint16_t checksum;

    if (pPayload == NULL && payload_length > 0) {
        return 0;
    }

    pResult[0] = payload_length + 4;
    pResult[1] = (uint8_t)(cmd);
    pResult[2] = 0x04;
    pResult[3] = status;
    if (payload_length > 0) {
        memcpy(&pResult[4], pPayload, payload_length);
    }
    checksum = se2707_checksum_gen(pResult, pResult[0]);
    pResult[pResult[0] + 0] = checksum >> 8;
    pResult[pResult[0] + 1] = checksum & 0xFF;
    return pResult[0] + 2;
}

/**
 * @brief  se2707 ssi ACK 组包
 * @param  pResult 结果存放指针
 * @retval 结果长度
 */
uint16_t se2707_build_pack_ack(uint8_t * pResult)
{
    return se2707_build_pack(CMD_ACK, Set_Param_Tempory, NULL, 0, pResult);
}

/**
 * @brief  se2707 ssi 查询参数组包
 * @param  param_type 参数类型
 * @param  pResult 结果存放指针
 * @retval 结果长度
 */
uint16_t se2707_build_pack_beep_conf(uint8_t beep_code, eSE2707_Set_Param_status status, uint8_t * pResult)
{
    uint8_t payload[1];

    payload[0] = beep_code;
    return se2707_build_pack(BEEP, status, payload, 1, pResult);
}

/**
 * @brief  se2707 ssi 查询参数组包
 * @param  param_type 参数类型
 * @param  pResult 结果存放指针
 * @retval 结果长度
 */
uint16_t se2707_build_pack_param_read(uint16_t param_type, uint8_t * pResult)
{
    uint8_t payload[2];

    payload[0] = param_type >> 8;
    payload[1] = param_type & 0xFF;
    return se2707_build_pack(PARAM_REQUEST, Set_Param_Tempory, payload, 2, pResult);
}

/**
 * @brief  se2707 ssi 设置参数组包
 * @param  param_type 参数类型
 * @param  status 永久或临时设置
 * @param  conf  参数值
 * @param  pResult 结果存放指针
 * @retval 结果长度
 */
uint16_t se2707_build_pack_param_write(uint16_t param_type, eSE2707_Set_Param_status status, uint8_t conf, uint8_t * pResult)
{
    uint8_t payload[4];

    payload[0] = 0xFF;
    payload[1] = param_type >> 8;
    payload[2] = param_type & 0xFF;
    payload[3] = conf;
    return se2707_build_pack(PARAM_SEND, status, payload, 4, pResult);
}

/**
 * @brief  se2707 ssi 恢复默认参数组包
 * @param  pResult 结果存放指针
 * @retval 结果长度
 */
uint16_t se2707_build_pack_reset_Default(uint8_t * pResult)
{
    return se2707_build_pack(PARAM_DEFAULTS, Set_Param_Tempory, NULL, 0, pResult);
}

/**
 * @brief  se2707 ssi 发送操作
 * @param  puart 串口句柄指针
 * @param  pData length 数组描述
 * @retval 0 发送正常 1 发送异常
 * @note   先发 0x00 唤醒 发完还要等待一段时间
 */
uint16_t se2707_send_pack(UART_HandleTypeDef * puart, uint8_t * pData, uint16_t length)
{
    uint8_t nn[1] = {0};

    if (HAL_UART_Transmit(puart, nn, 1, 10) != HAL_OK) {
        return 1;
    }
    SE2707_DELAY(20);

    if (HAL_UART_Transmit(puart, nn, 1, 10) != HAL_OK) {
        return 1;
    }
    SE2707_DELAY(40);

    if (HAL_UART_Transmit(puart, pData, length, 10) != HAL_OK) {
        return 1;
    }
    return 0;
}

/**
 * @brief  se2707 ssi 发送操作
 * @param  puart 串口句柄指针
 * @param  pData length 数组描述
 * @param  timeout 接收超时时间
 * @retval 接收长度
 */
uint16_t se2707_recv_pack(UART_HandleTypeDef * puart, uint8_t * pData, uint16_t length, uint32_t timeout)
{
    HAL_StatusTypeDef status;
    uint16_t recv_length;

    status = HAL_UART_Receive(puart, pData, length, timeout);

    recv_length = length - puart->RxXferCount;
    switch (status) {
        case HAL_TIMEOUT: /* 接收超时 */
            recv_length = length - puart->RxXferCount - 1;
            break;
        case HAL_OK: /* 接收到足够字符串 */
            recv_length = length;
            break;
        default:
            recv_length = 0; /* 故障 HAL_ERROR HAL_BUSY */
            break;
    }
    return recv_length;
}

/**
 * @brief  se2707 ssi 检查收到的是不是ack包
 * @param  puart 串口句柄指针
 * @param  pData length 数组描述
 * @retval 0 是 1 数据不正常 2 数据不匹配
 */
uint8_t se2707_check_recv_ack(uint8_t * pData, uint8_t length)
{
    if (length < ARRAY_LEN(cSe2707_ACK_PACK) || pData == NULL) {
        return 1;
    }
    if (memcmp(pData, cSe2707_ACK_PACK, ARRAY_LEN(cSe2707_ACK_PACK)) != 0) {
        return 2;
    }
    return 0;
}

/**
 * @brief  se2707 ssi 从收到的包中提取参数
 * @param  puart 串口句柄指针
 * @param  pData length 数组描述
 * @param  pResult 参数描述
 * @retval 0 解析成功 1 数据不正常 2 数据校验错误
 */
uint8_t se2707_decode_param(uint8_t * pData, uint8_t length, sSE2707_Image_Capture_Param * pResult)
{

    if (length < 10 || pData == NULL || pData[1] != PARAM_SEND) {
        return 1;
    }
    if (se2707_checksum_check(pData, 10) != 0) {
        return 2;
    }
    pResult->param = (pData[5] << 8) + pData[6];
    pResult->data = pData[7];
    return 0;
}

/**
 * @brief  se2707 ssi 配置参数
 * @param  puart 串口句柄指针
 * @param  pResult 参数描述
 * @param  timeout 接收超时时间
 * @param  retry 重试次数 0 时尝试到死
 * @retval 0 解析成功 1 数据不正常 2 数据校验错误
 */
uint8_t se2707_conf_param(UART_HandleTypeDef * puart, sSE2707_Image_Capture_Param * pICP, uint32_t timeout, uint8_t retry)
{
    uint8_t buffer[10], result;
    uint16_t length;

    do {
        length = se2707_build_pack_param_write(pICP->param, Set_Param_Permanent, pICP->data, buffer);
        se2707_send_pack(puart, buffer, length);
        memset(buffer, 0, ARRAY_LEN(buffer));
        length = se2707_recv_pack(puart, buffer, 7, timeout);
        result = se2707_check_recv_ack(buffer, length);
        if (result == 0) {
            break;
        }
        SE2707_DELAY(1000);
    } while (--retry > 0);
    return result;
}

/**
 * @brief  se2707 ssi 检查参数
 * @param  puart 串口句柄指针
 * @param  pResult 参数描述
 * @param  timeout 接收超时时间
 * @param  retry 重试次数 0 时尝试到死
 * @retval 0 解析成功 1 数据不正常 2 数据校验错误
 */
uint8_t se2707_check_param(UART_HandleTypeDef * puart, sSE2707_Image_Capture_Param conf, uint32_t timeout, uint8_t retry)
{
    uint8_t buffer[10], result;
    uint16_t length;
    sSE2707_Image_Capture_Param icp;

    do {
        length = se2707_build_pack_param_read(conf.param, buffer);
        se2707_send_pack(puart, buffer, length);
        memset(buffer, 0, ARRAY_LEN(buffer));
        length = se2707_recv_pack(puart, buffer, 10, timeout);
        result = se2707_decode_param(buffer, length, &icp);
        if (result != 0) {
            SE2707_DELAY(1000);
            continue;
        }
        if (icp.param != conf.param) {
            result = 3;
        }
        if (icp.data != conf.data) {
            result = 4;
        }
        break;
        SE2707_DELAY(1000);
    } while (--retry > 0);
    return result;
}

/**
 * @brief  se2707 ssi 重置默认参数
 * @param  puart 串口句柄指针
 * @param  timeout 接收超时时间
 * @param  retry 重试次数 0 时尝试到死
 * @retval 0 重置成功 1 数据不正常 2 数据校验错误
 */
uint8_t se2707_reset_param(UART_HandleTypeDef * puart, uint32_t timeout, uint8_t retry)
{
    uint8_t buffer[10], result;
    uint16_t length;

    do {
        length = se2707_build_pack_reset_Default(buffer);
        se2707_send_pack(puart, buffer, length);
        memset(buffer, 0, ARRAY_LEN(buffer));
        length = se2707_recv_pack(puart, buffer, 6, timeout); /* ack 报文长度为6 */
        result = se2707_check_recv_ack(buffer, length);
        if (result == 0) {
            break;
        }
        se2707_recv_pack(puart, buffer, ARRAY_LEN(buffer), 1000); /* nak 报文长度为7清除串口缓冲 */
    } while (--retry > 0);
    return result;
}

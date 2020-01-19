// https://github.com/MaJerle/STM32_USART_DMA_RX
// https://github.com/akospasztor/stm32-dma-uart
// https://stackoverflow.com/questions/43298708/stm32-implementing-uart-in-dma-mode
// https://github.com/kiltum/modbus
// https://www.cnblogs.com/pingwen/p/8416608.html
// https://os.mbed.com/handbook/CMSIS-RTOS

#include "main.h"
#include "serial.h"
#include "comm_out.h"
#include "comm_main.h"
#include "comm_data.h"

/* Extern variables ----------------------------------------------------------*/
extern UART_HandleTypeDef huart5; /* 外串口 */
extern UART_HandleTypeDef huart1; /* 上位机 */
extern UART_HandleTypeDef huart2; /* 采样板 */

/* Private define ------------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
static EventGroupHandle_t serial_source_flags = NULL; /* 串口资源标志 */

/* Private constants ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/* Private user code ---------------------------------------------------------*/

/**
 * @brief  串口全局资源初始化
 * @param  None
 * @retval None
 */
void SerialInit(void)
{
    serial_source_flags = xEventGroupCreate();
    if (serial_source_flags == NULL) {
        FL_Error_Handler(__FILE__, __LINE__);
        return;
    }
    serialSourceFlagsSet(eSerial_Source_COMM_Out_Send_Buffer_Bit + eSerial_Source_COMM_Out_Send_Buffer_ISR_Bit + eSerial_Source_COMM_Main_Send_Buffer_Bit +
                         eSerial_Source_COMM_Main_Send_Buffer_ISR_Bit + eSerial_Source_COMM_Data_Send_Buffer_Bit +
                         eSerial_Source_COMM_Data_Send_Buffer_ISR_Bit);
}

/**
 * @brief  串口全局资源标志位状态获取
 * @param  None
 * @retval 标志位状态
 */
EventBits_t serialSourceFlagsGet(void)
{
    return xEventGroupGetBits(serial_source_flags);
}

/**
 * @brief  串口全局资源标志位状态获取 中断版本
 * @param  None
 * @retval 标志位状态
 */
EventBits_t serialSourceFlagsGet_FromISR(void)
{
    return xEventGroupGetBitsFromISR(serial_source_flags);
}

/**
 * @brief  串口全局资源标志位等待
 * @param  flag_bits 等待位
 * @param  timeout 超时时间
 * @retval 等待结果
 */
EventBits_t serialSourceFlagsWait(EventBits_t flag_bits, uint32_t timeout)
{
    return xEventGroupWaitBits(serial_source_flags, flag_bits, pdTRUE, pdTRUE, timeout);
}

/**
 * @brief  串口全局资源标志位等待 中断版本
 * @param  flag_bits 等待位
 * @param  timeout 超时时间
 * @retval 标志位状态
 */
EventBits_t serialSourceFlagsSet_FromISR(EventBits_t flag_bits)
{
    return xEventGroupSetBitsFromISR(serial_source_flags, flag_bits, NULL);
}

/**
 * @brief  串口全局资源标志位等待
 * @param  flag_bits 等待位
 * @param  timeout 超时时间
 * @retval 标志位状态
 */
EventBits_t serialSourceFlagsSet(EventBits_t flag_bits)
{
    return xEventGroupSetBits(serial_source_flags, flag_bits);
}

/**
 * @brief  串口全局资源标志位等待 中断版本
 * @param  flag_bits 等待位
 * @param  timeout 超时时间
 * @retval 标志位状态
 */
EventBits_t serialSourceFlagsClear_FromISR(EventBits_t flag_bits)
{
    return xEventGroupClearBitsFromISR(serial_source_flags, flag_bits);
}

/**
 * @brief  串口全局资源标志位等待
 * @param  flag_bits 等待位
 * @param  timeout 超时时间
 * @retval 标志位状态
 */
EventBits_t serialSourceFlagsClear(EventBits_t flag_bits)
{
    return xEventGroupClearBits(serial_source_flags, flag_bits);
}

/**
 * @brief  接收细节处理
 * @param  huart 串口信息
 * @param  pDMA_Record DMA接收信息
 * @param  phdma   DMA句柄
 * @param  psrd    串口上层处理信息
 * @retval None
 */
void serialGenerateDealRecv(UART_HandleTypeDef * huart, sDMA_Record * pDMA_Record, DMA_HandleTypeDef * phdma, sSerialRecord * psrd)
{
    /* Calculate current position in buffer */
    pDMA_Record->curPos = pDMA_Record->buffLength - __HAL_DMA_GET_COUNTER(phdma);
    if (pDMA_Record->curPos != pDMA_Record->oldPos) {    /* Check change in received data */
        if (pDMA_Record->curPos > pDMA_Record->oldPos) { /* Current position is over previous one */
            /* We are in "linear" mode */
            /* Process data directly by subtracting "pointers" */
            pDMA_Record->callback(&pDMA_Record->pDMA_Buff[pDMA_Record->oldPos], pDMA_Record->curPos - pDMA_Record->oldPos, psrd);
        } else {
            /* We are in "overflow" mode */
            /* First process data to the end of buffer */
            pDMA_Record->callback(&pDMA_Record->pDMA_Buff[pDMA_Record->oldPos], pDMA_Record->buffLength - pDMA_Record->oldPos, psrd);
            /* Check and continue with beginning of buffer */
            if (pDMA_Record->curPos > 0) {
                pDMA_Record->callback(&pDMA_Record->pDMA_Buff[0], pDMA_Record->curPos, psrd);
            }
        }
    }
    pDMA_Record->oldPos = pDMA_Record->curPos; /* Save current position as old */

    /* Check and manually update if we reached end of buffer */
    if (pDMA_Record->oldPos == pDMA_Record->buffLength) {
        pDMA_Record->oldPos = 0;
    }
}

/**
 * @brief  拼包并提交到任务接收队列
 * @param  huart 串口信息
 * @param  pDMA_Record DMA接收信息
 * @retval None
 */
void serialGenerateCallback(uint8_t * pBuff, uint16_t length, sSerialRecord * psrd)
{
    uint16_t i, pos = 0, tail = 0, j;

    if (psrd->validLength + length >= psrd->maxLength) {                      /* 串口接收缓冲空闲长度不足抛弃 */
        if (psrd->has_head(pBuff, length) && psrd->has_tail(pBuff, length)) { /* 新鲜包为有头有尾整包 */
            memcpy(psrd->pSerialBuff, pBuff, length);                         /* 新鲜整包作为未处理包 */
            psrd->validLength = length;                                       /* 更新未处理数据长度 */
        } else {
            for (i = 0; i < psrd->maxLength - length; ++i) {
                psrd->pSerialBuff[0 + i] = psrd->pSerialBuff[length + i]; /* 剔除旧包 */
            }
            memcpy(&psrd->pSerialBuff[psrd->maxLength - length], pBuff, length); /* 拼接到尾部 */
            psrd->validLength = psrd->maxLength;                                 /* 更新未处理数据长度 */
        }
    } else {
        memcpy(&psrd->pSerialBuff[psrd->validLength], pBuff, length); /* 拼接到尾部 */
        psrd->validLength += length;                                  /* 更新未处理数据长度 */
    }

    if (psrd->validLength < psrd->minLength) { /* 报文长度不足 */
        return;
    }

    for (i = 0; i < psrd->validLength - psrd->minLength + 1; ++i) {
        if (psrd->has_head(&(psrd->pSerialBuff[i]), psrd->validLength - i)) {      /* 寻找包头 */
            tail = psrd->has_tail(&(psrd->pSerialBuff[i]), psrd->validLength - i); /* 找到包头后 寻找包尾 */
            if (tail > 0 && psrd->is_cmop(&(psrd->pSerialBuff[i]), tail)) {        /* 完整性判断 */
                pos = i + tail;                                                    /* 记录处理位置 */
                if (tail > 0) {                                                    /* 完整一包 */
                    psrd->callback(&psrd->pSerialBuff[i], tail);                   /* 直接中断内处理 */
                    i += tail - 1;                                                 /* 更新起始测试值 */
                    continue;                                                      /* 新一轮检测  */
                }
            } else {                                                          /* 有包头但数据不正确 */
                if (tail > 0 && tail + psrd->minLength < psrd->validLength) { /* 剩余长度大于最小长度 */
                    i += tail - 1;                                            /* 更新起始测试值 */
                    continue;                                                 /* 新一轮检测  */
                } else {
                    pos = i; /* 记录处理位置 */
                    break;   /* 提前结束检测保留数据 */
                }
            }
        } else {
            pos = i; /* 记录处理位置  */
        }
    }

    psrd->validLength -= pos; /* 更新未处理数据长度 */
    for (j = 0; j < psrd->validLength; ++j) {
        psrd->pSerialBuff[j] = psrd->pSerialBuff[j + pos]; /* 残余数据处理 */
    }
    return;
}

/**
 * @brief  DMA接收一半中断回调
 * @param  argument: Not used
 * @retval None
 */
void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef * huart)
{
    switch ((uint32_t)(huart->Instance)) {
        case (uint32_t)USART1:
            comm_Main_IRQ_RX_Deal(huart);
            break;
        case (uint32_t)USART2:
            comm_Data_IRQ_RX_Deal(huart);
            break;
        case (uint32_t)UART5:
            comm_Out_IRQ_RX_Deal(huart);
            break;
    }
}

/**
 * @brief  DMA接收完满中断回调
 * @param  argument: Not used
 * @retval None
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef * huart)
{
    HAL_UART_RxHalfCpltCallback(huart);
}

/**
 * @brief  串口异常处理
 * @param  huart 串口句柄
 * @retval None
 * @note   https://community.st.com/s/question/0D50X00009XkfN8SAJ/restore-circular-dma-rx-after-uart-error
 * Heath Raftery (Community Member)
 *
 * Edited by STM Community July 21, 2018 at 5:37 PM
 * Posted on June 01, 2018 at 11:04
 *
 *
 *
 *
 * We also have a 'standard' CubeMX circular DMA UART setup, which stops on error. Caused us some headaches, but ultimately able to resolve it thusly:
 *
 * In DMA mode, a comms error actually triggers a DMA end of transfer interrupt (not a UART error). The DMA HAL picks this up, aborts the DMA and then calls the
 * registered
 *
 * XferAbortCallback
 *
 * (see
 * stm32f4xx_hal_dma.c:883
 *
 * ) which is actually
 * UART_DMAAbortOnError
 *
 * . That then calls
 * HAL_UART_ErrorCallback
 *
 * , so once that callback runs the DMA is already aborted. Further, it turns out only a deliberate call to
 * HAL_UART_Abort_IT
 *
 * ends up calling
 * HAL_UART_AbortCpltCallback
 *
 * , so in DMA mode UART DMA restoration needs to happen in
 * HAL_UART_ErrorCallback
 *
 * .
 * In the end, all we did is call
 *
 * HAL_UART_Receive_DMA()
 *
 * from
 * HAL_UART_ErrorCallback
 *
 * (once the error is logged/otherwise dealt with). No init/de-init, flag clearing or error suppressing necessary.
 * To the OP: is it possible you missed the simplest one-statement option in your efforts?
 *
 * Anyone else solved it otherwise?
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef * huart)
{
    switch ((uint32_t)(huart->Instance)) {
        case (uint32_t)USART1:
            error_Emit_FromISR(eError_Comm_Main_UART);
            comm_Main_DMA_RX_Restore();
            break;
        case (uint32_t)USART2:
            error_Emit_FromISR(eError_Comm_Data_UART);
            comm_Data_DMA_RX_Restore();
            break;
        case (uint32_t)UART5:
            error_Emit_FromISR(eError_Comm_Out_UART);
            comm_Out_DMA_RX_Restore();
            break;
    }
}

/**
 * @brief  DMA发送完成中断回调
 * @param  argument: Not used
 * @retval None
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef * huart)
{
    switch ((uint32_t)(huart->Instance)) {
        case (uint32_t)(USART1):
            comm_Main_DMA_TX_CallBack();
            break;
        case (uint32_t)(USART2):
            comm_Data_DMA_TX_CallBack();
            break;
        case (uint32_t)(UART5):
            comm_Out_DMA_TX_CallBack();
            break;
    }
}

/**
 * @brief  串口发送启动 DMA 方式
 * @param  None
 * @retval None
 */
BaseType_t serialSendStartDMA(eSerialIndex serialIndex, uint8_t * pSendBuff, uint8_t sendLength, uint32_t timeout)
{
    HAL_StatusTypeDef result;

    if (sendLength == 0 || pSendBuff == NULL) {
        return pdFALSE;
    }

    switch (serialIndex) {
        case eSerialIndex_1:
            if (comm_Main_DMA_TX_Enter(timeout) != pdPASS) { /* 确保发送完成信号量被释放 */
                return pdFALSE;                              /* 115200波特率下 发送长度少于 256B 长度数据包耗时超过 30mS */
            }
            result = HAL_UART_Transmit_DMA(&huart1, pSendBuff, sendLength); /* 执行DMA发送 */
            if (result != HAL_OK) {                                         /* 发送结果判断 异常反应 HAL_BUSY */
                comm_Main_DMA_TX_Error();                                   /* 发送失败处理 */
                return pdFALSE;
            } else {
                return pdTRUE;
            }
            break;
        case eSerialIndex_2:
            if (comm_Data_DMA_TX_Enter(timeout) != pdPASS) { /* 确保发送完成信号量被释放 */
                return pdFALSE;                              /* 115200波特率下 发送长度少于 256B 长度数据包耗时超过 30mS */
            }
            result = HAL_UART_Transmit_DMA(&huart2, pSendBuff, sendLength); /* 执行DMA发送 */
            if (result != HAL_OK) {                                         /* 发送结果判断 异常反应 HAL_BUSY */
                comm_Data_DMA_TX_Error();                                   /* 发送失败处理 */
                return pdFALSE;
            } else {
                return pdTRUE;
            }
            break;
        case eSerialIndex_5:
            if (comm_Out_DMA_TX_Enter(timeout) != pdPASS) { /* 确保发送完成信号量被释放 */
                return pdFALSE;                             /* 115200波特率下 发送长度少于 256B 长度数据包耗时超过 30mS */
            }
            result = HAL_UART_Transmit_DMA(&huart5, pSendBuff, sendLength); /* 执行DMA发送 */
            if (result != HAL_OK) {                                         /* 发送结果判断 异常反应 HAL_BUSY */
                comm_Out_DMA_TX_Error();                                    /* 发送失败处理 */
                return pdFALSE;
            } else {
                return pdTRUE;
            }
            break;
        default:
            return pdFALSE;
    }
    return result;
}

/**
 * @brief  串口发送启动 查询方式
 * @param  None
 * @retval None
 */
BaseType_t serialSendStart(eSerialIndex serialIndex, uint8_t * pSendBuff, uint8_t sendLength, uint32_t timeout)
{
    BaseType_t result;

    switch (serialIndex) {
        case eSerialIndex_1:
            result = (HAL_UART_Transmit(&huart1, pSendBuff, sendLength, timeout) == HAL_OK) ? (pdTRUE) : (pdFALSE);
            break;
        case eSerialIndex_2:
            result = (HAL_UART_Transmit(&huart2, pSendBuff, sendLength, timeout) == HAL_OK) ? (pdTRUE) : (pdFALSE);
            break;
        case eSerialIndex_5:
            result = (HAL_UART_Transmit(&huart5, pSendBuff, sendLength, timeout) == HAL_OK) ? (pdTRUE) : (pdFALSE);
            break;
        default:
            return pdFALSE;
    }
    return result;
}

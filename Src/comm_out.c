// https://github.com/MaJerle/STM32_USART_DMA_RX
// https://github.com/akospasztor/stm32-dma-uart
// https://stackoverflow.com/questions/43298708/stm32-implementing-uart-in-dma-mode
// https://github.com/kiltum/modbus
// https://www.cnblogs.com/pingwen/p/8416608.html
// https://os.mbed.com/handbook/CMSIS-RTOS

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "serial.h"
#include "protocol.h"
#include "stdio.h"
#include "comm_out.h"
#include "m_l6470.h"
#include "barcode_scan.h"
#include "tray_run.h"
#include "m_drv8824.h"

/* Extern variables ----------------------------------------------------------*/
extern UART_HandleTypeDef huart5;
extern DMA_HandleTypeDef hdma_uart5_rx;

/* Private define ------------------------------------------------------------*/
#define COMM_OUT_SERIAL_INDEX eSerialIndex_5
#define COMM_OUT_UART_HANDLE huart5

/* Private typedef -----------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* DMA 接收缓存操作信息 */
static sDMA_Record vComm_Out_DMA_RX_Conf;

/* 串口接收缓存操作信息 */
static sSerialRecord vComm_Out_Serial_Record;

/* DMA 接收缓存 */
static uint8_t gComm_Out_RX_dma_buffer[COMM_OUT_DMA_RX_SIZE];

/* DMA 接收后 提交到串口拼包缓存 */
static uint8_t gComm_Out_RX_serial_buffer[COMM_OUT_SER_RX_SIZE];

/* 串口接收队列 */
static xQueueHandle comm_Out_RecvQueue = NULL;

/* 串口发送队列 */
static xQueueHandle comm_Out_SendQueue = NULL;

/* 串口DMA发送资源信号量 */
static xSemaphoreHandle comm_Out_Send_Sem = NULL;

/* 串口收发任务句柄 */
static xTaskHandle comm_Out_Recv_Task_Handle = NULL;
static xTaskHandle comm_Out_Send_Task_Handle = NULL;

static sComm_Out_SendInfo gComm_Out_SendInfo;
static uint8_t gComm_Out_SendInfoFlag = 1;

/* Private constants ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
static void comm_Out_Recv_Task(void * argument);
static void comm_Out_Send_Task(void * argument);

static BaseType_t comm_Out_RecvTask_QueueEmit_ISR(uint8_t * pData, uint16_t length);

/* Private user code ---------------------------------------------------------*/

/**
 * @brief  串口拼包记录信息初始化
 * @param  None
 * @retval None
 */
void comm_Out_ConfInit(void)
{
    vComm_Out_DMA_RX_Conf.pDMA_Buff = gComm_Out_RX_dma_buffer;
    vComm_Out_DMA_RX_Conf.buffLength = ARRAY_LEN(gComm_Out_RX_dma_buffer);
    vComm_Out_DMA_RX_Conf.curPos = 0;
    vComm_Out_DMA_RX_Conf.oldPos = 0;
    vComm_Out_DMA_RX_Conf.callback = serialGenerateCallback;

    vComm_Out_Serial_Record.pSerialBuff = gComm_Out_RX_serial_buffer;
    vComm_Out_Serial_Record.maxLength = ARRAY_LEN(gComm_Out_RX_serial_buffer);
    vComm_Out_Serial_Record.minLength = 7;
    vComm_Out_Serial_Record.has_head = protocol_has_head;
    vComm_Out_Serial_Record.has_tail = protocol_has_tail;
    vComm_Out_Serial_Record.is_cmop = protocol_is_comp;
    vComm_Out_Serial_Record.callback = comm_Out_RecvTask_QueueEmit_ISR;
}

/**
 * @brief  串口接收DMA 空闲中断 回调
 * @param  None
 * @retval None
 */
void comm_Out_IRQ_RX_Deal(UART_HandleTypeDef * huart)
{
    serialGenerateDealRecv(huart, &vComm_Out_DMA_RX_Conf, &hdma_uart5_rx, &vComm_Out_Serial_Record); /* 使用拼包模式模板 */
}

/**
 * @brief  串口DMA发送完成回调
 * @param  None
 * @retval None
 */
void comm_Out_DMA_TX_CallBack(void)
{
    BaseType_t xTaskWoken = pdFALSE;
    if (comm_Out_Send_Sem != NULL) {
        xSemaphoreGiveFromISR(comm_Out_Send_Sem, &xTaskWoken); /* DMA 发送完成 */
    } else {
        FL_Error_Handler(__FILE__, __LINE__);
    }
}

/**
 * @brief  串口DMA发送开始前准备
 * @param  timeout 超时时间
 * @retval None
 */
BaseType_t comm_Out_DMA_TX_Enter(uint32_t timeout)
{
    if (xSemaphoreTake(comm_Out_Send_Sem, pdMS_TO_TICKS(timeout)) != pdPASS) { /* 确保发送完成信号量被释放 */
        return pdFALSE;                                                        /* 115200波特率下 发送长度少于 256B 长度数据包耗时超过 30mS */
    }
    return pdPASS;
}

/**
 * @brief  串口DMA发送失败后处理
 * @param  timeout 超时时间
 * @retval None
 */
void comm_Out_DMA_TX_Error(void)
{
    xSemaphoreGive(comm_Out_Send_Sem); /* DMA 发送异常 释放信号量 */
}

/**
 * @brief  串口任务初始化
 * @param  None
 * @retval None
 */
void comm_Out_Init(void)
{
    BaseType_t xResult;

    comm_Out_ConfInit();

    /* 接收队列 */
    comm_Out_RecvQueue = xQueueCreate(3, sizeof(sComm_Out_RecvInfo));
    if (comm_Out_RecvQueue == NULL) {
        FL_Error_Handler(__FILE__, __LINE__);
    }

    /* DMA 发送资源信号量*/
    comm_Out_Send_Sem = xSemaphoreCreateBinary();
    if (comm_Out_Send_Sem == NULL) {
        FL_Error_Handler(__FILE__, __LINE__);
    }
    xSemaphoreGive(comm_Out_Send_Sem);

    /* 发送队列 */
    comm_Out_SendQueue = xQueueCreate(1, sizeof(sComm_Out_SendInfo));
    if (comm_Out_SendQueue == NULL) {
        FL_Error_Handler(__FILE__, __LINE__);
    }

    /* Start DMA */
    if (HAL_UART_Receive_DMA(&COMM_OUT_UART_HANDLE, gComm_Out_RX_dma_buffer, ARRAY_LEN(gComm_Out_RX_dma_buffer)) != HAL_OK) {
        FL_Error_Handler(__FILE__, __LINE__);
    }

    /* 创建串口接收任务 */
    xResult = xTaskCreate(comm_Out_Recv_Task, "CommOutRX", 200, NULL, TASK_PRIORITY_COMM_OUT_RX, &comm_Out_Recv_Task_Handle);
    if (xResult != pdPASS) {
        FL_Error_Handler(__FILE__, __LINE__);
    }
    /* 创建串口发送任务 */
    xResult = xTaskCreate(comm_Out_Send_Task, "CommOutTX", 128, NULL, TASK_PRIORITY_COMM_OUT_TX, &comm_Out_Send_Task_Handle);
    if (xResult != pdPASS) {
        FL_Error_Handler(__FILE__, __LINE__);
    }

    /* 使能串口空闲中断 */
    __HAL_UART_ENABLE_IT(&COMM_OUT_UART_HANDLE, UART_IT_IDLE);
}

/**
 * @brief  加入串口接收队列
 * @param  pData   数据指针
 * @param  length  数据长度
 * @retval 加入发送队列结果
 */
BaseType_t comm_Out_RecvTask_QueueEmit_ISR(uint8_t * pData, uint16_t length)
{
    BaseType_t xWaken = pdFALSE;
    sComm_Out_RecvInfo recvInfo;

    recvInfo.length = length;
    memcpy(recvInfo.buff, pData, length);
    return xQueueSendToBackFromISR(comm_Out_RecvQueue, &recvInfo, &xWaken);
}

/**
 * @brief  加入串口发送队列
 * @param  pData   数据指针
 * @param  length  数据长度
 * @param  timeout 超时时间
 * @retval 加入发送队列结果
 */
BaseType_t comm_Out_SendTask_QueueEmit(uint8_t * pData, uint8_t length, uint32_t timeout)
{
    BaseType_t xResult;
    if (length == 0 || pData == NULL) {
        return pdFALSE;
    }
    if (gComm_Out_SendInfoFlag == 0) {
        return pdFALSE;
    }
    gComm_Out_SendInfoFlag = 0;
    memcpy(gComm_Out_SendInfo.buff, pData, length);
    gComm_Out_SendInfo.length = length;

    xResult = xQueueSendToBack(comm_Out_SendQueue, &gComm_Out_SendInfo, pdMS_TO_TICKS(timeout));
    gComm_Out_SendInfoFlag = 1;
    return xResult;
}

/**
 * @brief  加入串口发送队列
 * @param  pData   数据指针
 * @param  length  数据长度
 * @param  timeout 超时时间
 * @retval 加入发送队列结果
 */
BaseType_t comm_Out_SendTask_QueueEmitWithBuild(uint8_t cmdType, uint8_t * pData, uint8_t length, uint32_t timeout)
{
    BaseType_t xResult;
    length = buildPackOrigin(eComm_Out, cmdType, pData, length);
    xResult = comm_Out_SendTask_QueueEmit(pData, length, timeout);
    return xResult;
}

/**
 * @brief  串口接收回应包收到处理
 * @param  packIndex   回应包中帧号
 * @retval 加入发送队列结果
 */
BaseType_t comm_Out_Send_ACK_Notify(uint8_t packIndex)
{
    return xTaskNotify(comm_Out_Send_Task_Handle, packIndex, eSetValueWithoutOverwrite);
}

/**
 * @brief  串口1接收任务 屏托板上位机
 * @param  argument: 每个串口任务配置结构体指针
 * @retval None
 */
static void comm_Out_Recv_Task(void * argument)
{
    sComm_Out_RecvInfo recvInfo;
    eProtocolParseResult pResult;

    for (;;) {
        if (xQueueReceive(comm_Out_RecvQueue, &recvInfo, pdMS_TO_TICKS(5)) != pdPASS) { /* 检查接收队列 */
            continue;                                                                   /* 队列空 */
        }

        pResult = protocol_Parse_Out(recvInfo.buff, recvInfo.length); /* 数据包协议解析 */
        if (pResult == PROTOCOL_PARSE_OK) {                           /* 数据包解析正常 */
        }
    }
}

/**
 * @brief  串口1发送任务 屏托板上位机
 * @param  argument: 每个串口任务配置结构体指针
 * @retval None
 */
static void comm_Out_Send_Task(void * argument)
{
    sComm_Out_SendInfo sendInfo;
    uint8_t i, ucResult;
    uint32_t ulNotifyValue;

    for (;;) {
        if (uxSemaphoreGetCount(comm_Out_Send_Sem) == 0) { /* DMA发送未完成 此时从接收队列提取数据覆盖发送指针会干扰DMA发送 */
            vTaskDelay(5);
            continue;
        }
        if (xQueuePeek(comm_Out_SendQueue, &sendInfo, pdMS_TO_TICKS(5)) != pdPASS) { /* 发送队列为空 */
            continue;
        }
        ucResult = 0; /* 发送结果初始化 */
        for (i = 0; i < COMM_OUT_SER_TX_RETRY_NUM; ++i) {
            if (serialSendStartDMA(COMM_OUT_SERIAL_INDEX, sendInfo.buff, sendInfo.length, 30) != pdPASS) { /* 启动串口发送 */
                vTaskDelay(pdMS_TO_TICKS(30));                                                             /* 30mS 后重新尝试启动DMA发送 */
                continue;
            }
            if (protocol_is_NeedWaitRACK(sendInfo.buff) != pdTRUE) { /* 判断发送后是否需要等待回应包 */
                ucResult = 2;                                        /* 无需等待回应包默认成功 */
                break;
            }
            if (xTaskNotifyWait(0, 0xFFFFFFFF, &ulNotifyValue, COMM_OUT_SER_TX_RETRY_INT) == pdPASS) { /* 等待任务通知 */
                if (ulNotifyValue == sendInfo.buff[3]) {
                    ucResult = 1; /* 置位发送成功 */
                    break;
                }
            }
        }
        if (ucResult == 0) { /* 重发失败处理 */
//            memset(sendInfo.buff, 0xAA, sendInfo.length);
//            serialSendStartDMA(COMM_OUT_SERIAL_INDEX, sendInfo.buff, sendInfo.length, 30);
        }
        xQueueReceive(comm_Out_SendQueue, &sendInfo, 0); /* 释放发送队列 */
    }
}

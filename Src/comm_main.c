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
#include "comm_main.h"
#include "soft_timer.h"

/* Extern variables ----------------------------------------------------------*/
extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef hdma_usart1_rx;
extern UART_HandleTypeDef huart5;

/* Private define ------------------------------------------------------------*/
#define COMM_MAIN_SERIAL_INDEX eSerialIndex_1
#define COMM_MAIN_UART_HANDLE huart1

#define COMM_MAIN_RECV_QUEU_LENGTH 3
#define COMM_MAIN_SEND_QUEU_LENGTH 12
#define COMM_MAIN_ERROR_SEND_QUEU_LENGTH 16

/* Private typedef -----------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* DMA 接收缓存操作信息 */
static sDMA_Record vComm_Main_DMA_RX_Conf;

/* 串口接收缓存操作信息 */
static sSerialRecord vComm_Main_Serial_Record;

/* DMA 接收缓存 */
static uint8_t gComm_Main_RX_dma_buffer[COMM_MAIN_DMA_RX_SIZE];

/* DMA 接收后 提交到串口拼包缓存 */
static uint8_t gComm_Main_RX_serial_buffer[COMM_MAIN_SER_RX_SIZE];

/* 串口接收队列 */
static xQueueHandle comm_Main_RecvQueue = NULL;

/* 串口发送队列 */
static xQueueHandle comm_Main_SendQueue = NULL;
static xQueueHandle comm_Main_Error_Info_SendQueue = NULL;

/* 串口DMA发送资源信号量 */
static xSemaphoreHandle comm_Main_Send_Sem = NULL;

/* 串口收发任务句柄 */
static xTaskHandle comm_Main_Recv_Task_Handle = NULL;
static xTaskHandle comm_Main_Send_Task_Handle = NULL;

/* 串口接收ACK记录 */
static sProcol_COMM_ACK_Record gComm_Main_ACK_Records[COMM_MAIN_SEND_QUEU_LENGTH];

static sComm_Main_SendInfo gComm_Main_SendInfo; /* 提交发送任务到队列用缓存 */
static uint8_t gComm_Main_SendInfoFlag = 1;     /* 上述缓存使用锁 */
static uint8_t gComm_Main_Connected = 0;        /* 通信正常标志 */

/* Private constants ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
static void comm_Main_Recv_Task(void * argument);
static void comm_Main_Send_Task(void * argument);

static BaseType_t comm_Main_RecvTask_QueueEmit_ISR(uint8_t * pData, uint16_t length);

/* Private user code ---------------------------------------------------------*/

/**
 * @brief  串口拼包记录信息初始化
 * @param  None
 * @retval None
 */
void comm_Main_ConfInit(void)
{
    vComm_Main_DMA_RX_Conf.pDMA_Buff = gComm_Main_RX_dma_buffer;
    vComm_Main_DMA_RX_Conf.buffLength = ARRAY_LEN(gComm_Main_RX_dma_buffer);
    vComm_Main_DMA_RX_Conf.curPos = 0;
    vComm_Main_DMA_RX_Conf.oldPos = 0;
    vComm_Main_DMA_RX_Conf.callback = serialGenerateCallback;

    vComm_Main_Serial_Record.pSerialBuff = gComm_Main_RX_serial_buffer;
    vComm_Main_Serial_Record.maxLength = ARRAY_LEN(gComm_Main_RX_serial_buffer);
    vComm_Main_Serial_Record.minLength = 7;
    vComm_Main_Serial_Record.has_head = protocol_has_head;
    vComm_Main_Serial_Record.has_tail = protocol_has_tail;
    vComm_Main_Serial_Record.is_cmop = protocol_is_comp;
    vComm_Main_Serial_Record.callback = comm_Main_RecvTask_QueueEmit_ISR;
}

/**
 * @brief  串口接收DMA 空闲中断 回调
 * @param  None
 * @retval None
 */
void comm_Main_IRQ_RX_Deal(UART_HandleTypeDef * huart)
{
    serialGenerateDealRecv(huart, &vComm_Main_DMA_RX_Conf, &hdma_usart1_rx, &vComm_Main_Serial_Record); /* 使用拼包模式模板 */
}

/**
 * @brief  串口DMA发送完成回调
 * @param  None
 * @retval None
 */
void comm_Main_DMA_TX_CallBack(void)
{
    BaseType_t xTaskWoken = pdFALSE;
    if (comm_Main_Send_Sem != NULL) {
        xSemaphoreGiveFromISR(comm_Main_Send_Sem, &xTaskWoken); /* DMA 发送完成 */
    } else {
        FL_Error_Handler(__FILE__, __LINE__);
    }
}

/**
 * @brief  串口DMA发送信号量等待
 * @param  timeout 超时时间
 * @retval None
 */
BaseType_t comm_Main_DMA_TX_Wait(uint32_t timeout)
{
    TickType_t tick;

    tick = xTaskGetTickCount();
    do {
        vTaskDelay(pdMS_TO_TICKS(5));
        if (uxSemaphoreGetCount(comm_Main_Send_Sem) > 0) {
            return pdPASS;
        }
    } while (xTaskGetTickCount() - tick < timeout);
    return pdFALSE;
}

/**
 * @brief  串口DMA发送开始前准备
 * @param  timeout 超时时间
 * @retval None
 */
BaseType_t comm_Main_DMA_TX_Enter(uint32_t timeout)
{
    if (xSemaphoreTake(comm_Main_Send_Sem, pdMS_TO_TICKS(timeout)) != pdPASS) { /* 确保发送完成信号量被释放 */
        return pdFALSE; /* 115200波特率下 发送长度少于 256B 长度数据包耗时超过 30mS */
    }
    return pdPASS;
}

/**
 * @brief  串口DMA发送失败后处理
 * @param  timeout 超时时间
 * @retval None
 */
void comm_Main_DMA_TX_Error(void)
{
    xSemaphoreGive(comm_Main_Send_Sem); /* DMA 发送异常 释放信号量 */
}

/**
 * @brief  串口DMA发送接收异常处理
 * @note   https://community.st.com/s/question/0D50X00009XkfN8SAJ/restore-circular-dma-rx-after-uart-error
 * @param  None
 * @retval None
 */
void comm_Main_DMA_RX_Restore(void)
{
    if (HAL_UART_Receive_DMA(&COMM_MAIN_UART_HANDLE, gComm_Main_RX_dma_buffer, ARRAY_LEN(gComm_Main_RX_dma_buffer)) != HAL_OK) {
        FL_Error_Handler(__FILE__, __LINE__);
    }
}

/**
 * @brief  串口任务初始化
 * @param  None
 * @retval None
 */
void comm_Main_Init(void)
{
    BaseType_t xResult;

    comm_Main_ConfInit();

    /* 接收队列 */
    comm_Main_RecvQueue = xQueueCreate(COMM_MAIN_RECV_QUEU_LENGTH, sizeof(sComm_Main_RecvInfo));
    if (comm_Main_RecvQueue == NULL) {
        FL_Error_Handler(__FILE__, __LINE__);
    }

    /* DMA 发送资源信号量*/
    comm_Main_Send_Sem = xSemaphoreCreateBinary();
    if (comm_Main_Send_Sem == NULL) {
        FL_Error_Handler(__FILE__, __LINE__);
    }
    xSemaphoreGive(comm_Main_Send_Sem);

    /* 发送队列 */
    comm_Main_SendQueue = xQueueCreate(COMM_MAIN_SEND_QUEU_LENGTH, sizeof(sComm_Main_SendInfo));
    if (comm_Main_SendQueue == NULL) {
        FL_Error_Handler(__FILE__, __LINE__);
    }
    /* 发送队列 错误信息专用 */
    comm_Main_Error_Info_SendQueue = xQueueCreate(COMM_MAIN_ERROR_SEND_QUEU_LENGTH, sizeof(uint16_t));
    if (comm_Main_Error_Info_SendQueue == NULL) {
        FL_Error_Handler(__FILE__, __LINE__);
    }

    /* Start DMA */
    if (HAL_UART_Receive_DMA(&COMM_MAIN_UART_HANDLE, gComm_Main_RX_dma_buffer, ARRAY_LEN(gComm_Main_RX_dma_buffer)) != HAL_OK) {
        FL_Error_Handler(__FILE__, __LINE__);
    }

    /* 创建串口接收任务 */
    xResult = xTaskCreate(comm_Main_Recv_Task, "CommMainRX", 256, NULL, TASK_PRIORITY_COMM_MAIN_RX, &comm_Main_Recv_Task_Handle);
    if (xResult != pdPASS) {
        FL_Error_Handler(__FILE__, __LINE__);
    }
    /* 创建串口发送任务 */
    xResult = xTaskCreate(comm_Main_Send_Task, "CommMainTX", 192, NULL, TASK_PRIORITY_COMM_MAIN_TX, &comm_Main_Send_Task_Handle);
    if (xResult != pdPASS) {
        FL_Error_Handler(__FILE__, __LINE__);
    }

    /* 使能串口空闲中断 */
    __HAL_UART_ENABLE_IT(&COMM_MAIN_UART_HANDLE, UART_IT_IDLE);
}

/**
 * @brief  加入串口接收队列
 * @param  pData   数据指针
 * @param  length  数据长度
 * @retval 加入发送队列结果
 */
BaseType_t comm_Main_RecvTask_QueueEmit_ISR(uint8_t * pData, uint16_t length)
{
    BaseType_t xWaken = pdFALSE;
    sComm_Main_RecvInfo recvInfo;

    recvInfo.length = length;
    memcpy(recvInfo.buff, pData, length);
    return xQueueSendToBackFromISR(comm_Main_RecvQueue, &recvInfo, &xWaken);
}

/**
 * @brief  串口发送队列 未处理个数
 * @param  Npne
 * @retval 串口发送队列 未处理个数
 */
UBaseType_t comm_Main_SendTask_Queue_GetWaiting(void)
{
    return uxQueueMessagesWaiting(comm_Main_SendQueue);
}

/**
 * @brief  加入串口发送队列
 * @param  pData   数据指针
 * @param  length  数据长度
 * @param  timeout 超时时间
 * @retval 加入发送队列结果
 */
BaseType_t comm_Main_SendTask_QueueEmit(uint8_t * pData, uint8_t length, uint32_t timeout)
{
    BaseType_t xResult;
    if (length == 0 || pData == NULL) {
        return pdFALSE;
    }
    if (gComm_Main_SendInfoFlag == 0) {
        return pdFALSE;
    }
    gComm_Main_SendInfoFlag = 0;
    memcpy(gComm_Main_SendInfo.buff, pData, length);
    gComm_Main_SendInfo.length = length;

    xResult = xQueueSendToBack(comm_Main_SendQueue, &gComm_Main_SendInfo, pdMS_TO_TICKS(timeout));
    gComm_Main_SendInfoFlag = 1;
    return xResult;
}

/**
 * @brief  加入串口发送队列
 * @param  pData   数据指针
 * @param  length  数据长度
 * @param  timeout 超时时间
 * @retval 加入发送队列结果
 */
BaseType_t comm_Main_SendTask_QueueEmitWithBuild(uint8_t cmdType, uint8_t * pData, uint8_t length, uint32_t timeout)
{
    BaseType_t xResult;
    length = buildPackOrigin(eComm_Main, cmdType, pData, length);
    xResult = comm_Main_SendTask_QueueEmit(pData, length, timeout);
    return xResult;
}

/**
 * @brief  加入串口发送队列
 * @param  pData   数据指针
 * @param  length  数据长度
 * @param  timeout 超时时间
 * @retval 加入发送队列结果
 */
BaseType_t comm_Main_SendTask_ErrorInfoQueueEmit(uint16_t * pErrorCode, uint32_t timeout)
{
    BaseType_t xResult;
    xResult = xQueueSendToBack(comm_Main_Error_Info_SendQueue, pErrorCode, pdMS_TO_TICKS(timeout));
    return xResult;
}

uint8_t comm_MainSendTask_ErrorInfo_IsAble(void)
{
    if (gComm_Main_Connected > 0) {
        return 1;
    }
    return 0;
}

void gComm_Main_Connected_Set_Disbale(void)
{
    gComm_Main_Connected = 0;
}

void gComm_Main_Connected_Set_Enable(void)
{
    gComm_Main_Connected = 1;
}

/**
 * @brief  串口接收回应包 帧号接收
 * @param  packIndex   回应包中帧号
 * @retval 加入发送队列结果
 */
BaseType_t comm_Main_Send_ACK_Give(uint8_t packIndex)
{
    static uint8_t idx = 0;
    gComm_Main_ACK_Records[idx].tick = xTaskGetTickCount();
    gComm_Main_ACK_Records[idx].ack_idx = packIndex;
    ++idx;
    if (idx >= ARRAY_LEN(gComm_Main_ACK_Records)) {
        idx = 0;
    }
    return pdPASS;
}

/**
 * @brief  串口接收回应包收到处理
 * @param  packIndex   回应包中帧号
 * @retval 加入发送队列结果
 */
BaseType_t comm_Main_Send_ACK_Wait(uint8_t packIndex, uint32_t timeout)
{
    uint8_t i;
    TickType_t tick;

    tick = xTaskGetTickCount();
    do {
        for (i = 0; i < ARRAY_LEN(gComm_Main_ACK_Records); ++i) {
            if (gComm_Main_ACK_Records[i].ack_idx == packIndex) {
                return pdPASS;
            }
        }
        vTaskDelay(5);
    } while (xTaskGetTickCount() - tick < timeout);
    return pdFALSE;
}

/**
 * @brief  串口1接收任务 屏托板上位机
 * @param  argument: 每个串口任务配置结构体指针
 * @retval None
 */
static void comm_Main_Recv_Task(void * argument)
{
    sComm_Main_RecvInfo recvInfo;

    for (;;) {
        if (xQueueReceive(comm_Main_RecvQueue, &recvInfo, portMAX_DELAY) != pdPASS) { /* 检查接收队列 */
            continue;                                                                 /* 队列空 */
        }

        protocol_Parse_Main(recvInfo.buff, recvInfo.length); /* 数据包协议解析 */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief  串口1发送任务 屏托板上位机
 * @param  argument: 每个串口任务配置结构体指针
 * @retval None
 */
static void comm_Main_Send_Task(void * argument)
{
    sComm_Main_SendInfo sendInfo;
    uint16_t errorCode;
    uint8_t i, ucResult;

    for (;;) {
        if (uxSemaphoreGetCount(comm_Main_Send_Sem) == 0) { /* DMA发送未完成 此时从接收队列提取数据覆盖发送指针会干扰DMA发送 */
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        if (comm_MainSendTask_ErrorInfo_IsAble() && (xQueueReceive(comm_Main_Error_Info_SendQueue, &errorCode, 0) == pdPASS)) { /* 查看错误信息队列 */
            memcpy(sendInfo.buff, (uint8_t *)(&errorCode), 2);                                                                  /* 错误代码 */
            sendInfo.length = buildPackOrigin(eComm_Main, eProtocoleRespPack_Client_ERR, sendInfo.buff, 2);                     /* 构造数据包 */
        } else if (xQueueReceive(comm_Main_SendQueue, &sendInfo, pdMS_TO_TICKS(10)) != pdPASS) {                                /* 发送队列为空 */
            continue;
        }
        ucResult = 0; /* 发送结果初始化 */
        for (i = 0; i < COMM_MAIN_SER_TX_RETRY_NUM; ++i) {
            if (serialSendStartDMA(COMM_MAIN_SERIAL_INDEX, sendInfo.buff, sendInfo.length, 30) != pdPASS) { /* 启动串口发送 */
                vTaskDelay(pdMS_TO_TICKS(30));                                                              /* 30mS 后重新尝试启动DMA发送 */
                continue;
            }
            if (protocol_is_NeedWaitRACK(sendInfo.buff) != pdTRUE) { /* 判断发送后是否需要等待回应包 */
                ucResult = 2;                                        /* 无需等待回应包默认成功 */
                break;
            }
            comm_Main_DMA_TX_Wait(30);                                                             /* 等待发送完成 */
            if (comm_Main_Send_ACK_Wait(sendInfo.buff[3], COMM_MAIN_SER_TX_RETRY_INT) == pdPASS) { /* 等待ACK回应包 */
                ucResult = 1;                                                                      /* 置位发送成功 */
                break;
            } else {
                ucResult = 0;
            }
        }
        if (ucResult == 0) {                                         /* 重发失败处理 */
            protocol_Temp_Upload_Comm_Set(eComm_Main, 0);            /* 关闭本串口温度上送 */
            while (xQueueReceive(comm_Main_SendQueue, &sendInfo, 0)) /* 清空发送队列 */
                ;
            gComm_Main_Connected_Set_Disbale(); /* 标记通信失败 */
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

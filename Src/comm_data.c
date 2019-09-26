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
#include "comm_data.h"

/* Extern variables ----------------------------------------------------------*/
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart5;
extern DMA_HandleTypeDef hdma_usart2_rx;
extern TIM_HandleTypeDef htim6;

/* Private define ------------------------------------------------------------*/
#define COMM_DATA_SERIAL_INDEX eSerialIndex_2
#define COMM_DATA_UART_HANDLE huart2
#define COMM_DATA_TIM_PD htim6

/* Private typedef -----------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* DMA 接收缓存操作信息 */
static sDMA_Record vComm_Data_DMA_RX_Conf;

/* 串口接收缓存操作信息 */
static sSerialRecord vComm_Data_Serial_Record;

/* DMA 接收缓存 */
static uint8_t gComm_Data_RX_dma_buffer[COMM_DATA_DMA_RX_SIZE];

/* DMA 接收后 提交到串口拼包缓存 */
static uint8_t gComm_Data_RX_serial_buffer[COMM_DATA_SER_RX_SIZE];

/* 串口接收队列 */
static xQueueHandle comm_Data_RecvQueue = NULL;

/* 串口发送队列 */
static xQueueHandle comm_Data_SendQueue = NULL;

/* 串口DMA发送资源信号量 */
static xSemaphoreHandle comm_Data_Send_Sem = NULL;

/* 串口收发任务句柄 */
static xTaskHandle comm_Data_Recv_Task_Handle = NULL;
static xTaskHandle comm_Data_Send_Task_Handle = NULL;

static sComm_Data_SendInfo gComm_Data_SendInfo;
static uint8_t gComm_Data_SendInfoFlag = 1;   /* 加入发送队列缓存修改重入标志 */
static uint8_t gComm_Data_RecvConfirm = 0x5A; /* 启动定时发送后 接收到确认采样完成标志 */
static uint8_t gComm_Data_TIM_StartFlag = 0;
static sComm_Data_Sample_Conf_Unit gComm_Data_Sample_Confs[6];

/* Private constants ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
static void comm_Data_Recv_Task(void * argument);
static void comm_Data_Send_Task(void * argument);

static BaseType_t comm_Data_RecvTask_QueueEmit_ISR(uint8_t * pData, uint16_t length);

/* Private user code ---------------------------------------------------------*/

/**
 * @brief  采样完成接收标志 获取
 * @param  None
 * @retval 采样完成接收标志
 */
uint8_t gComm_Data_RecvConfirm_Get(void)
{
    return gComm_Data_RecvConfirm;
}

/**
 * @brief  采样完成接收标志 设置
 * @param  data    数据
 * @retval None
 */
void gComm_Data_RecvConfirm_Set(uint8_t data)
{
    gComm_Data_RecvConfirm = data;
}

/**
 * @brief  串口拼包记录信息初始化
 * @param  None
 * @retval None
 */
void comm_Data_ConfInit(void)
{
    vComm_Data_DMA_RX_Conf.pDMA_Buff = gComm_Data_RX_dma_buffer;
    vComm_Data_DMA_RX_Conf.buffLength = ARRAY_LEN(gComm_Data_RX_dma_buffer);
    vComm_Data_DMA_RX_Conf.curPos = 0;
    vComm_Data_DMA_RX_Conf.oldPos = 0;
    vComm_Data_DMA_RX_Conf.callback = serialGenerateCallback;

    vComm_Data_Serial_Record.pSerialBuff = gComm_Data_RX_serial_buffer;
    vComm_Data_Serial_Record.maxLength = ARRAY_LEN(gComm_Data_RX_serial_buffer);
    vComm_Data_Serial_Record.minLength = 7;
    vComm_Data_Serial_Record.has_head = protocol_has_head;
    vComm_Data_Serial_Record.has_tail = protocol_has_tail;
    vComm_Data_Serial_Record.is_cmop = protocol_is_comp;
    vComm_Data_Serial_Record.callback = comm_Data_RecvTask_QueueEmit_ISR;
}

/**
 * @brief  串口接收DMA 空闲中断 回调
 * @param  None
 * @retval None
 */
void comm_Data_IRQ_RX_Deal(UART_HandleTypeDef * huart)
{
    serialGenerateDealRecv(huart, &vComm_Data_DMA_RX_Conf, &hdma_usart2_rx, &vComm_Data_Serial_Record); /* 使用拼包模式模板 */
}

/**
 * @brief  串口DMA发送完成回调
 * @param  None
 * @retval None
 */
void comm_Data_DMA_TX_CallBack(void)
{
    BaseType_t xTaskWoken = pdFALSE;
    if (comm_Data_Send_Sem != NULL) {
        xSemaphoreGiveFromISR(comm_Data_Send_Sem, &xTaskWoken); /* DMA 发送完成 */
    } else {
        FL_Error_Handler(__FILE__, __LINE__);
    }
}

/**
 * @brief  串口DMA发送开始前准备
 * @param  timeout 超时时间
 * @retval None
 */
BaseType_t comm_Data_DMA_TX_Enter(uint32_t timeout)
{
    if (xSemaphoreTake(comm_Data_Send_Sem, pdMS_TO_TICKS(timeout)) != pdPASS) { /* 确保发送完成信号量被释放 */
        return pdFALSE; /* 115200波特率下 发送长度少于 256B 长度数据包耗时超过 30mS */
    }
    return pdPASS;
}

/**
 * @brief  串口DMA发送失败后处理
 * @param  timeout 超时时间
 * @retval None
 */
void comm_Data_DMA_TX_Error(void)
{
    xSemaphoreGive(comm_Data_Send_Sem); /* DMA 发送异常 释放信号量 */
}

/**
 * @brief  串口定时器中断处理 用于同步发送开始采集信号 PD
 * @param  None
 * @retval None
 */
void comm_Data_PD_Time_Deal_FromISR(void)
{
    static uint8_t i = 0, retry = 0, data[12];
    static uint8_t length;
    TickType_t tick;

    if (i >= 6) {
        i = 0;
        retry = 0;
        HAL_TIM_Base_Stop_IT(&COMM_DATA_TIM_PD); /* 停止定时器 */
        gComm_Data_TIM_StartFlag = 0;
        return;
    }

    if (1) {
        // if (i == 0 || gComm_Data_RecvConfirm_Get() + 1 == i) { /* 构造下一个数据包 */
        tick = xTaskGetTickCountFromISR();
        data[0] = (tick >> 24) & 0xFF;
        data[1] = (tick >> 16) & 0xFF;
        data[2] = (tick >> 8) & 0xFF;
        data[3] = (tick >> 0) & 0xFF;
        data[4] = 0x02;
        length = buildPackOrigin(eComm_Data, eComm_Data_Outbound_CMD_START, data, 5);
        ++i;
        retry = 1;
    } else {
        ++retry; /* 重发计数器加一 */
        if (retry > 5) {
            i = 0;
            retry = 0;
            HAL_TIM_Base_Stop_IT(&COMM_DATA_TIM_PD); /* 停止定时器 */
            gComm_Data_TIM_StartFlag = 0;
            return;
        }
    }

    if (HAL_UART_Transmit_DMA(&huart5, data, length) != HAL_OK) { /* 发送失败 */
        FL_Error_Handler(__FILE__, __LINE__);
    }
}

/**
 * @brief  向采样板周期性发送数据
 * @param  None
 * @retval None
 */
uint8_t comm_Data_Sample_Start(void)
{
    if (gComm_Data_TIM_StartFlag == 1) {
        return 1;
    }

    gComm_Data_TIM_StartFlag = 1;
    HAL_TIM_Base_Start_IT(&COMM_DATA_TIM_PD); /* 启动定时器 开始测试 */

    return 0;
}

/**
 * @brief  采样板配置数据包
 * @param  pData 结果存放指针
 * @retval 数据包长度
 */
BaseType_t comm_Data_Sample_Load_Conf(uint8_t * pData)
{
    uint8_t i, sendLength;

    if (ARRAY_LEN(gComm_Data_Sample_Confs) == 0) {
        return PROTOCOL_PARSE_DATA_ERROR;
    }

    for (i = 0; i < ARRAY_LEN(gComm_Data_Sample_Confs); ++i) {
        pData[3 * i + 0] = gComm_Data_Sample_Confs[i].assay;
        pData[3 * i + 1] = gComm_Data_Sample_Confs[i].radiant;
        pData[3 * i + 2] = (gComm_Data_Sample_Confs[i].points_num > 120) ? (120) : (gComm_Data_Sample_Confs[i].points_num);
    }
    sendLength = buildPackOrigin(eComm_Data, eComm_Data_Outbound_CMD_CONF, pData, 3 * i); /* 构造测试配置包 */
    return comm_Data_SendTask_QueueEmit(pData, sendLength, 50);
}

/**
 * @brief  采样数据处理
 * @param  data_num    数据个数
 * @param  channel     采样通道索引
 * @param  pSample     数据指针
 * @retval None
 */
void comm_Data_Sample_Data_Deal(uint8_t data_num, uint8_t channel, uint8_t * pSample) {}

/**
 * @brief  串口任务初始化
 * @param  None
 * @retval None
 */
void comm_Data_Init(void)
{
    BaseType_t xResult;

    comm_Data_ConfInit();

    /* 接收队列 */
    comm_Data_RecvQueue = xQueueCreate(3, sizeof(sComm_Data_RecvInfo));
    if (comm_Data_RecvQueue == NULL) {
        FL_Error_Handler(__FILE__, __LINE__);
    }

    /* DMA 发送资源信号量*/
    comm_Data_Send_Sem = xSemaphoreCreateBinary();
    if (comm_Data_Send_Sem == NULL) {
        FL_Error_Handler(__FILE__, __LINE__);
    }
    xSemaphoreGive(comm_Data_Send_Sem);

    /* 发送队列 */
    comm_Data_SendQueue = xQueueCreate(1, sizeof(sComm_Data_SendInfo));
    if (comm_Data_SendQueue == NULL) {
        FL_Error_Handler(__FILE__, __LINE__);
    }

    /* Start DMA */
    if (HAL_UART_Receive_DMA(&COMM_DATA_UART_HANDLE, gComm_Data_RX_dma_buffer, ARRAY_LEN(gComm_Data_RX_dma_buffer)) != HAL_OK) {
        FL_Error_Handler(__FILE__, __LINE__);
    }

    /* 创建串口接收任务 */
    xResult = xTaskCreate(comm_Data_Recv_Task, "CommDataRX", 192, NULL, TASK_PRIORITY_COMM_DATA_RX, &comm_Data_Recv_Task_Handle);
    if (xResult != pdPASS) {
        FL_Error_Handler(__FILE__, __LINE__);
    }
    /* 创建串口发送任务 */
    xResult = xTaskCreate(comm_Data_Send_Task, "CommDataTX", 128, NULL, TASK_PRIORITY_COMM_DATA_TX, &comm_Data_Send_Task_Handle);
    if (xResult != pdPASS) {
        FL_Error_Handler(__FILE__, __LINE__);
    }

    /* 使能串口空闲中断 */
    __HAL_UART_ENABLE_IT(&COMM_DATA_UART_HANDLE, UART_IT_IDLE);
}

/**
 * @brief  加入串口接收队列
 * @param  pData   数据指针
 * @param  length  数据长度
 * @retval 加入发送队列结果
 */
BaseType_t comm_Data_RecvTask_QueueEmit_ISR(uint8_t * pData, uint16_t length)
{
    BaseType_t xWaken = pdFALSE;
    sComm_Data_RecvInfo recvInfo;

    recvInfo.length = length;
    memcpy(recvInfo.buff, pData, length);
    return xQueueSendToBackFromISR(comm_Data_RecvQueue, &recvInfo, &xWaken);
}

/**
 * @brief  加入串口发送队列
 * @param  pData   数据指针
 * @param  length  数据长度
 * @param  timeout 超时时间
 * @retval 加入发送队列结果
 */
BaseType_t comm_Data_SendTask_QueueEmit(uint8_t * pData, uint8_t length, uint32_t timeout)
{
    BaseType_t xResult;
    if (length == 0 || pData == NULL) { /* 数据有效性检查 */
        return pdFALSE;
    }
    if (gComm_Data_SendInfoFlag == 0) { /* 重入标志 */
        return pdFALSE;
    }
    gComm_Data_SendInfoFlag = 0;
    memcpy(gComm_Data_SendInfo.buff, pData, length);
    gComm_Data_SendInfo.length = length;

    xResult = xQueueSendToBack(comm_Data_SendQueue, &gComm_Data_SendInfo, pdMS_TO_TICKS(timeout));
    gComm_Data_SendInfoFlag = 1;
    return xResult;
}

/**
 * @brief  串口接收回应包收到处理
 * @param  packIndex   回应包中帧号
 * @retval 加入发送队列结果
 */
BaseType_t comm_Data_Send_ACK_Notify(uint8_t packIndex)
{
    return xTaskNotify(comm_Data_Send_Task_Handle, packIndex, eSetValueWithoutOverwrite);
}

/**
 * @brief  串口1接收任务 采样板
 * @param  argument: 任务参数指针
 * @retval None
 */
static void comm_Data_Recv_Task(void * argument)
{
    sComm_Data_RecvInfo recvInfo;
    eProtocolParseResult pResult;

    for (;;) {
        if (xQueueReceive(comm_Data_RecvQueue, &recvInfo, pdMS_TO_TICKS(5)) != pdPASS) { /* 检查接收队列 */
            continue;                                                                    /* 队列空 */
        }

        pResult = protocol_Parse_Data(recvInfo.buff, recvInfo.length); /* 数据包协议解析 */
        if (pResult == PROTOCOL_PARSE_OK) {                            /* 数据包解析正常 */
        }
    }
}

/**
 * @brief  串口1发送任务 采样板
 * @param  argument: 任务参数指针
 * @retval None
 */
static void comm_Data_Send_Task(void * argument)
{
    sComm_Data_SendInfo sendInfo;
    uint8_t i, ucResult;
    uint32_t ulNotifyValue;

    for (;;) {
        if (uxSemaphoreGetCount(comm_Data_Send_Sem) == 0) { /* DMA发送未完成 此时从接收队列提取数据覆盖发送指针会干扰DMA发送 */
            vTaskDelay(5);
            continue;
        }
        if (xQueuePeek(comm_Data_SendQueue, &sendInfo, pdMS_TO_TICKS(5)) != pdPASS) { /* 发送队列为空 */
            continue;
        }
        ucResult = 0; /* 发送结果初始化 */
        for (i = 0; i < COMM_DATA_SER_TX_RETRY_NUM; ++i) {
            if (serialSendStartDMA(COMM_DATA_SERIAL_INDEX, sendInfo.buff, sendInfo.length, 30) != pdPASS) { /* 启动串口发送 */
                vTaskDelay(pdMS_TO_TICKS(30));                                                              /* 30mS 后重新尝试启动DMA发送 */
                continue;
            }
            if (protocol_is_NeedWaitRACK(sendInfo.buff) != pdTRUE) { /* 判断发送后是否需要等待回应包 */
                ucResult = 2;                                        /* 无需等待回应包默认成功 */
                break;
            }
            if (xTaskNotifyWait(0, 0xFFFFFFFF, &ulNotifyValue, COMM_DATA_SER_TX_RETRY_INT) == pdPASS) { /* 等待任务通知 */
                if (ulNotifyValue == sendInfo.buff[3]) {
                    ucResult = 1; /* 置位发送成功 */
                    break;
                }
            }
        }
        if (ucResult == 0) { /* 重发失败处理 */
            memset(sendInfo.buff, 0xAA, sendInfo.length);
            serialSendStartDMA(COMM_DATA_SERIAL_INDEX, sendInfo.buff, sendInfo.length, 30);
        }
        xQueueReceive(comm_Data_SendQueue, &sendInfo, 0); /* 释放发送队列 */
    }
}

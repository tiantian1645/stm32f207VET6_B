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
#include "comm_out.h"
#include "comm_main.h"
#include "motor.h"
#include "temperature.h"
#include "white_motor.h"
#include "sample.h"

/* Extern variables ----------------------------------------------------------*/
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart5;
extern DMA_HandleTypeDef hdma_usart2_rx;
extern TIM_HandleTypeDef htim6;
extern TIM_HandleTypeDef htim7;

/* Private define ------------------------------------------------------------*/
#define COMM_DATA_SERIAL_INDEX eSerialIndex_2
#define COMM_DATA_UART_HANDLE huart2
#define COMM_DATA_TIM_WH htim6
#define COMM_DATA_TIM_PD htim7

#define COMM_DATA_SEND_QUEU_LENGTH 2
#define COMM_DATA_ACK_SEND_QUEU_LENGTH 6

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

/* 串口接收ACK记录 */
static sProcol_COMM_ACK_Record gComm_Data_ACK_Records[12];

/* 串口发送队列 */
static xQueueHandle comm_Data_SendQueue = NULL;
static xQueueHandle comm_Data_ACK_SendQueue = NULL;

/* 串口DMA发送资源信号量 */
static xSemaphoreHandle comm_Data_Send_Sem = NULL;

/* 串口收发任务句柄 */
static xTaskHandle comm_Data_Send_Task_Handle = NULL;

/* 测试配置项信号量 */
static xSemaphoreHandle comm_Data_Conf_Sem = NULL;

static sComm_Data_SendInfo gComm_Data_SendInfo;
static uint8_t gComm_Data_SendInfoFlag = 1; /* 加入发送队列缓存修改重入标志 */
static uint8_t gComm_Data_TIM_StartFlag = 0;
static uint8_t gComm_Data_Sample_Max_Point = 0;

/* 采样数据记录 */
static sComm_Data_Sample gComm_Data_Samples[6];

/* 采样对次数 */
static uint8_t gComm_Data_Sample_Pair_Cnt = 0;

static uint8_t gComm_Data_Sample_PD_WH_Idx = 0xFF;

static uint8_t gComm_Data_Stary_test_Falg = 0;

static uint8_t gComm_Data_Correct_Flag = 0; /* 定标状态标志 */
static eComm_Data_Sample_Radiant gComm_Data_Correct_wave = eComm_Data_Sample_Radiant_550;
/* Private constants ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
static void comm_Data_Send_Task(void * argument);
static BaseType_t comm_Data_Sample_Apply_Conf(uint8_t * pData);

/* Private user code ---------------------------------------------------------*/

/**
 * @brief  当前采样对次数 获取
 * @param  None
 * @retval gComm_Data_Sample_Pair_Cnt
 */
uint8_t gComm_Data_Sample_Pair_Cnt_Get(void)
{
    return gComm_Data_Sample_Pair_Cnt;
}

/**
 * @brief  当前采样对次数 +1
 * @param  None
 * @retval None
 */
void gComm_Data_Sample_Pair_Cnt_Inc(void)
{
    ++gComm_Data_Sample_Pair_Cnt;
}

/**
 * @brief  当前采样对次数 清零
 * @param  None
 * @retval None
 */
void gComm_Data_Sample_Pair_Cnt_Clear(void)
{
    gComm_Data_Sample_Pair_Cnt = 0;
}

/**
 * @brief  当前采样项目 获取
 * @param  None
 * @retval 1 白板 2 PD 0xFF 重置值
 */
uint8_t gComm_Data_Sample_PD_WH_Idx_Get(void)
{
    return gComm_Data_Sample_PD_WH_Idx;
}

/**
 * @brief  当前采样项目 设置
 * @param  idx 1 白板 2 PD 0xFF 重置值
 * @retval None
 */
void gComm_Data_Sample_PD_WH_Idx_Set(uint8_t idx)
{
    gComm_Data_Sample_PD_WH_Idx = idx;
}

/**
 * @brief  当前采样项目 清零
 * @param  None
 * @retval None
 */
void gComm_Data_Sample_PD_WH_Idx_Clear(void)
{
    gComm_Data_Sample_PD_WH_Idx_Set(0xFF);
}

/**
 * @brief  定标状态 标记
 * @param  stage 定标段索引
 * @retval None
 */
void gComm_Data_Correct_Flag_Mark(void)
{
    gComm_Data_Correct_Flag = 1;
}

/**
 * @brief  定标状态 清零
 * @param  None
 * @retval None
 */
void gComm_Data_Correct_Flag_Clr(void)
{
    gComm_Data_Correct_Flag = 0;
}

/**
 * @brief  定标状态 检查
 * @param  None
 * @retval 1 处于定标状态 0 未处于定标
 */
uint8_t gComm_Data_Correct_Flag_Check(void)
{
    return (gComm_Data_Correct_Flag > 0) ? (1) : (0);
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
    vComm_Data_Serial_Record.callback = protocol_Parse_Data_ISR;
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
 * @brief  串口DMA发送信号量等待
 * @param  timeout 超时时间
 * @retval None
 */
BaseType_t comm_Data_DMA_TX_Wait(uint32_t timeout)
{
    TickType_t tick;

    tick = xTaskGetTickCount();
    do {
        vTaskDelay(pdMS_TO_TICKS(5));
        if (uxSemaphoreGetCount(comm_Data_Send_Sem) > 0) {
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
 * @brief  串口DMA发送接收异常处理
 * @note   https://community.st.com/s/question/0D50X00009XkfN8SAJ/restore-circular-dma-rx-after-uart-error
 * @param  None
 * @retval None
 */
void comm_Data_DMA_RX_Restore(void)
{
    if (HAL_UART_Receive_DMA(&COMM_DATA_UART_HANDLE, gComm_Data_RX_dma_buffer, ARRAY_LEN(gComm_Data_RX_dma_buffer)) != HAL_OK) {
        FL_Error_Handler(__FILE__, __LINE__);
    }
}

/**
 * @brief  最大采样点数 获取
 * @param  None
 * @retval 最大采样点数
 */
uint8_t gComm_Data_Sample_Max_Point_Get(void)
{
    return gComm_Data_Sample_Max_Point;
}

/**
 * @brief  最大采样点数 清零
 * @param  None
 * @retval void
 */
void gComm_Data_Sample_Max_Point_Clear(void)
{
    gComm_Data_Sample_Max_Point = 0;
}

/**
 * @brief  最大采样点数 更新
 * @param  data
 * @retval None
 */
void gComm_Data_Sample_Max_Point_Update(uint8_t data)
{
    if (data > gComm_Data_Sample_Max_Point) {
        gComm_Data_Sample_Max_Point = data;
    }
}

/**
 * @brief  采样启动标志 清除
 * @param  None
 * @retval void
 */
void gComm_Data_TIM_StartFlag_Clear(void)
{
    gComm_Data_TIM_StartFlag = 0;
}

/**
 * @brief  采样启动标志 置位
 * @param  None
 * @retval
 */
void gComm_Data_TIM_StartFlag_Set(void)
{
    gComm_Data_TIM_StartFlag = 1;
}

/**
 * @brief  采样启动标志 检查
 * @param  None
 * @retval 0 未启动 1 进行中
 */
uint8_t gComm_Data_TIM_StartFlag_Check(void)
{
    return gComm_Data_TIM_StartFlag == 1;
}

/**
 * @brief  杂散光测试标志 标记
 * @param  None
 * @retval None
 */
void comm_Data_Stary_Test_Mark(void)
{
    gComm_Data_Stary_test_Falg = 1;
}

/**
 * @brief  杂散光测试标志 清除
 * @param  None
 * @retval None
 */
void comm_Data_Stary_Test_Clear(void)
{
    gComm_Data_Stary_test_Falg = 0;
}

/**
 * @brief  杂散光测试标志 读取
 * @param  None
 * @retval 1 正在测试 0 已经结束
 */
uint8_t comm_Data_Stary_Test_Is_Running(void)
{
    return (gComm_Data_Stary_test_Falg == 1) ? (1) : (0);
}

/**
 * @brief  同步发送开始采集信号
 * @param  None
 * @retval None
 */
void comm_Data_WH_Time_Deal(void)
{
    gComm_Data_Sample_PD_WH_Idx_Set(1); /* 更新当前采样项目 白物质 */
    comm_Data_ISR_Tran(0);              /* 采集白板 */
}

/**
 * @brief  串口定时器中断处理 用于同步发送开始采集信号
 * @note   每 10S 回调一次 优先级置于操作系统之上 不允许调用系统API
 * @param  None
 * @retval None
 */
void comm_Data_WH_Time_Deal_FromISR(void)
{
    comm_Data_WH_Time_Deal();
}

/**
 * @brief  串口定时器中断处理 用于同步发送开始采集信号
 * @note   800 mS 回调一次 优先级置于操作系统之上 不允许调用系统API
 * @param  None
 * @retval None
 */
void comm_Data_PD_Time_Deal_FromISR(void)
{
    gComm_Data_Sample_PD_WH_Idx_Set(2);                   /* 更新当前采样项目 PD */
    comm_Data_ISR_Tran(1);                                /* 采集PD */
    HAL_TIM_Base_Stop_IT(&COMM_DATA_TIM_PD);              /* 停止PD计时器 */
    gComm_Data_Sample_Pair_Cnt_Inc();                     /* 采样对次数+1 */
    __HAL_TIM_CLEAR_IT(&COMM_DATA_TIM_PD, TIM_IT_UPDATE); /* 清除更新事件标志位 */
    __HAL_TIM_SET_COUNTER(&COMM_DATA_TIM_PD, 0);          /* 清零定时器计数寄存器 */
}

/**
 * @brief  启动白板定时器
 * @note   屏蔽0S时中断
 * @param  None
 * @retval 0 成功启动 1 采样进行中
 */
uint8_t comm_Data_Sample_Start(void)
{
    if (gComm_Data_TIM_StartFlag_Check()) { /* 定时器未停止 采样进行中 */
        return 1;
    }
    gComm_Data_TIM_StartFlag_Set();                       /* 标记定时器启动 */
    gComm_Data_Sample_Pair_Cnt_Clear();                   /* 清零 采样对次数 */
    __HAL_TIM_CLEAR_IT(&COMM_DATA_TIM_WH, TIM_IT_UPDATE); /* 清除更新事件标志位 */
    __HAL_TIM_SET_COUNTER(&COMM_DATA_TIM_WH, 0);          /* 清零定时器计数寄存器 */
    HAL_TIM_Base_Start_IT(&COMM_DATA_TIM_WH);             /* 启动白板定时器 开始测试 */
    comm_Data_WH_Time_Deal();                             /* 首次发送 */
    return 0;
}

/**
 * @brief  停止PD定时器
 * @param  None
 * @retval None
 */
uint8_t comm_Data_sample_Stop(void)
{
    HAL_TIM_Base_Stop_IT(&COMM_DATA_TIM_WH);              /* 停止定时器 终止测试 */
    __HAL_TIM_CLEAR_IT(&COMM_DATA_TIM_WH, TIM_IT_UPDATE); /* 清除更新事件标志位 */
    __HAL_TIM_SET_COUNTER(&COMM_DATA_TIM_WH, 0);          /* 清零定时器计数寄存器 */
    return 0;
}

/**
 * @brief  启动PD定时器
 * @note   屏蔽0S时中断
 * @param  None
 * @retval None
 */
uint8_t comm_Data_sample_Start_PD(void)
{
    __HAL_TIM_CLEAR_IT(&COMM_DATA_TIM_PD, TIM_IT_UPDATE); /* 清除更新事件标志位 */
    __HAL_TIM_SET_COUNTER(&COMM_DATA_TIM_PD, 0);          /* 清零定时器计数寄存器 */
    HAL_TIM_Base_Start_IT(&COMM_DATA_TIM_PD);             /* 启动PD定时器 开始测试 */
    return 0;
}

/**
 * @brief  停止白板定时器
 * @param  None
 * @retval None
 */
uint8_t comm_Data_sample_Stop_PD(void)
{
    HAL_TIM_Base_Stop_IT(&COMM_DATA_TIM_PD);              /* 停止定时器 终止测试 */
    __HAL_TIM_CLEAR_IT(&COMM_DATA_TIM_PD, TIM_IT_UPDATE); /* 清除更新事件标志位 */
    __HAL_TIM_SET_COUNTER(&COMM_DATA_TIM_PD, 0);          /* 清零定时器计数寄存器 */
    return 0;
}

/**
 * @brief  终止采样
 * @param  None
 * @retval 0 成功停止 1 采样已停止
 */
uint8_t comm_Data_Sample_Force_Stop(void)
{
    if (gComm_Data_TIM_StartFlag_Check() == 0) {
        return 1;
    }
    comm_Data_sample_Stop();             /* 停止白板定时器 终止测试 */
    comm_Data_sample_Stop_PD();          /* 停止PD定时器 终止测试 */
    gComm_Data_TIM_StartFlag_Clear();    /* 清除测试中标志位 */
    gComm_Data_Sample_Max_Point_Clear(); /* 清除最大点数 */
    gComm_Data_Sample_Pair_Cnt_Clear();  /* 清零 采样对次数 */
    comm_Data_Sample_Send_Clear_Conf();  /* 通知采样板 */
    return 0;
}

/**
 * @brief  终止采样
 * @param  None
 * @retval 0 成功停止 1 采样已停止
 */
uint8_t comm_Data_Sample_Force_Stop_FromISR(void)
{
    if (gComm_Data_TIM_StartFlag_Check() == 0) {
        return 1;
    }
    comm_Data_sample_Stop();                    /* 停止白板定时器 终止测试 */
    comm_Data_sample_Stop_PD();                 /* 停止PD定时器 终止测试 */
    gComm_Data_TIM_StartFlag_Clear();           /* 清除测试中标志位 */
    gComm_Data_Sample_Max_Point_Clear();        /* 清除最大点数 */
    gComm_Data_Sample_Pair_Cnt_Clear();         /* 清零 采样对次数 */
    comm_Data_Sample_Send_Clear_Conf_FromISR(); /* 通知采样板 */
    return 0;
}

/**
 * @brief  采样板配置数据包
 * @note   记录最大点数 决定采样终点
 * @param  pData 结果存放指针
 * @retval 提交到采集板发送队列结果
 */
static BaseType_t comm_Data_Sample_Apply_Conf(uint8_t * pData)
{
    uint8_t i, result = 0;

    gComm_Data_Sample_Max_Point_Clear(); /* 清除最大点数 */
    for (i = 0; i < ARRAY_LEN(gComm_Data_Samples); ++i) {
        gComm_Data_Samples[i].conf.assay = pData[3 * i + 0];                           /* 测试方法 */
        gComm_Data_Samples[i].conf.radiant = pData[3 * i + 1];                         /* 测试波长 */
        if (gComm_Data_Samples[i].conf.assay >= eComm_Data_Sample_Assay_Continuous &&  /* 测试方法范围内 */
            gComm_Data_Samples[i].conf.assay <= eComm_Data_Sample_Assay_Fixed &&       /* and */
            gComm_Data_Samples[i].conf.radiant >= eComm_Data_Sample_Radiant_610 &&     /* 测试波长范围内 */
            gComm_Data_Samples[i].conf.radiant <= eComm_Data_Sample_Radiant_405 &&     /* and */
            pData[3 * i + 2] > 0 && pData[3 * i + 2] <= 120) {                         /* 测试点数范围内 */
            gComm_Data_Samples[i].conf.points_num = pData[3 * i + 2];                  /* 设置测试点数 */
            gComm_Data_Sample_Max_Point_Update(gComm_Data_Samples[i].conf.points_num); /* 更新最大点数 */
            ++result;
        } else {
            gComm_Data_Samples[i].conf.points_num = 0; /* 点数清零 */
        }
    }
    comm_Data_Conf_Sem_Give(); /* 通知电机任务 配置项已下达 */
    if (result > 0) {
        return pdPASS;
    }
    return pdFALSE;
}

/**
 * @brief  采样板配置数据包 中断版本
 * @note   记录最大点数 决定采样终点
 * @param  pData 结果存放指针
 * @retval 提交到采集板发送队列结果
 */
static BaseType_t comm_Data_Sample_Apply_Conf_FromISR(uint8_t * pData)
{
    uint8_t i, result = 0;

    gComm_Data_Sample_Max_Point_Clear(); /* 清除最大点数 */
    for (i = 0; i < ARRAY_LEN(gComm_Data_Samples); ++i) {
        gComm_Data_Samples[i].conf.assay = pData[3 * i + 0];                           /* 测试方法 */
        gComm_Data_Samples[i].conf.radiant = pData[3 * i + 1];                         /* 测试波长 */
        if (gComm_Data_Samples[i].conf.assay >= eComm_Data_Sample_Assay_Continuous &&  /* 测试方法范围内 */
            gComm_Data_Samples[i].conf.assay <= eComm_Data_Sample_Assay_Fixed &&       /* and */
            gComm_Data_Samples[i].conf.radiant >= eComm_Data_Sample_Radiant_610 &&     /* 测试波长范围内 */
            gComm_Data_Samples[i].conf.radiant <= eComm_Data_Sample_Radiant_405 &&     /* and */
            pData[3 * i + 2] > 0 && pData[3 * i + 2] <= 120) {                         /* 测试点数范围内 */
            gComm_Data_Samples[i].conf.points_num = pData[3 * i + 2];                  /* 设置测试点数 */
            gComm_Data_Sample_Max_Point_Update(gComm_Data_Samples[i].conf.points_num); /* 更新最大点数 */
            ++result;
        } else {
            gComm_Data_Samples[i].conf.points_num = 0; /* 点数清零 */
        }
    }
    comm_Data_Conf_Sem_Give_FromISR(); /* 通知电机任务 配置项已下达 */
    if (result > 0) {
        return pdPASS;
    }
    return pdFALSE;
}

/**
 * @brief  发送采样配置
 * @param  pData 缓存指针
 * @retval pdPASS 提交成功 pdFALSE 提交失败
 */
BaseType_t comm_Data_Sample_Send_Conf(uint8_t * pData)
{
    uint8_t sendLength;

    comm_Data_Sample_Apply_Conf(pData);
    sendLength = buildPackOrigin(eComm_Data, eComm_Data_Outbound_CMD_CONF, pData, 18); /* 构造测试配置包 */
    return comm_Data_SendTask_QueueEmit(pData, sendLength, 50);
}

/**
 * @brief  发送采样配置
 * @param  pData 缓存指针
 * @retval pdPASS 提交成功 pdFALSE 提交失败
 */
BaseType_t comm_Data_Sample_Send_Conf_FromISR(uint8_t * pData)
{
    uint8_t sendLength;

    comm_Data_Sample_Apply_Conf_FromISR(pData);
    sendLength = buildPackOrigin(eComm_Data, eComm_Data_Outbound_CMD_CONF, pData, 18); /* 构造测试配置包 */
    return comm_Data_SendTask_QueueEmit_FromISR(pData, sendLength);
}

/**
 * @brief  发送采样配置 工装测试
 * @param  pData 缓存指针
 * @retval pdPASS 提交成功 pdFALSE 提交失败
 */
BaseType_t comm_Data_Sample_Send_Conf_TV(uint8_t * pData)
{
    uint8_t sendLength;

    comm_Data_Sample_Apply_Conf(pData);
    sendLength = buildPackOrigin(eComm_Data, eComm_Data_Outbound_CMD_TEST, pData, 19); /* 构造工装测试配置包 */
    return comm_Data_SendTask_QueueEmit(pData, sendLength, 50);
}

/**
 * @brief  发送采样配置 工装测试 中断版本
 * @param  pData 缓存指针
 * @retval pdPASS 提交成功 pdFALSE 提交失败
 */
BaseType_t comm_Data_Sample_Send_Conf_TV_FromISR(uint8_t * pData)
{
    uint8_t sendLength;

    comm_Data_Sample_Apply_Conf_FromISR(pData);
    sendLength = buildPackOrigin(eComm_Data, eComm_Data_Outbound_CMD_TEST, pData, 19); /* 构造工装测试配置包 */
    return comm_Data_SendTask_QueueEmit_FromISR(pData, sendLength);
}
/**
 * @brief  发送采样配置 定标
 * @param  wave 波长配置  405不做定标
 * @param  pData 缓存指针
 * @retval pdPASS 提交成功 pdFALSE 提交失败
 */
BaseType_t comm_Data_Sample_Send_Conf_Correct(uint8_t * pData, eComm_Data_Sample_Radiant wave)
{
    uint8_t i, sendLength;

    gComm_Data_Sample_Max_Point_Clear(); /* 清除最大点数 */
    for (i = 0; i < 6; ++i) {
        pData[0 + 3 * i] = eComm_Data_Sample_Assay_Continuous; /* 测试方法 */
        pData[1 + 3 * i] = wave;                               /* 测试波长 */
        if (wave == eComm_Data_Sample_Radiant_610) {
            pData[2 + 3 * i] = 12; /* 测试点数 12 */
        } else {
            pData[2 + 3 * i] = 12; /* 测试点数 12 */
        }
        gComm_Data_Sample_Max_Point_Update(pData[2 + 3 * i]); /* 更新最大点数 */
    }
    gComm_Data_Correct_wave = wave;
    sendLength = buildPackOrigin(eComm_Data, eComm_Data_Outbound_CMD_CONF, pData, 18); /* 构造工装测试配置包 */
    return comm_Data_SendTask_QueueEmit(pData, sendLength, 50);
}

/**
 * @brief  校正波长配置获取
 * @param  None
 * @retval gComm_Data_Correct_wave
 */
eComm_Data_Sample_Radiant comm_Data_Get_Correct_Wave(void)
{
    return gComm_Data_Correct_wave;
}

/**
 * @brief  发送无效采样配置
 * @note   用于停止采样
 * @param  None
 * @retval pdPASS 提交成功 pdFALSE 提交失败
 */
BaseType_t comm_Data_Sample_Send_Clear_Conf(void)
{
    uint8_t i, sendLength, pData[30];

    for (i = 0; i < ARRAY_LEN(gComm_Data_Samples); ++i) {
        pData[3 * i + 0] = eComm_Data_Sample_Assay_None;       /* 测试方法 */
        pData[3 * i + 1] = gComm_Data_Samples[i].conf.radiant; /* 测试波长 */
        pData[3 * i + 2] = 0;                                  /* 测试点数 */
    }

    sendLength = buildPackOrigin(eComm_Data, eComm_Data_Outbound_CMD_CONF, pData, 18); /* 构造测试配置包 */
    return comm_Data_SendTask_QueueEmit(pData, sendLength, 50);
}

/**
 * @brief  发送无效采样配置 中断版本
 * @note   用于停止采样
 * @param  None
 * @retval pdPASS 提交成功 pdFALSE 提交失败
 */
BaseType_t comm_Data_Sample_Send_Clear_Conf_FromISR(void)
{
    uint8_t i, sendLength, pData[30];

    for (i = 0; i < ARRAY_LEN(gComm_Data_Samples); ++i) {
        pData[3 * i + 0] = eComm_Data_Sample_Assay_None;       /* 测试方法 */
        pData[3 * i + 1] = gComm_Data_Samples[i].conf.radiant; /* 测试波长 */
        pData[3 * i + 2] = 0;                                  /* 测试点数 */
    }

    sendLength = buildPackOrigin(eComm_Data, eComm_Data_Outbound_CMD_CONF, pData, 18); /* 构造测试配置包 */
    return comm_Data_SendTask_QueueEmit_FromISR(pData, sendLength);
}

/**
 * @brief  发送杂散光测试包
 * @note   开始杂散光测试 耗时15秒
 * @retval pdPASS 提交成功 pdFALSE 提交失败
 */
BaseType_t comm_Data_Start_Stary_Test(void)
{
    uint8_t sendLength, pData[8];

    sendLength = buildPackOrigin(eComm_Data, eComm_Data_Outbound_CMD_STRAY, pData, 0); /* 构造杂散光测试包 */
    pData[3] = (uint8_t)(temp_Random_Generate());                                      /* 随机帧号 */
    pData[3] = (pData[3] == 0) ? (0xA5) : (pData[3]);                                  /* 排除零值 */
    comm_Data_Stary_Test_Mark();                                                       /* 标记杂散光测试开始 */
    return comm_Data_SendTask_QueueEmit(pData, sendLength, 50);
}

/**
 * @brief  测试配置项信号量 等待
 * @param  timeout 超时时间
 * @retval 等待结果
 */
BaseType_t comm_Data_Conf_Sem_Wait(uint32_t timeout)
{
    return xSemaphoreTake(comm_Data_Conf_Sem, timeout);
}

/**
 * @brief  测试配置项信号量 释放
 * @param  None
 * @retval 释放结果
 */
BaseType_t comm_Data_Conf_Sem_Give(void)
{
    return xSemaphoreGive(comm_Data_Conf_Sem);
}

/**
 * @brief  测试配置项信号量 释放 中断版本
 * @param  None
 * @retval 释放结果
 */
BaseType_t comm_Data_Conf_Sem_Give_FromISR(void)
{
    BaseType_t xResult, xHigherPriorityTaskWoken = pdFALSE;

    xResult = xSemaphoreGiveFromISR(comm_Data_Conf_Sem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    return xResult;
}

/**
 * @brief  采样数据记录 获取 原始数据
 * @param  channel 通道索引 1～6 pBuffer 数据输入指针 pLength 数据输入长度
 * @retval 获取结果 0 正常 1 通道索引异常
 */
uint8_t comm_Data_Sample_Data_Fetch(uint8_t channel, uint8_t * pBuffer, uint8_t * pLength)
{

    if (channel < 1 || channel > 6) { /* 检查通道编码 */
        *pLength = 0;
        return 1;
    }

    pBuffer[0] = gComm_Data_Samples[channel - 1].num;
    pBuffer[1] = channel;
    *pLength = gComm_Data_Samples[channel - 1].num * gComm_Data_Samples[channel - 1].data_type + 2;
    memcpy(pBuffer + 2, gComm_Data_Samples[channel - 1].raw_datas, *pLength - 2);
    return 0;
}

/**
 * @brief  采样数据记录
 * @param  channel 通道索引 1～6 pBuffer 数据输入指针 length 数据输入长度
 * @retval 变换结果 0 正常 1 通道索引异常
 */
uint8_t comm_Data_Sample_Data_Commit(uint8_t channel, uint8_t * pBuffer, uint8_t length)
{

    if (channel < 1 || channel > 6) { /* 检查通道编码 */
        return 1;
    }

    gComm_Data_Samples[channel - 1].num = pBuffer[6]; /* 数据个数 u16 | u32 */

    if (length - 9 == pBuffer[6] * 2) {                /* u16 */
        gComm_Data_Samples[channel - 1].data_type = 2; /* 数据类型标识 */
    } else if (length - 9 == pBuffer[6] * 4) {         /* u32 */
        gComm_Data_Samples[channel - 1].data_type = 4; /* 数据类型标识 */
    } else {                                           /* 异常长度 */
        gComm_Data_Samples[channel - 1].data_type = 1; /* 数据类型标识 */
    }
    memcpy(gComm_Data_Samples[channel - 1].raw_datas, pBuffer + 8, length - 9); /* 原封不动复制 */
    return 0;
}

/**
 * @brief  采样数据校正变换
 * @param  channel 通道索引 1～6 pBuffer 数据输出指针 pLength 数据输出长度
 * @note   uint16_t 型数据执行校正 uin32_t 型原样返回
 * @retval 变换结果 0 正常 1 通道索引异常 2 数据类型不匹配 3 线性投影失败
 */
uint8_t comm_Data_Sample_Data_Correct(uint8_t channel, uint8_t * pBuffer, uint8_t * pLength)
{
    uint8_t error = 0, i;
    uint16_t output_16;
    uint32_t input, output_32;
    sComm_Data_Sample * pSampleData;

    *pLength = 0; /* 初始化输出数据长度 */

    if (channel < 1 || channel > 6) { /* 通道索引异常 */
        return 1;                     /* 提前返回 */
    }

    pSampleData = gComm_Data_Samples + channel - 1;

    if (pSampleData->data_type == 4) {                                     /* uint32_t型 */
        pBuffer[0] = pSampleData->num;                                     /* 数据个数 */
        pBuffer[1] = channel;                                              /* 通道编码 */
        memcpy(pBuffer + 2, pSampleData->raw_datas, pSampleData->num * 4); /* 原样复制 */
        *pLength = pSampleData->num * 4 + 2;                               /* 数据长度 */
        return 0;                                                          /* 提前返回 */
    }

    if (pSampleData->data_type != 2) { /* 数据类型不匹配 */
        return 2;                      /* 提前返回 */
    }

    pBuffer[0] = pSampleData->num;                                                                /* 数据个数 */
    pBuffer[1] = channel;                                                                         /* 通道编码 */
    *pLength = 2;                                                                                 /* 数据包长度 */
    for (i = 0; i < pSampleData->num; ++i) {                                                      /* 各个数据投影校正 */
        input = *((uint16_t *)(pSampleData->raw_datas + (2 * i)));                                /* 实际采样值 */
        if (sample_first_degree_cal(channel, pSampleData->conf.radiant, input, &output_32) > 0) { /* 线性投影 */
            error = 3;                                                                            /* 线性投影失败 */
        }
        output_16 = output_32;                        /* 范围判断 */
        memcpy(pBuffer + 2 + (2 * i), &output_16, 2); /* 拷贝 */
        *pLength += 2;                                /* 输出数据长度+2 (16 / 8) */
    }
    return error;
}

/**
 * @brief  串口任务初始化
 * @param  None
 * @retval None
 */
void comm_Data_Init(void)
{
    BaseType_t xResult;

    comm_Data_ConfInit();
    comm_Data_GPIO_Init();

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

    /* 测试配置项信号量 */
    comm_Data_Conf_Sem = xSemaphoreCreateBinary();
    if (comm_Data_Conf_Sem == NULL) {
        FL_Error_Handler(__FILE__, __LINE__);
    }
    xSemaphoreTake(comm_Data_Conf_Sem, 0);

    /* 发送队列 */
    comm_Data_SendQueue = xQueueCreate(COMM_DATA_SEND_QUEU_LENGTH, sizeof(sComm_Data_SendInfo));
    if (comm_Data_SendQueue == NULL) {
        FL_Error_Handler(__FILE__, __LINE__);
    }
    /* 发送队列 ACK专用 */
    comm_Data_ACK_SendQueue = xQueueCreate(COMM_DATA_ACK_SEND_QUEU_LENGTH, sizeof(uint8_t));
    if (comm_Data_ACK_SendQueue == NULL) {
        FL_Error_Handler(__FILE__, __LINE__);
    }

    /* Start DMA */
    if (HAL_UART_Receive_DMA(&COMM_DATA_UART_HANDLE, gComm_Data_RX_dma_buffer, ARRAY_LEN(gComm_Data_RX_dma_buffer)) != HAL_OK) {
        FL_Error_Handler(__FILE__, __LINE__);
    }

    /* 创建串口发送任务 */
    xResult = xTaskCreate(comm_Data_Send_Task, "CommDataTX", 256, NULL, TASK_PRIORITY_COMM_DATA_TX, &comm_Data_Send_Task_Handle);
    if (xResult != pdPASS) {
        FL_Error_Handler(__FILE__, __LINE__);
    }

    /* 使能串口空闲中断 */
    __HAL_UART_ENABLE_IT(&COMM_DATA_UART_HANDLE, UART_IT_IDLE);
}

/**
 * @brief  加入串口ACK发送队列 中断版本
 * @param  pPackIndex   回应帧号
 * @retval 加入发送队列结果
 */
BaseType_t comm_Data_SendTask_ACK_QueueEmitFromISR(uint8_t * pPackIndex)
{
    BaseType_t xResult, xHigherPriorityTaskWoken = pdFALSE;

    xResult = xQueueSendToBackFromISR(comm_Data_ACK_SendQueue, pPackIndex, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    return xResult;
}

/**
 * @brief  串口ACK发送队列 处理
 * @param  timoeut   超时时间
 * @retval 处理发送队列结果
 */
BaseType_t comm_Data_SendTask_ACK_Consume(uint32_t timeout)
{
    BaseType_t xResult;
    uint8_t buffer[8];

    if (comm_Data_ACK_SendQueue == NULL) {
        return pdFAIL;
    }

    for (;;) {
        xResult = xQueueReceive(comm_Data_ACK_SendQueue, &buffer[0], timeout / COMM_DATA_ACK_SEND_QUEU_LENGTH);
        if (xResult) {
            if (serialSendStartDMA(COMM_DATA_SERIAL_INDEX, buffer, buildPackOrigin(eComm_Data, eProtocolRespPack_Client_ACK, &buffer[0], 1), 30)) {
                comm_Out_DMA_TX_Wait(30);
            }
        } else {
            break;
        }
    }

    return xResult;
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
        error_Emit(eError_Comm_Data_Busy);
        return pdFALSE;
    }
    gComm_Data_SendInfoFlag = 0;
    memcpy(gComm_Data_SendInfo.buff, pData, length);
    gComm_Data_SendInfo.length = length;

    xResult = xQueueSendToBack(comm_Data_SendQueue, &gComm_Data_SendInfo, pdMS_TO_TICKS(timeout));
    gComm_Data_SendInfoFlag = 1;
    if (xResult != pdPASS) {
        error_Emit(eError_Comm_Data_Busy);
    }
    return xResult;
}

/**
 * @brief  加入串口发送队列
 * @param  pData   数据指针
 * @param  length  数据长度
 * @retval 加入发送队列结果
 */
BaseType_t comm_Data_SendTask_QueueEmit_FromISR(uint8_t * pData, uint8_t length)
{
    BaseType_t xResult;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (length == 0 || pData == NULL) { /* 数据有效性检查 */
        return pdFALSE;
    }
    if (gComm_Data_SendInfoFlag == 0) { /* 重入标志 */
        error_Emit_FromISR(eError_Comm_Data_Busy);
        return pdFALSE;
    }
    gComm_Data_SendInfoFlag = 0;
    memcpy(gComm_Data_SendInfo.buff, pData, length);
    gComm_Data_SendInfo.length = length;

    xResult = xQueueSendToBackFromISR(comm_Data_SendQueue, &gComm_Data_SendInfo, &xHigherPriorityTaskWoken);
    gComm_Data_SendInfoFlag = 1;
    if (xResult != pdPASS) {
        error_Emit_FromISR(eError_Comm_Data_Busy);
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    return xResult;
}

/**
 * @brief  串口接收回应包 帧号接收
 * @param  packIndex   回应包中帧号
 * @retval 加入发送队列结果
 */
BaseType_t comm_Data_Send_ACK_Give_From_ISR(uint8_t packIndex)
{
    static uint8_t idx = 0;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    gComm_Data_ACK_Records[idx].tick = xTaskGetTickCountFromISR();
    gComm_Data_ACK_Records[idx].ack_idx = packIndex;
    ++idx;
    if (idx >= ARRAY_LEN(gComm_Data_ACK_Records)) {
        idx = 0;
    }
    xTaskNotifyFromISR(comm_Data_Send_Task_Handle, packIndex, eSetValueWithOverwrite, &xHigherPriorityTaskWoken); /* 允许覆盖 */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    return pdPASS;
}

/**
 * @brief  串口接收回应包收到处理
 * @param  packIndex   回应包中帧号
 * @retval 加入发送队列结果
 */
BaseType_t comm_Data_Send_ACK_Wait(uint8_t packIndex)
{
    uint8_t i, j;
    uint32_t ulNotifyValue = 0;

    for (j = 0; j < COMM_DATA_SER_TX_RETRY_WC; ++j) {
        if (xTaskNotifyWait(0, 0xFFFFFFFF, &ulNotifyValue, COMM_DATA_SER_TX_RETRY_WT) == pdPASS && ulNotifyValue == packIndex) {
            return pdPASS;
        }
        for (i = 0; i < ARRAY_LEN(gComm_Data_ACK_Records); ++i) {
            if (gComm_Data_ACK_Records[i].ack_idx == packIndex) {
                return pdPASS;
            }
        }
        comm_Data_SendTask_ACK_Consume(0);
    }
    return pdFALSE;
}

/**
 * @brief  采样全部结束 后续处理
 * @param  None
 * @retval
 */
BaseType_t comm_Data_Sample_Owari(void)
{
    uint8_t buffer[7];
    BaseType_t result;

    result = comm_Main_SendTask_QueueEmitWithBuildCover(eProtocolRespPack_Client_SAMP_OVER, buffer, 0);
    comm_Out_SendTask_QueueEmitWithModify(buffer, 7, 0);
    return result;
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

    for (;;) {
        if (uxSemaphoreGetCount(comm_Data_Send_Sem) == 0) { /* DMA发送未完成 此时从接收队列提取数据覆盖发送指针会干扰DMA发送 保护 sendInfo */
            vTaskDelay(5);
            continue;
        }
        comm_Data_SendTask_ACK_Consume(5);

        if (xQueuePeek(comm_Data_SendQueue, &sendInfo, pdMS_TO_TICKS(5)) != pdPASS) { /* 发送队列为空 */
            continue;
        }
        ucResult = 0; /* 发送结果初始化 */
        for (i = 0; i < COMM_DATA_SER_TX_RETRY_NUM; ++i) {
            if (serialSendStartDMA(COMM_DATA_SERIAL_INDEX, sendInfo.buff, sendInfo.length, 30) != pdPASS) { /* 启动串口发送 */
                error_Emit(eError_Comm_Data_Send_Failed);                                                   /* 提交发送失败错误信息 */
                vTaskDelay(pdMS_TO_TICKS(30));                                                              /* 30mS 后重新尝试启动DMA发送 */
                continue;
            }
            if (protocol_is_NeedWaitRACK(sendInfo.buff) != pdTRUE) { /* 判断发送后是否需要等待回应包 */
                ucResult = 2;                                        /* 无需等待回应包默认成功 */
                break;
            }
            if (comm_Data_Send_ACK_Wait(sendInfo.buff[3]) == pdPASS) { /* 等待任务通知 */
                ucResult = 1;                                          /* 置位发送成功 */
                break;
            } else {
                ucResult = 0;
                switch (i) {
                    case 0:
                        error_Emit(eError_Comm_Out_Resend_1);
                        break;
                    case 1:
                        error_Emit(eError_Comm_Out_Resend_2);
                        break;
                    default:
                        break;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(10 << i)); /* 指数退避 */
        }
        if (ucResult == 0) {                                         /* 重发失败处理 */
            error_Emit(eError_Comm_Data_Not_ACK);                    /* 提交无ACK错误信息 */
            if (sendInfo.buff[5] == eComm_Data_Outbound_CMD_STRAY) { /* 发送无回应的是杂散光测试报文 */
                motor_Sample_Info(eMotorNotifyValue_SP_ERR);         /* 结束电机等待 */
            }
        }
        xQueueReceive(comm_Data_SendQueue, &sendInfo, 0); /* 释放发送队列 */
    }
}

/**
 * @brief  采样板相关管脚初始化
 * @param  argument    None
 * @note   只针对输出功能的 PD3 PD7
 * @retval None
 **/
void comm_Data_GPIO_Init(void)
{
    HAL_GPIO_WritePin(FRONT_STATUS_GPIO_Port, FRONT_STATUS_Pin, GPIO_PIN_SET); /* 采样输出脚拉高 准备下降沿采集白板 */
}

/**
 * @brief  重启采样板
 * @param  argument    None
 * @note   临时将PD3配置成输出
 * @retval BNone
 **/
void comm_Data_Board_Reset(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    GPIO_InitStruct.Pin = FRONT_RESET_Pin;
    GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
    GPIO_InitStruct.Pull = GPIO_NOPULL;

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP; /* 配置推挽输出 */
    HAL_GPIO_Init(FRONT_RESET_GPIO_Port, &GPIO_InitStruct);

    HAL_GPIO_WritePin(FRONT_RESET_GPIO_Port, FRONT_RESET_Pin, GPIO_PIN_SET);   /* 进入重置 */
    HAL_Delay(100);                                                            /* 延时100毫秒 */
    HAL_GPIO_WritePin(FRONT_RESET_GPIO_Port, FRONT_RESET_Pin, GPIO_PIN_RESET); /* 退出重置 */

    GPIO_InitStruct.Mode = GPIO_MODE_INPUT; /* 恢复输入状态 */
    HAL_GPIO_Init(FRONT_RESET_GPIO_Port, &GPIO_InitStruct);
}

/**
 * @brief  采样过程中采样板中断信号PD4处理
 * @note   上升沿 采样板开始采样 下降沿 采样板采样完成
 * @retval None
 **/
void comm_Data_ISR_Deal(void)
{
    if (HAL_GPIO_ReadPin(FRONT_TRIG_IN_GPIO_Port, FRONT_TRIG_IN_Pin) == GPIO_PIN_SET) { /* 上升沿 采样板开始采样 */

    } else {                                                                             /* 下降沿 采样板采样完成 */
        if (comm_Data_Stary_Test_Is_Running()) {                                         /* 判断是否处于杂散光测试中 */
            motor_Sample_Info_From_ISR(eMotorNotifyValue_SP);                            /* 通知电机任务杂散光测试完成 */
        } else if (gComm_Data_TIM_StartFlag_Check()) {                                   /* 定时器未停止 采样进行中 */
            if (gComm_Data_Sample_Pair_Cnt_Get() >= gComm_Data_Sample_Max_Point_Get()) { /* 采样对次数 大于等于 最大点数 */
                gComm_Data_Sample_PD_WH_Idx_Clear();                                     /* 标记采样对完成 */
                comm_Data_sample_Stop();                                                 /* 停止白板定时器 终止测试 */
                comm_Data_sample_Stop_PD();                                              /* 停止PD定时器 终止测试 */
                gComm_Data_TIM_StartFlag_Clear();                                        /* 清除测试中标志位 */
            }
            motor_Sample_Info_From_ISR(eMotorNotifyValue_TG); /* 通知电机任务采样完成 */
            if (gComm_Data_Sample_PD_WH_Idx_Get() == 1) {     /* 当前检测白物质 */
                comm_Data_sample_Start_PD();                  /* 启动PD定时器 */
            }
        }
    }
}

/**
 * @brief  向采样板发送采样类型信号
 * @param  wp 0 白板 下降沿 1 PD 上升沿
 * @retval None
 **/
void comm_Data_ISR_Tran(uint8_t wp)
{
    if (wp == 0) {
        HAL_GPIO_WritePin(FRONT_STATUS_GPIO_Port, FRONT_STATUS_Pin, GPIO_PIN_RESET); /* 采样输出脚拉低 下降沿 白板 */
    } else {
        HAL_GPIO_WritePin(FRONT_STATUS_GPIO_Port, FRONT_STATUS_Pin, GPIO_PIN_SET); /* 采样输出脚拉低 上升沿 PD */
    }
}

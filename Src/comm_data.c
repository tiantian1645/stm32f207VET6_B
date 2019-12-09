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

/* 串口接收ACK记录 */
static sProcol_COMM_ACK_Record gComm_Data_ACK_Records[12];

/* 串口发送队列 */
static xQueueHandle comm_Data_SendQueue = NULL;

/* 串口DMA发送资源信号量 */
static xSemaphoreHandle comm_Data_Send_Sem = NULL;

/* 串口收发任务句柄 */
static xTaskHandle comm_Data_Recv_Task_Handle = NULL;
static xTaskHandle comm_Data_Send_Task_Handle = NULL;

/* 测试配置项信号量 */
static xSemaphoreHandle comm_Data_Conf_Sem = NULL;

static sComm_Data_SendInfo gComm_Data_SendInfo;
static uint8_t gComm_Data_SendInfoFlag = 1; /* 加入发送队列缓存修改重入标志 */
static uint8_t gComm_Data_TIM_StartFlag = 0;
static uint8_t gComm_Data_Sample_Max_Point = 0;
static sComm_Data_Sample_Conf_Unit gComm_Data_Sample_Confs[6];

static uint32_t gComm_Data_Sample_ISR_Cnt = 0;
static uint8_t gComm_Data_Sample_PD_WH_Idx = 0xFF;
static uint8_t gCoMM_Data_Sample_ISR_Buffer[16];

static uint8_t gComm_Data_Stary_test_Falg = 0;

static uint16_t gComm_Data_Sample_Period = 500;
static uint16_t gComm_Data_Sample_Next_Idle = WHITE_MOTOR_RUN_PERIOD;
/* Private constants ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
static void comm_Data_Recv_Task(void * argument);
static void comm_Data_Send_Task(void * argument);

static BaseType_t comm_Data_RecvTask_QueueEmit_ISR(uint8_t * pData, uint16_t length);
static BaseType_t comm_Data_Send_ACK_Check(uint8_t packIndex);

static BaseType_t comm_Data_Sample_Apply_Conf(uint8_t * pData);
static BaseType_t comm_Data_Sample_Send_Clear_Conf(void);
/* Private user code ---------------------------------------------------------*/

/**
 * @brief  采样周期 获取
 * @param  None
 * @note   单位 定时器次数
 * @retval 采样周期 gComm_Data_Sample_Period
 */
uint16_t gComm_Data_Sample_Period_Get(void)
{
    return gComm_Data_Sample_Period;
}

/**
 * @brief  采样周期 设置
 * @param  se 采样周期单位 秒
 * @retval None
 */
void gComm_Data_Sample_Period_Set(uint8_t se)
{
    gComm_Data_Sample_Period = se * 50;
}

/**
 * @brief  白板电机切换时间 获取
 * @param  None
 * @note   单位 毫秒
 * @retval 白板电机切换时间 gComm_Data_Sample_Next_Idle
 */
uint16_t gComm_Data_Sample_Next_Idle_Get(void)
{
    return gComm_Data_Sample_Next_Idle;
}

/**
 * @brief  白板电机切换时间 设置
 * @param  idle 白板电机切换时间 单位 毫秒
 * @retval None
 */
void gComm_Data_Sample_Next_Idle_Set(uint16_t idle)
{
    gComm_Data_Sample_Next_Idle = idle;
}

/**
 * @brief  白板电机切换时间 初始化
 * @param  None
 * @retval None
 */
void gComm_Data_Sample_Next_Idle_Clr(void)
{
    gComm_Data_Sample_Next_Idle = WHITE_MOTOR_RUN_PERIOD;
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
 * @brief  串口定时器中断处理 用于同步发送开始采集信号
 * @note   200 mS 回调一次
 * @param  None
 * @retval None
 */
void comm_Data_PD_Time_Deal_FromISR(void)
{
    static uint8_t length, last_idx = 0, pair_cnt = 0;
    static uint32_t start_cnt_rs, start_cnt_pd;
    uint32_t period;

    if (gComm_Data_Sample_ISR_Cnt == 0) {
        pair_cnt = 0;
    }

    period = gComm_Data_Sample_Period_Get();
    if (gComm_Data_Sample_ISR_Cnt % period == 0 ||                                                                   /* 每 500 次  10S 或者 */
        gComm_Data_Sample_ISR_Cnt - start_cnt_pd == (gComm_Data_Sample_Next_Idle_Get() / COMM_DATA_PD_TIMER_TIME)) { /* 每 500k + 30 次  10S * k + 0.6*/
        gCoMM_Data_Sample_ISR_Buffer[0] = pair_cnt % 2 + 1;                                                          /* 采样类型 白物质 -> PD */
        gComm_Data_Sample_PD_WH_Idx_Set(gCoMM_Data_Sample_ISR_Buffer[0]);                                            /* 更新当前采样项目 */
        length = buildPackOrigin(eComm_Data, eComm_Data_Outbound_CMD_START, gCoMM_Data_Sample_ISR_Buffer, 1);        /* 构造下一个数据包 */
        last_idx = gCoMM_Data_Sample_ISR_Buffer[3];                                                                  /* 记录帧号 */

        if (HAL_UART_Transmit_DMA(&COMM_DATA_UART_HANDLE, gCoMM_Data_Sample_ISR_Buffer, length) != HAL_OK) { /* 首次发送 */
            error_Emit_FromISR(eError_Comm_Out_Lost_0);
        } else {
            ++pair_cnt;
        }
        if (gComm_Data_Sample_ISR_Cnt % period == 0) { /* 白板 */
            start_cnt_pd = gComm_Data_Sample_ISR_Cnt;  /* 记录当前中断次数 */
        } else {
            start_cnt_pd = 0;
        }
        start_cnt_rs = gComm_Data_Sample_ISR_Cnt;                                                          /* 记录当前中断次数 */
    } else if ((gComm_Data_Sample_ISR_Cnt - start_cnt_rs) % 10 == 0 && gComm_Data_TIM_StartFlag_Check() && /* 其余时刻 每10次 200mS 处理是否需要重发 */
               comm_Data_Send_ACK_Check(last_idx) != pdPASS) {                                             /* 收到的回应帧号不匹配 */

        error_Emit_FromISR(eError_Comm_Out_Lost_0 + (gComm_Data_Sample_ISR_Cnt - start_cnt_rs) / 10);

        if ((gComm_Data_Sample_ISR_Cnt - start_cnt_rs) / 10 >= COMM_DATA_SER_TX_RETRY_NUM) { /* 重发数目达到3次 放弃采样测试 */
            HAL_TIM_Base_Stop_IT(&COMM_DATA_TIM_PD);                                         /* 停止定时器 */
            gComm_Data_TIM_StartFlag_Clear();                                                /* 标记定时器停止 */
            gComm_Data_Sample_ISR_Cnt = 0;                                                   /* 定时器中断计数清零 */
            motor_Sample_Info_ISR(eMotorNotifyValue_BR_ERR);
            return;
        } else {
            if (HAL_UART_Transmit_DMA(&COMM_DATA_UART_HANDLE, gCoMM_Data_Sample_ISR_Buffer, length) != HAL_OK) { /* 执行重发 */
                error_Emit_FromISR(eError_Comm_Out_Lost_0);
            }
        }
    }
    if (pair_cnt % 2 == 0 &&                                 /* 新包次数是双数 而且 */
        pair_cnt / 2 >= gComm_Data_Sample_Max_Point_Get()) { /* 新包次数大于等于 最大点数 */
        pair_cnt = 0;                                        /* 清零新包次数 */
        HAL_TIM_Base_Stop_IT(&COMM_DATA_TIM_PD);             /* 停止定时器 */
        gComm_Data_TIM_StartFlag_Clear();                    /* 标记定时器停止 */
        gComm_Data_Sample_ISR_Cnt = 0;                       /* 定时器中断计数清零 */
        gComm_Data_Sample_PD_WH_Idx_Clear();                 /* 清除项目记录 */
        return;
    }
    ++gComm_Data_Sample_ISR_Cnt;
}

/**
 * @brief  向采样板周期性发送数据
 * @param  None
 * @retval 0 成功启动 1 采样进行中
 */
uint8_t comm_Data_Sample_Start(void)
{
    if (gComm_Data_TIM_StartFlag_Check()) { /* 定时器未停止 采样进行中 */
        return 1;
    }
    xSemaphoreTake(comm_Data_Send_Sem, portMAX_DELAY); /* 等待发送队列为空 死等! */
    gComm_Data_TIM_StartFlag_Set();                    /* 标记定时器启动 */
    gComm_Data_Sample_ISR_Cnt = 0;                     /* 定时器中断计数清零 */
    HAL_TIM_Base_Start_IT(&COMM_DATA_TIM_PD);          /* 启动定时器 开始测试 */
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
    HAL_TIM_Base_Stop_IT(&COMM_DATA_TIM_PD); /* 停止定时器 终止测试 */
    gComm_Data_TIM_StartFlag_Clear();
    gComm_Data_Sample_ISR_Cnt = 0;
    gComm_Data_Sample_Max_Point_Clear(); /* 清除最大点数 */
    comm_Data_Sample_Send_Clear_Conf();  /* 通知采样板 */
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
    for (i = 0; i < ARRAY_LEN(gComm_Data_Sample_Confs); ++i) {
        gComm_Data_Sample_Confs[i].assay = pData[3 * i + 0];                   /* 测试方法 */
        gComm_Data_Sample_Confs[i].radiant = pData[3 * i + 1];                 /* 测试波长 */
        if (gComm_Data_Sample_Confs[i].assay < eComm_Data_Sample_Assay_None || /* 测试方法越限 */
            gComm_Data_Sample_Confs[i].assay > eComm_Data_Sample_Assay_Fixed ||
            gComm_Data_Sample_Confs[i].radiant < eComm_Data_Sample_Radiant_610 || /* 测试波长越限 */
            gComm_Data_Sample_Confs[i].radiant > eComm_Data_Sample_Radiant_405) {
            gComm_Data_Sample_Confs[i].points_num = 0; /* 点数清零 */
        } else {
            gComm_Data_Sample_Confs[i].points_num = (pData[3 * i + 2] > 120) ? (0) : (pData[3 * i + 2]); /* 测试点数 */
            gComm_Data_Sample_Max_Point_Update(gComm_Data_Sample_Confs[i].points_num);                   /* 更新最大点数 */
            ++result;
        }
    }
    if (result > 0) {
        comm_Data_Conf_Sem_Give(); /* 通知电机任务 配置项已下达 */
    }
    return pdPASS;
}

/**
 * @brief  发送采样配置
 * @param  None
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
 * @brief  发送采样配置 工装测试
 * @param  None
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
 * @brief  发送无效采样配置
 * @note   用于停止采样
 * @param  None
 * @retval pdPASS 提交成功 pdFALSE 提交失败
 */
static BaseType_t comm_Data_Sample_Send_Clear_Conf(void)
{
    uint8_t i, sendLength, pData[30];

    for (i = 0; i < ARRAY_LEN(gComm_Data_Sample_Confs); ++i) {
        pData[3 * i + 0] = eComm_Data_Sample_Assay_None;       /* 测试方法 */
        pData[3 * i + 1] = gComm_Data_Sample_Confs[i].radiant; /* 测试波长 */
        pData[3 * i + 2] = 0;                                  /* 测试点数 */
    }

    sendLength = buildPackOrigin(eComm_Data, eComm_Data_Outbound_CMD_CONF, pData, 18); /* 构造测试配置包 */
    return comm_Data_SendTask_QueueEmit(pData, sendLength, 50);
}

/**
 * @brief  发送杂散光测试包
 * @note   开始杂散光测试 耗时5～6秒
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
 * @brief 测试配置项信号量 等待
 * @param  timeout 超时时间
 * @retval 等待结果
 */
BaseType_t comm_Data_Conf_Sem_Wait(uint32_t timeout)
{
    return xSemaphoreTake(comm_Data_Conf_Sem, timeout);
}

/**
 * @brief 测试配置项信号量 释放
 * @param  None
 * @retval 释放结果
 */
BaseType_t comm_Data_Conf_Sem_Give(void)
{
    return xSemaphoreGive(comm_Data_Conf_Sem);
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
    comm_Data_SendQueue = xQueueCreate(2, sizeof(sComm_Data_SendInfo));
    if (comm_Data_SendQueue == NULL) {
        FL_Error_Handler(__FILE__, __LINE__);
    }

    /* Start DMA */
    if (HAL_UART_Receive_DMA(&COMM_DATA_UART_HANDLE, gComm_Data_RX_dma_buffer, ARRAY_LEN(gComm_Data_RX_dma_buffer)) != HAL_OK) {
        FL_Error_Handler(__FILE__, __LINE__);
    }

    /* 创建串口接收任务 */
    xResult = xTaskCreate(comm_Data_Recv_Task, "CommDataRX", 232, NULL, TASK_PRIORITY_COMM_DATA_RX, &comm_Data_Recv_Task_Handle);
    if (xResult != pdPASS) {
        FL_Error_Handler(__FILE__, __LINE__);
    }
    /* 创建串口发送任务 */
    xResult = xTaskCreate(comm_Data_Send_Task, "CommDataTX", 216, NULL, TASK_PRIORITY_COMM_DATA_TX, &comm_Data_Send_Task_Handle);
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
 * @brief  串口接收回应包 帧号接收
 * @param  packIndex   回应包中帧号
 * @retval 加入发送队列结果
 */
BaseType_t comm_Data_Send_ACK_Give(uint8_t packIndex)
{
    static uint8_t idx = 0;
    gComm_Data_ACK_Records[idx].tick = xTaskGetTickCount();
    gComm_Data_ACK_Records[idx].ack_idx = packIndex;
    ++idx;
    if (idx >= ARRAY_LEN(gComm_Data_ACK_Records)) {
        idx = 0;
    }
    xTaskNotify(comm_Data_Send_Task_Handle, packIndex, eSetValueWithOverwrite); /* 允许覆盖 */
    return pdPASS;
}

/**
 * @brief  串口接收回应包收到处理
 * @param  packIndex   回应包中帧号
 * @retval 加入发送队列结果
 */
BaseType_t comm_Data_Send_ACK_Wait(uint8_t packIndex, uint32_t timeout)
{
    uint8_t i;
    TickType_t tick;
    uint32_t ulNotifyValue = 0;

    tick = xTaskGetTickCount();
    do {
        if (xTaskNotifyWait(0, 0xFFFFFFFF, &ulNotifyValue, 100) == pdPASS && ulNotifyValue == packIndex) {
            return pdPASS;
        }
        for (i = 0; i < ARRAY_LEN(gComm_Data_ACK_Records); ++i) {
            if (gComm_Data_ACK_Records[i].ack_idx == packIndex) {
                return pdPASS;
            }
        }
        vTaskDelay(5);
    } while (xTaskGetTickCount() - tick < timeout);
    return pdFALSE;
}

/**
 * @brief  串口接收回应包收到处理
 * @param  packIndex   回应包中帧号
 * @retval 加入发送队列结果
 */
static BaseType_t comm_Data_Send_ACK_Check(uint8_t packIndex)
{
    uint8_t i;

    for (i = 0; i < ARRAY_LEN(gComm_Data_ACK_Records); ++i) {
        if (gComm_Data_ACK_Records[i].ack_idx == packIndex) {
            return pdPASS;
        }
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
 * @brief  串口1接收任务 采样板
 * @param  argument: 任务参数指针
 * @retval None
 */
static void comm_Data_Recv_Task(void * argument)
{
    sComm_Data_RecvInfo recvInfo;

    for (;;) {
        if (xQueueReceive(comm_Data_RecvQueue, &recvInfo, portMAX_DELAY) != pdPASS) { /* 检查接收队列 */
            continue;                                                                 /* 队列空 */
        }
        protocol_Parse_Data(recvInfo.buff, recvInfo.length); /* 数据包协议解析 */
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

    for (;;) {
        if (uxSemaphoreGetCount(comm_Data_Send_Sem) == 0) { /* DMA发送未完成 此时从接收队列提取数据覆盖发送指针会干扰DMA发送 保护 sendInfo */
            vTaskDelay(5);
            continue;
        }
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
            if (comm_Data_Send_ACK_Wait(sendInfo.buff[3], COMM_DATA_SER_TX_RETRY_INT) == pdPASS) { /* 等待任务通知 */
                ucResult = 1;                                                                      /* 置位发送成功 */
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

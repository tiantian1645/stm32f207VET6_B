// https://github.com/MaJerle/STM32_USART_DMA_RX
// https://github.com/akospasztor/stm32-dma-uart
// https://stackoverflow.com/questions/43298708/stm32-implementing-uart-in-dma-mode
// https://github.com/kiltum/modbus
// https://www.cnblogs.com/pingwen/p/8416608.html
// https://os.mbed.com/handbook/CMSIS-RTOS

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <math.h>
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
#include "heater.h"
#include "storge_task.h"

/* Extern variables ----------------------------------------------------------*/
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart5;
extern DMA_HandleTypeDef hdma_usart2_rx;
extern TIM_HandleTypeDef htim6;
extern TIM_HandleTypeDef htim7;

/* Private define ------------------------------------------------------------*/
#define COMM_DATA_UART_HANDLE huart2
#define COMM_DATA_TIM_WH htim6
#define COMM_DATA_TIM_PD htim7

#define COMM_DATA_SEND_QUEU_LENGTH 2
#define COMM_DATA_ACK_SEND_QUEU_LENGTH 6

/* Private typedef -----------------------------------------------------------*/
typedef struct {
    int16_t dac;
    int32_t adc_avg;
} sComm_Data_LED_SP_Record;

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

static sComm_Data_SendInfo gComm_Data_SendInfo;         /* 提交发送任务到队列用缓存 */
static sComm_Data_SendInfo gComm_Data_SendInfo_FromISR; /* 提交发送任务到队列用缓存 中断用 */

static uint8_t gComm_Data_TIM_StartFlag = 0;
static uint8_t gComm_Data_Sample_Max_Point = 0;

/* 采样数据记录 */
static sComm_Data_Sample gComm_Data_Samples[6];

/* 采样对次数 */
static uint8_t gComm_Data_Sample_Pair_Cnt = 0;

static uint8_t gComm_Data_Sample_PD_WH_Idx = 0xFF;

static uint8_t gComm_Data_Stary_test_Falg = 0;
static eComm_Data_Sample_Radiant gComm_Data_SP_LED_Flag = 0;
static eComm_Data_Sample_Radiant gComm_Data_SelfCheck_PD_Flag = 0;
static sComm_LED_Voltage gComm_LED_Voltage = {0, 0, 0};
static int16_t gComm_Data_LED_Voltage_Interval = 1;
static uint8_t gComm_Data_LED_Voltage_Points = 1;

static uint8_t gComm_Data_Correct_Flag = 0; /* 定标状态标志 */
static eComm_Data_Sample_Radiant gComm_Data_Correct_wave = eComm_Data_Sample_Radiant_550;
static uint8_t gComm_Data_Correct_stages[6] = {0, 0, 0, 0, 0, 0}; /* 定标段索引 */

static uint8_t gComm_Data_Lamp_BP_Flag = 0;   /* 灯BP状态标志 */
static uint8_t gComm_Data_AgingLoop_Mode = 0; /* 老化测试 配置状态 */

static sComm_Data_LED_SP_Record gComm_Data_LED_SP_Record[3];

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
 * @param  None
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
 * @brief  灯BP状态 标记
 * @param  None
 * @retval None
 */
void gComm_Data_Lamp_BP_Flag_Mark(void)
{
    gComm_Data_Lamp_BP_Flag = 1;
}

/**
 * @brief  灯BP状态 清零
 * @param  None
 * @retval None
 */
void gComm_Data_Lamp_BP_Flag_Clr(void)
{
    gComm_Data_Lamp_BP_Flag = 0;
}

/**
 * @brief  灯BP状态 检查
 * @param  None
 * @retval 1 处于灯BP状态 0 未处于定标
 */
uint8_t gComm_Data_Lamp_BP_Flag_Check(void)
{
    return (gComm_Data_Lamp_BP_Flag > 0) ? (1) : (0);
}

/**
 * @brief  老化测试模式 读取
 * @param  None
 * @retval 老化测试模式
 */
uint8_t gComm_Data_AgingLoop_Mode_Get(void)
{
    return gComm_Data_AgingLoop_Mode;
}

/**
 * @brief  老化测试模式 设置
 * @param  mode 模式 0 为普通 1 为PD 2 为混合
 * @retval None
 */
void gComm_Data_AgingLoop_Mode_Set(uint8_t mode)
{
    gComm_Data_AgingLoop_Mode = mode;
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
 * @brief  采样数据记录信息初始化
 * @param  None
 * @retval None
 */
void comm_Data_RecordInit(void)
{
    uint8_t i;

    for (i = 0; i < ARRAY_LEN(gComm_Data_Samples); ++i) {
        gComm_Data_Samples[i].num = 0;
        gComm_Data_Samples[i].wave = 0;
        memset(gComm_Data_Samples[i].raw_datas, 0, 240);
    }
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
    BaseType_t xResult, xHigherPriorityTaskWoken = pdFALSE;

    if (comm_Data_Send_Sem != NULL) {
        xResult = xSemaphoreGiveFromISR(comm_Data_Send_Sem, &xHigherPriorityTaskWoken); /* DMA 发送完成 */
        if (xResult == pdTRUE) {
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
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
 * @brief  串口DMA发送开始前准备 中断版本
 * @param  None
 * @retval None
 */
BaseType_t comm_Data_DMA_TX_Enter_From_ISR(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (xSemaphoreTakeFromISR(comm_Data_Send_Sem, &xHigherPriorityTaskWoken) != pdPASS) { /* 确保发送完成信号量被释放 */
        return pdFALSE; /* 115200波特率下 发送长度少于 256B 长度数据包耗时超过 30mS */
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    return pdPASS;
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
 * @brief  串口DMA发送失败后处理 中断版本
 * @param  None
 * @retval None
 */
void comm_Data_DMA_TX_Error_From_ISR(void)
{
    BaseType_t xResult, xHigherPriorityTaskWoken = pdFALSE;

    xResult = xSemaphoreGiveFromISR(comm_Data_Send_Sem, &xHigherPriorityTaskWoken); /* DMA 发送异常 释放信号量 */
    if (xResult == pdTRUE) {
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
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
 * @brief  LED电压校正标志 标记
 * @param  None
 * @retval None
 */
void gComm_Data_SP_LED_Flag_Mark(eComm_Data_Sample_Radiant radiant)
{
    gComm_Data_SP_LED_Flag = radiant;
}

/**
 * @brief  LED电压校正标志 清除
 * @param  None
 * @retval None
 */
void gComm_Data_SP_LED_Flag_Clr(void)
{
    gComm_Data_SP_LED_Flag = 0;
}

/**
 * @brief  LED电压校正 读取
 * @param  None
 * @retval 1 正在测试 0 已经结束
 */
eComm_Data_Sample_Radiant comm_Data_SP_LED_Is_Running(void)
{
    return gComm_Data_SP_LED_Flag;
}

/**
 * @brief  自检测试 单项 PD 标记
 * @param  None
 * @retval None
 */
void gComm_Data_SelfCheck_PD_Flag_Mark(eComm_Data_Sample_Radiant radiant)
{
    gComm_Data_SelfCheck_PD_Flag = radiant;
}

/**
 * @brief  自检测试 单项 PD 清除
 * @param  None
 * @retval None
 */
void gComm_Data_SelfCheck_PD_Flag_Clr(void)
{
    gComm_Data_SelfCheck_PD_Flag = 0;
}

/**
 * @brief  自检测试 单项 PD 读取
 * @param  None
 * @retval 1 正在测试 0 已经结束
 */
eComm_Data_Sample_Radiant gComm_Data_SelfCheck_PD_Flag_Get(void)
{
    return gComm_Data_SelfCheck_PD_Flag;
}

/**
 * @brief  LED电压值获取
 * @param  None
 * @retval 0 操作成功 1 操作失败
 */
uint8_t comm_Data_Get_LED_Voltage()
{
    comm_Data_Conf_LED_Voltage_Get();
    vTaskDelay(300);
    return 0;
}

/**
 * @brief  LED电压值设置
 * @param  None
 * @retval 0 操作成功 1 操作失败
 */
uint8_t comm_Data_Set_LED_Voltage(eComm_Data_Sample_Radiant radiant, uint16_t voltage)
{
    uint8_t buffer[14];

    switch (radiant) {
        case eComm_Data_Sample_Radiant_610:
            gComm_LED_Voltage.led_voltage_610 = voltage;
            break;
        case eComm_Data_Sample_Radiant_550:
            gComm_LED_Voltage.led_voltage_550 = voltage;
            break;
        case eComm_Data_Sample_Radiant_405:
            gComm_LED_Voltage.led_voltage_405 = voltage;
            break;
        default:
            return 1;
    }
    memcpy(buffer, (uint8_t *)(&gComm_LED_Voltage), sizeof(sComm_LED_Voltage));
    comm_Data_Conf_LED_Voltage_Set(buffer);
    vTaskDelay(600);
    return 0;
}

/**
 * @brief  等待采样板数据
 * @param  mask 通道掩码 0x01->通道1 0x3F->通道1～6
 * @param  timeout 超时时间 毫秒
 * @retval 0 等待时间内接收完成 1 超时
 */
uint8_t comm_Data_Wait_Data(uint8_t mask, uint32_t timeout)
{
    uint8_t i, check;
    TickType_t xTick;

    xTick = xTaskGetTickCount();
    do {
        check = 1;
        for (i = 0; i < ARRAY_LEN(gComm_Data_Samples); ++i) {
            if (((1 << i) & mask) && gComm_Data_Samples[i].num == 0) {
                check = 0;
            }
        }
        if (check) {
            return 0;
        }
        vTaskDelay(200);
    } while (check == 0 && xTick + timeout < xTaskGetTickCount());
    return 1;
}

/**
 * @brief  复制采样板数据 PD
 * @param  mask 通道掩码 0x01->通道1 0x3F->通道1～6
 * @param  pBuffer
 * @retval 0 正常 1 数量过长
 */
uint8_t comm_Data_Copy_Data_U32(uint8_t mask, uint32_t * pBuffer)
{
    uint8_t i;

    for (i = 0; i < ARRAY_LEN(gComm_Data_Samples); ++i) {
        if (((1 << i) & mask) == 0) {
            continue;
        }
        if (gComm_Data_Samples[i].num == 0) {
            return 1;
        }
        memcpy((uint8_t *)(pBuffer), &gComm_Data_Samples[i].raw_datas[0], 4);
        ++pBuffer;
    }
    return 0;
}

/**
 * @brief  电压增量间隔 获取
 * @param  None
 * @retval 电压增量间隔
 */
int16_t gComm_Data_LED_Voltage_Interval_Get(void)
{
    return gComm_Data_LED_Voltage_Interval;
}

/**
 * @brief  电压增量间隔 设置
 * @param  interval 间隔值 毫伏
 * @retval None
 */
void gComm_Data_LED_Voltage_Interval_Set(int16_t interval)
{
    gComm_Data_LED_Voltage_Interval = interval;
}

/**
 * @brief  电压校正测试点数 获取
 * @param  None
 * @retval 电压校正测试点数
 */
uint8_t gComm_Data_LED_Voltage_Points_Get(void)
{
    return gComm_Data_LED_Voltage_Points;
}

/**
 * @brief  电压校正测试点数 设置
 * @param  points 测试点数
 * @retval None
 */
void gComm_Data_LED_Voltage_Points_Set(uint8_t points)
{
    gComm_Data_LED_Voltage_Points = points;
}

/**
 * @brief  检查LED测试数据
 * @param  radiant 波长
 * @param  dac LED输出电压DAC值
 * @param  idx 次数索引
 * @note   白板PD值应在 1000万～1400万之间
 * @note   根据测试值调整电压增量间隔 gComm_Data_LED_Voltage_Interval
 * @note 610
 * 30  8602768
 * 34  9926537
 * 38  11177849
 * 42  12531599
 * 46  13657148
 * 44  13104119
 * 43  12859434
 * @note 550
 * 400  8871945
 * 416  9281926
 * 432  9684998
 * 448  10067955
 * 464  10483267
 * 480  10901802
 * 496  11307418
 * 528  11716487
 * @retval 0 结束检查 1 继续检查
 */
uint8_t comm_Data_Check_LED(eComm_Data_Sample_Radiant radiant, uint16_t dac, uint8_t idx)
{
    uint8_t i, j, result = 1;
    uint32_t sums[6] = {0, 0, 0, 0, 0, 0}, temp_32, min = 0xFFFFFFFF, max = 0;
    int16_t sign;
    int32_t bias_1300 = 0;
    float cal_inter;
    static int32_t last_bias_1300 = 0x80000000;

    /* 计算总和 */
    for (i = 0; i < ARRAY_LEN(gComm_Data_Samples); ++i) {
        if (gComm_Data_Samples[i].data_type != 4) { /* 非32位整型 */
            return 2;                               /* 数据类型错误 */
        }
        for (j = 0; j < gComm_Data_Samples[i].num; j++) {
            memcpy((uint8_t *)(&temp_32), &gComm_Data_Samples[i].raw_datas[4 * j], 4);
            sums[i] += temp_32;
        }
    }

    /*均值判断处理 */
    j = 0;
    for (i = 0; i < ARRAY_LEN(gComm_Data_Samples); ++i) {
        if (gComm_Data_Samples[i].num == 0) {
            continue;
        }
        temp_32 = sums[i] / gComm_Data_Samples[i].num;
        storge_Sample_LED_PD_Set(radiant, i, temp_32, dac);
        if (temp_32 == 0) {
            continue;
        }
        ++j;
        if (temp_32 > max) {
            max = temp_32;
        }
        if (temp_32 < min) {
            min = temp_32;
        }
        if (temp_32 >= 13000000) {
            bias_1300 += temp_32 - 13000000;
        } else {
            bias_1300 -= 13000000 - temp_32;
        }
    }

    if (idx < ARRAY_LEN(gComm_Data_LED_SP_Record)) {
        gComm_Data_LED_SP_Record[idx % 3].dac = dac;
        gComm_Data_LED_SP_Record[idx % 3].adc_avg = temp_32;
    } else {
        memcpy(gComm_Data_LED_SP_Record, gComm_Data_LED_SP_Record + 1, sizeof(sComm_Data_LED_SP_Record));
        memcpy(gComm_Data_LED_SP_Record + 1, gComm_Data_LED_SP_Record + 2, sizeof(sComm_Data_LED_SP_Record));
        gComm_Data_LED_SP_Record[2].dac = dac;
        gComm_Data_LED_SP_Record[2].adc_avg = temp_32;
    }

    if (j == 0 || bias_1300 == 0) {             /* 没有有效值 或者没有误差*/
        gComm_Data_LED_Voltage_Interval_Set(0); /* 不修改间隔结束流程 */
        last_bias_1300 = 0x80000000;            /* 初始化历史值 */
        gComm_Data_LED_Voltage_Points_Set(1);   /* 点数设为 1 */
        return 0;
    }

    bias_1300 = bias_1300 / j;
    if (max >= 14000000) { /* 最大值越限 */
        sign = gComm_Data_LED_Voltage_Interval_Get() / 2;
        if (sign == 0) {
            gComm_Data_LED_Voltage_Interval_Set(-1);
            last_bias_1300 = 0x80000000;          /* 初始化历史值 */
            gComm_Data_LED_Voltage_Points_Set(1); /* 点数设为 1 */
            if (max != 0xFFFFFF) {
                return 0;
            }
            return 1;
        } else {
            if (sign > 0) {
                sign *= -1;
            }
            gComm_Data_LED_Voltage_Interval_Set(sign);
            bias_1300 = 0x7fffffff;
            last_bias_1300 = bias_1300;
            return 1;
        }
    }

    if (idx == 0) { /* 首次进入 */
        if (bias_1300 < 0) {
            sign = 1;
        } else {
            sign = -1;
        }
        switch (radiant) {
            case eComm_Data_Sample_Radiant_610:
                gComm_Data_LED_Voltage_Interval_Set(sign * COMM_DATA_LED_VOLTAGE_UNIT_610);
                break;
            case eComm_Data_Sample_Radiant_550:
                gComm_Data_LED_Voltage_Interval_Set(sign * COMM_DATA_LED_VOLTAGE_UNIT_550);
                break;
            case eComm_Data_Sample_Radiant_405:
                gComm_Data_LED_Voltage_Interval_Set(sign * COMM_DATA_LED_VOLTAGE_UNIT_405);
                break;
        }
        last_bias_1300 = bias_1300;
        return 1;
    }
    if ((bias_1300 ^ last_bias_1300) < 0) { /* 符号不同 */
        sign = -1 * gComm_Data_LED_Voltage_Interval_Get() / 2;
        if (sign == 0) {
            gComm_Data_LED_Voltage_Interval_Set(-1);
            last_bias_1300 = 0x80000000;          /* 初始化历史值 */
            gComm_Data_LED_Voltage_Points_Set(1); /* 点数设为 1 */
            result = 0;
        } else {
            gComm_Data_LED_Voltage_Interval_Set(sign);
            result = 1;
        }
    } else if (idx >= 2) {
        cal_inter =
            (gComm_Data_LED_SP_Record[2].adc_avg - gComm_Data_LED_SP_Record[1].adc_avg) / (gComm_Data_LED_SP_Record[2].dac - gComm_Data_LED_SP_Record[1].dac);
        if ((14000000 + gComm_Data_LED_SP_Record[2].adc_avg) > (13000000 + max)) {
            cal_inter = (13000000.0 - gComm_Data_LED_SP_Record[2].adc_avg) / cal_inter;
        } else {
            cal_inter = (14000000.0 - max) / cal_inter;
        }

        cal_inter *= 0.9;
        cal_inter += (cal_inter > 0) ? (0.5) : (-0.5);

        if (fabs(cal_inter) > 2) {
            if (cal_inter > 200) {
                cal_inter = 200;
            } else if (cal_inter < -200) {
                cal_inter = -200;
            }
            gComm_Data_LED_Voltage_Interval_Set(cal_inter);
        }
    }
    last_bias_1300 = bias_1300;
    return result;
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
 * @note   500 mS 回调一次 优先级置于操作系统之上 不允许调用系统API
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
        gComm_Data_Samples[i].conf.assay = pData[3 * i + 0];                                    /* 测试方法 */
        gComm_Data_Samples[i].conf.radiant = pData[3 * i + 1];                                  /* 测试波长 */
        if (gComm_Data_Samples[i].conf.assay >= eComm_Data_Sample_Assay_None &&                 /* 测试方法范围内 */
            gComm_Data_Samples[i].conf.assay <= eComm_Data_Sample_Assay_Fixed &&                /* and */
            gComm_Data_Samples[i].conf.radiant >= eComm_Data_Sample_Radiant_610 &&              /* 测试波长范围内 */
            gComm_Data_Samples[i].conf.radiant <= eComm_Data_Sample_Radiant_405 &&              /* and */
            pData[3 * i + 2] > 0 && pData[3 * i + 2] <= 120) {                                  /* 测试点数范围内 */
            if (i > 0 && gComm_Data_Samples[i].conf.radiant == eComm_Data_Sample_Radiant_405) { /* 通道 2～6 没有405 */
                gComm_Data_Samples[i].conf.points_num = 0;                                      /* 清除点数 */
                gComm_Data_Samples[i].conf.assay = eComm_Data_Sample_Assay_None;                /* 清除项目 */
                pData[3 * i + 0] = 0;                                                           /* 修正原始数据 */
                pData[3 * i + 2] = 0;                                                           /* 修正原始数据 */
            } else {
                gComm_Data_Samples[i].conf.points_num = pData[3 * i + 2]; /* 设置测试点数 */
            }
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
        gComm_Data_Samples[i].conf.assay = pData[3 * i + 0];                                    /* 测试方法 */
        gComm_Data_Samples[i].conf.radiant = pData[3 * i + 1];                                  /* 测试波长 */
        if (gComm_Data_Samples[i].conf.assay >= eComm_Data_Sample_Assay_None &&                 /* 测试方法范围内 */
            gComm_Data_Samples[i].conf.assay <= eComm_Data_Sample_Assay_Fixed &&                /* and */
            gComm_Data_Samples[i].conf.radiant >= eComm_Data_Sample_Radiant_610 &&              /* 测试波长范围内 */
            gComm_Data_Samples[i].conf.radiant <= eComm_Data_Sample_Radiant_405 &&              /* and */
            pData[3 * i + 2] > 0 && pData[3 * i + 2] <= 120) {                                  /* 测试点数范围内 */
            if (i > 0 && gComm_Data_Samples[i].conf.radiant == eComm_Data_Sample_Radiant_405) { /* 通道 2～6 没有405 */
                gComm_Data_Samples[i].conf.points_num = 0;                                      /* 清除点数 */
                gComm_Data_Samples[i].conf.assay = eComm_Data_Sample_Assay_None;                /* 清除项目 */
                pData[3 * i + 0] = 0;                                                           /* 修正原始数据 */
                pData[3 * i + 2] = 0;                                                           /* 修正原始数据 */
            } else {
                gComm_Data_Samples[i].conf.points_num = pData[3 * i + 2]; /* 设置测试点数 */
            }
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

    gComm_Data_AgingLoop_Mode_Set(0);
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

    gComm_Data_AgingLoop_Mode_Set(0);
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

    gComm_Data_AgingLoop_Mode_Set(pData[18]);
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

    gComm_Data_AgingLoop_Mode_Set(pData[18]);
    comm_Data_Sample_Apply_Conf_FromISR(pData);
    sendLength = buildPackOrigin(eComm_Data, eComm_Data_Outbound_CMD_TEST, pData, 19); /* 构造工装测试配置包 */
    return comm_Data_SendTask_QueueEmit_FromISR(pData, sendLength);
}

/**
 * @brief  发送采样配置 定标
 * @param  wave 波长配置  405不做定标
 * @param  pData 缓存指针
 * @param  point_num 点数
 * @param  cmd_type 命令类型 (eComm_Data_Outbound_CMD_CONF, eComm_Data_Outbound_CMD_TEST)
 * @retval pdPASS 提交成功 pdFALSE 提交失败
 */
BaseType_t comm_Data_Sample_Send_Conf_Correct(uint8_t * pData, eComm_Data_Sample_Radiant wave, uint8_t point_num, uint8_t cmd_type)
{
    uint8_t i, sendLength;

    gComm_Data_Sample_Max_Point_Clear(); /* 清除最大点数 */
    for (i = 0; i < 6; ++i) {
        if (wave == eComm_Data_Sample_Radiant_405 && i > 0) { /* 通道2～6没有405 */
            pData[0 + 3 * i] = eComm_Data_Sample_Assay_None;  /* 测试方法 */
            pData[2 + 3 * i] = 0;                             /* 测试点数 point_num */
        } else {
            pData[0 + 3 * i] = eComm_Data_Sample_Assay_Continuous; /* 测试方法 */
            pData[2 + 3 * i] = point_num;                          /* 测试点数 point_num */
        }
        pData[1 + 3 * i] = wave;                              /* 测试波长 */
        gComm_Data_Sample_Max_Point_Update(pData[2 + 3 * i]); /* 更新最大点数 */
    }
    gComm_Data_Correct_wave = wave;
    if (cmd_type == eComm_Data_Outbound_CMD_CONF) {
        gComm_Data_AgingLoop_Mode_Set(0);
        sendLength = buildPackOrigin(eComm_Data, eComm_Data_Outbound_CMD_CONF, pData, 18); /* 构造普通测试配置包 */
    } else if (cmd_type == eComm_Data_Outbound_CMD_TEST) {
        if (gComm_Data_Correct_Flag_Check()) { /* 定标状态 */
            pData[18] = 3;                     /* MIX */
        } else {
            pData[18] = 2; /* PD */
        }
        gComm_Data_AgingLoop_Mode_Set(pData[18]);
        sendLength = buildPackOrigin(eComm_Data, eComm_Data_Outbound_CMD_TEST, pData, 19); /* 构造工装测试配置包 */
    }
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
 * @brief  校正段索引配置获取
 * @param  uint8_t channel 通道号 1 ~ 6
 * @retval gComm_Data_Correct_stages
 */
uint8_t comm_Data_Get_Corretc_Stage(uint8_t channel)
{
    return gComm_Data_Correct_stages[channel - 1];
}

/**
 * @brief  校正段索引配置设置
 * @param  uint8_t channel 通道号 1 ~ 6
 * @param  uint8_t idx 校正段索引 0 ~ 5
 * @retval gComm_Data_Correct_stages
 */
void comm_Data_Set_Corretc_Stage(uint8_t channel, uint8_t idx)
{
    gComm_Data_Correct_stages[channel - 1] = idx;
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
 * @brief  发送上次采样配置 中断版本
 * @note   用于老化测试采样
 * @param  None
 * @retval pdPASS 提交成功 pdFALSE 提交失败
 */
BaseType_t comm_Data_Sample_Send_Conf_Re(void)
{
    uint8_t i, sendLength, pData[30];

    for (i = 0; i < ARRAY_LEN(gComm_Data_Samples); ++i) {
        pData[3 * i + 0] = gComm_Data_Samples[i].conf.assay;      /* 测试方法 */
        pData[3 * i + 1] = gComm_Data_Samples[i].conf.radiant;    /* 测试波长 */
        pData[3 * i + 2] = gComm_Data_Samples[i].conf.points_num; /* 测试点数 */
        gComm_Data_Sample_Max_Point_Update(pData[3 * i + 2]);     /* 更新最大点数 */
    }
    if (gComm_Data_AgingLoop_Mode_Get() == 0) {
        sendLength = buildPackOrigin(eComm_Data, eComm_Data_Outbound_CMD_CONF, pData, 18); /* 构造测试配置包 */
    } else {
        pData[18] = gComm_Data_AgingLoop_Mode_Get();
        sendLength = buildPackOrigin(eComm_Data, eComm_Data_Outbound_CMD_TEST, pData, 19); /* 构造工装测试配置包 */
    }
    return comm_Data_SendTask_QueueEmit(pData, sendLength, 50);
}

/**
 * @brief  采样板LED电压获取
 * @note   采样板LED电压获取
 * @param  None
 * @retval pdPASS 提交成功 pdFALSE 提交失败
 */
BaseType_t comm_Data_Conf_LED_Voltage_Get(void)
{
    uint8_t sendLength, pData[8];

    sendLength = buildPackOrigin(eComm_Data, eComm_Data_Outbound_CMD_LED_GET, pData, 0); /* 构造测试配置包 */
    return comm_Data_SendTask_QueueEmit(pData, sendLength, 50);
}

/**
 * @brief  采样板LED电压获取 中断版本
 * @note   采样板LED电压获取
 * @param  None
 * @retval pdPASS 提交成功 pdFALSE 提交失败
 */
BaseType_t comm_Data_Conf_LED_Voltage_Get_FromISR(void)
{
    uint8_t sendLength, pData[8];

    sendLength = buildPackOrigin(eComm_Data, eComm_Data_Outbound_CMD_LED_GET, pData, 0); /* 构造测试配置包 */
    return comm_Data_SendTask_QueueEmit_FromISR(pData, sendLength);
}

/**
 * @brief  采样板LED电压设置
 * @note   采样板LED电压设置
 * @param  * pData 电压配置数组地址 uint16_t array[3]
 * @retval pdPASS 提交成功 pdFALSE 提交失败
 */
BaseType_t comm_Data_Conf_LED_Voltage_Set(uint8_t * pData)
{
    uint8_t sendLength;

    sendLength = buildPackOrigin(eComm_Data, eComm_Data_Outbound_CMD_LED_SET, pData, 6); /* 构造测试配置包 */
    return comm_Data_SendTask_QueueEmit(pData, sendLength, 50);
}

/**
 * @brief  采样板LED电压设置 中断版本
 * @note   采样板LED电压设置
 * @param  * pData 电压配置数组地址 uint16_t array[3]
 * @retval pdPASS 提交成功 pdFALSE 提交失败
 */
BaseType_t comm_Data_Conf_LED_Voltage_Set_FromISR(uint8_t * pData)
{
    uint8_t sendLength;

    sendLength = buildPackOrigin(eComm_Data, eComm_Data_Outbound_CMD_LED_SET, pData, 6); /* 构造测试配置包 */
    return comm_Data_SendTask_QueueEmit_FromISR(pData, sendLength);
}

/**
 * @brief  采样板工装PD测试设置 中断版本
 * @note   采样板工装PD测试设置
 * @param  * pData 电测试设置参数 uint8_t array[1]  0 结束 1 启动
 * @retval pdPASS 提交成功 pdFALSE 提交失败
 */
BaseType_t comm_Data_Conf_FA_PD_Set_FromISR(uint8_t * pData)
{
    uint8_t sendLength;

    sendLength = buildPackOrigin(eComm_Data, eComm_Data_Outbound_CMD_FA_PD_SET, pData, 1); /* 构造测试配置包 */
    return comm_Data_SendTask_QueueEmit_FromISR(pData, sendLength);
}

/**
 * @brief  采样板工装LED状态设置 中断版本
 * @note   采样板工装LED状态设置
 * @param  * pData LED设置参数 uint8_t array[2]  LED掩码
 * @retval pdPASS 提交成功 pdFALSE 提交失败
 */
BaseType_t comm_Data_Conf_FA_LED_Set_FromISR(uint8_t * pData)
{
    uint8_t sendLength;

    sendLength = buildPackOrigin(eComm_Data, eComm_Data_Outbound_CMD_FA_LED_SET, pData, 2); /* 构造测试配置包 */
    return comm_Data_SendTask_QueueEmit_FromISR(pData, sendLength);
}

/**
 * @brief  采样板杂散光获取 中断版本
 * @note   采样板杂散光获取
 * @param  None
 * @retval pdPASS 提交成功 pdFALSE 提交失败
 */
BaseType_t comm_Data_Conf_Offset_Get_FromISR(void)
{
    uint8_t sendLength, pData[8];

    sendLength = buildPackOrigin(eComm_Data, eComm_Data_Outbound_CMD_OFFSET_GET, pData, 0); /* 构造测试配置包 */
    return comm_Data_SendTask_QueueEmit_FromISR(pData, sendLength);
}

/**
 * @brief  采样板数据包转发 中断版本
 * @note   数据包长度至少为7
 * @param  pData 原始数据包指针
 * @param  length 原始数据包长度
 * @retval pdPASS 提交成功 pdFALSE 提交失败
 */
BaseType_t comm_Data_Transit_FromISR(uint8_t * pData, uint8_t length)
{
    uint8_t sendLength;

    if (length < 7) {
        return pdFALSE;
    }

    sendLength = buildPackOrigin(eComm_Data, pData[5], pData + 6, length - 7); /* 构造测试配置包 */
    return comm_Data_SendTask_QueueEmit_FromISR(pData + 6, sendLength);
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
    comm_Data_Stary_Test_Mark();                                                       /* 先行标记杂散光测试开始 */
    if (comm_Data_SendTask_QueueEmit(pData, sendLength, 50) != pdPASS) {               /* 加入队列失败 */
        comm_Data_Stary_Test_Clear();                                                  /* 清除标记 */
        return pdFAIL;
    }
    return pdPASS;
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
    if (xResult == pdTRUE) {
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
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
 * @param  replace 0 不替换 1 替换
 * @retval 变换结果 eComm_Data_Sample_Data
 */
eComm_Data_Sample_Data comm_Data_Sample_Data_Commit(uint8_t channel, uint8_t * pBuffer, uint8_t length, uint8_t replace)
{
    uint8_t result;
    uint16_t buffer16[20];
    uint32_t input, output_32;

    if (channel < 1 || channel > 6) { /* 检查通道编码 */
        return eComm_Data_Sample_Data_ERROR;
    }
    if (replace == 0 && gComm_Data_Samples[channel - 1].num > 0) {
        return eComm_Data_Sample_Data_ERROR;
    }

    gComm_Data_Samples[channel - 1].num = pBuffer[6]; /* 数据个数 u16 | u32 */

    if (length == pBuffer[6] * 2) {                    /* u16 */
        gComm_Data_Samples[channel - 1].data_type = 2; /* 数据类型标识 */
        result = eComm_Data_Sample_Data_U16;
    } else if (length == pBuffer[6] * 4) {             /* u32 */
        gComm_Data_Samples[channel - 1].data_type = 4; /* 数据类型标识 */
        result = eComm_Data_Sample_Data_U32;
    } else if (length == pBuffer[6] * 10) {                                     /* 混合类型 */
        gComm_Data_Samples[channel - 1].data_type = 12;                         /* 数据类型标识 */
        memcpy(gComm_Data_Samples[channel - 1].raw_datas, pBuffer + 8, length); /* 复制 */
        for (result = 0; result < pBuffer[6]; ++result) {
            input = *((uint16_t *)(pBuffer + 8 + 8 + 10 * result));                                            /* 实际采样值 */
            sample_first_degree_cal(channel, gComm_Data_Samples[channel - 1].conf.radiant, input, &output_32); /* 线性投影 */
            buffer16[result] = (uint16_t)(output_32);                                                          /* 存入缓存 */
        }
        for (result = 0; result < pBuffer[6]; ++result) {
            memcpy(gComm_Data_Samples[channel - 1].raw_datas + 12 * result, pBuffer + 8 + 10 * result, 10);          /* 复制原始值 */
            memcpy(gComm_Data_Samples[channel - 1].raw_datas + 12 * result + 10, (uint8_t *)(&buffer16[result]), 2); /* 补充校正值 */
        }
        memcpy(pBuffer + 8, gComm_Data_Samples[channel - 1].raw_datas, length / 10 * 12);
        return eComm_Data_Sample_Data_MIX;
    } else {                                           /* 异常长度 */
        gComm_Data_Samples[channel - 1].data_type = 1; /* 数据类型标识 */
        result = eComm_Data_Sample_Data_UNKNOW;
    }
    memcpy(gComm_Data_Samples[channel - 1].raw_datas, pBuffer + 8, length); /* 原封不动复制 */
    return result;
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
    comm_Data_RecordInit();
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
    if (xResult == pdTRUE) {
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
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
    if (serialSourceFlagsWait(eSerial_Source_COMM_Data_Send_Buffer_Bit, 10) == pdFALSE) {
        error_Emit(eError_Comm_Data_Source_Lock);
        return pdFALSE;
    }
    memcpy(gComm_Data_SendInfo.buff, pData, length);
    gComm_Data_SendInfo.length = length;

    xResult = xQueueSendToBack(comm_Data_SendQueue, &gComm_Data_SendInfo, pdMS_TO_TICKS(timeout));
    serialSourceFlagsSet(eSerial_Source_COMM_Data_Send_Buffer_Bit);
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
    BaseType_t xResult, xHigherPriorityTaskWoken = pdFALSE;

    if (length == 0 || pData == NULL) { /* 数据有效性检查 */
        return pdFALSE;
    }
    if (serialSourceFlagsGet_FromISR() && eSerial_Source_COMM_Main_Send_Buffer_ISR_Bit == 0) {
        error_Emit_FromISR(eError_Comm_Data_Source_Lock);
        return pdFALSE;
    }
    serialSourceFlagsClear_FromISR(eSerial_Source_COMM_Main_Send_Buffer_ISR_Bit);

    memcpy(gComm_Data_SendInfo_FromISR.buff, pData, length);
    gComm_Data_SendInfo_FromISR.length = length;

    xResult = xQueueSendToBackFromISR(comm_Data_SendQueue, &gComm_Data_SendInfo_FromISR, &xHigherPriorityTaskWoken);
    serialSourceFlagsSet_FromISR(eSerial_Source_COMM_Main_Send_Buffer_ISR_Bit);
    if (xResult != pdPASS) {
        error_Emit_FromISR(eError_Comm_Data_Busy);
    } else {
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
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
        heater_BTM_Output_Start();                                                       /* 恢复下加热体 */
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
        } else if (gComm_Data_Lamp_BP_Flag_Check()) {
            motor_Sample_Info_From_ISR(eMotorNotifyValue_LAMP_BP); /* 通知电机任务杂散光测试完成 */
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
    heater_BTM_Output_Stop(); /* 关闭下加热体 */
    if (wp == 0) {
        HAL_GPIO_WritePin(FRONT_STATUS_GPIO_Port, FRONT_STATUS_Pin, GPIO_PIN_RESET); /* 采样输出脚拉低 下降沿 白板 */
    } else {
        HAL_GPIO_WritePin(FRONT_STATUS_GPIO_Port, FRONT_STATUS_Pin, GPIO_PIN_SET); /* 采样输出脚拉低 上升沿 PD */
    }
}

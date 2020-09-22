/**
 * @file    motor.c
 * @brief   电机资源管理
 * @note    白板电机 功能完全独立 运动和上加热体电机竞争（共用脉冲输出）
 * @note    扫码电机 运动完全独立 功能受托盘电机限制
 * @note    托盘电机 运动受上加热体电机限制
 * @note    上加热体电机 功能完全独立 运动和白板电机竞争（共用脉冲输出）
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "motor.h"
#include "barcode_scan.h"
#include "m_l6470.h"
#include "m_drv8824.h"
#include "tray_run.h"
#include "heat_motor.h"
#include "white_motor.h"
#include "protocol.h"
#include "comm_out.h"
#include "comm_main.h"
#include "comm_data.h"
#include "soft_timer.h"
#include "beep.h"
#include "led.h"
#include "storge_task.h"
#include "temperature.h"
#include "heater.h"
#include "fan.h"

/* Extern variables ----------------------------------------------------------*/
extern TIM_HandleTypeDef htim6;
extern TIM_HandleTypeDef htim7;

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/
typedef struct {
    uint8_t cnt_on;
    uint8_t threshold_on;
    uint8_t cnt_off;
    uint8_t threshold_off;
    eMotor_OPT_Status status_result;
    eMotor_OPT_Status (*opt_status_get)(void);
} sMotor_OPT_Record;

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
xQueueHandle motor_Fun_Queue_Handle = NULL; /* 电机功能队列 */
xTaskHandle motor_Task_Handle = NULL;       /* 电机任务句柄 */

static sMotor_OPT_Record gMotor_OPT_Records[eMotor_OPT_Index_NUM];   /* 光耦记录 */
static uint8_t gMotorPressureStopBits = 0xFF;                        /* 压力测试停止标志位 */
static uint8_t gMotorTempStableWaiting = 0;                          /* 启动时等待温度稳定标志位 */
static eMotor_Sampl_Comm gMotor_Sampl_Comm = eMotor_Sampl_Comm_None; /* 采样时指令来源 */
static uint8_t gMotor_Aging_Sleep = 10;                              /* 老化测试出仓后等待间隔 单位 秒 */

/* Private function prototypes -----------------------------------------------*/
static void motor_Task(void * argument);
static void motor_Tray_Move_By_Index(eTrayIndex index);

static uint8_t motor_Sample_Deal(uint8_t normal_report);

static eMotor_OPT_Status motor_OPT_Status_Get_Scan(void);
static eMotor_OPT_Status motor_OPT_Status_Get_Tray_Scan(void);
static eMotor_OPT_Status motor_OPT_Status_Get_Heater(void);

static void motor_Self_Check_Motor_White(uint8_t * pBuffer);
static void motor_Self_Check_Motor_Heater(uint8_t * pBuffer);
static void motor_Self_Check_Motor_Tray(uint8_t * pBuffer);
static void motor_Self_Check_Motor_Scan(uint8_t * pBuffer);
static void motor_Self_Check_Scan(uint8_t * pBuffer);
static void motor_Self_Check_PD(uint8_t * pBuffer, uint8_t mask);
static void motor_Self_Check_FAN(uint8_t * pBuffer);

static void motor_Stary_Test(void);

static void motor_Sample_Temperature_Check(void);
static uint8_t motor_Sample_Barcode_Scan(void);

/* Private constants ---------------------------------------------------------*/
const eMotor_OPT_Status (*gOPT_Status_Get_Funs[])(void) = {motor_OPT_Status_Get_Scan,   motor_OPT_Status_Get_Tray,     motor_OPT_Status_Get_Tray_Scan,
                                                           motor_OPT_Status_Get_Heater, motor_OPT_Status_Get_White_In, motor_OPT_Status_Get_White_Out};

/* Private user code ---------------------------------------------------------*/
/**
 * @brief  软定时器光耦状态硬件读取 扫码电机
 * @param  None
 * @retval 光耦状态
 */
static eMotor_OPT_Status motor_OPT_Status_Get_Scan(void)
{
    if (HAL_GPIO_ReadPin(OPTSW_OUT0_GPIO_Port, OPTSW_OUT0_Pin) == GPIO_PIN_RESET) {
        return eMotor_OPT_Status_OFF;
    }
    return eMotor_OPT_Status_ON;
}

/**
 * @brief  软定时器光耦状态硬件读取 托盘电机
 * @param  None
 * @retval 光耦状态
 */
eMotor_OPT_Status motor_OPT_Status_Get_Tray(void)
{
    if (HAL_GPIO_ReadPin(OPTSW_OUT1_GPIO_Port, OPTSW_OUT1_Pin) == GPIO_PIN_RESET) {
        return eMotor_OPT_Status_OFF;
    }
    return eMotor_OPT_Status_ON;
}

/**
 * @brief  软定时器光耦状态硬件读取 托盘电机扫码位置
 * @param  None
 * @retval 光耦状态
 */
static eMotor_OPT_Status motor_OPT_Status_Get_Tray_Scan(void)
{
    if (HAL_GPIO_ReadPin(OPTSW_OUT2_GPIO_Port, OPTSW_OUT2_Pin) == GPIO_PIN_RESET) {
        return eMotor_OPT_Status_OFF;
    }
    return eMotor_OPT_Status_ON;
}

/**
 * @brief  软定时器光耦状态硬件读取 上加热体电机
 * @param  None
 * @retval 光耦状态
 */
static eMotor_OPT_Status motor_OPT_Status_Get_Heater(void)
{
    if (HAL_GPIO_ReadPin(OPTSW_OUT3_GPIO_Port, OPTSW_OUT3_Pin) == GPIO_PIN_RESET) {
        return eMotor_OPT_Status_OFF;
    }
    return eMotor_OPT_Status_ON;
}

/**
 * @brief  软定时器光耦状态硬件读取 白板电机
 * @param  None
 * @retval 光耦状态
 */
eMotor_OPT_Status motor_OPT_Status_Get_White_In(void)
{
    if (HAL_GPIO_ReadPin(OPTSW_OUT4_GPIO_Port, OPTSW_OUT4_Pin) == GPIO_PIN_RESET) {
        return eMotor_OPT_Status_OFF;
    }
    return eMotor_OPT_Status_ON;
}

/**
 * @brief  软定时器光耦状态硬件读取 白板电机
 * @param  None
 * @retval 光耦状态
 */
eMotor_OPT_Status motor_OPT_Status_Get_White_Out(void)
{
    if (HAL_GPIO_ReadPin(OPTSW_OUT5_GPIO_Port, OPTSW_OUT5_Pin) == GPIO_PIN_RESET) {
        return eMotor_OPT_Status_OFF;
    }
    return eMotor_OPT_Status_ON;
}

/**
 * @brief  压力测试停止标志位
 * @param  fun 压力测试项目
 * @retval 1 停止 0 继续
 */
uint8_t gMotorPressureStopBits_Get(eMotor_Fun fun)
{
    switch (fun) {
        case eMotor_Fun_PRE_TRAY:
            return gMotorPressureStopBits & (1 << 0);
        case eMotor_Fun_PRE_BARCODE:
            return gMotorPressureStopBits & (1 << 1);
        case eMotor_Fun_PRE_HEATER:
            return gMotorPressureStopBits & (1 << 2);
        case eMotor_Fun_PRE_WHITE:
            return gMotorPressureStopBits & (1 << 3);
        case eMotor_Fun_PRE_ALL:
            return gMotorPressureStopBits & (1 << 4);
        default:
            return 0;
    }
    return 0;
}

/**
 * @brief  压力测试停止标志位
 * @param  fun 压力测试项目
 * @param  b 1 停止 0 继续
 * @retval None
 */
void gMotorPressureStopBits_Set(eMotor_Fun fun, uint8_t b)
{
    switch (fun) {
        case eMotor_Fun_PRE_TRAY:
            if (b > 0) {
                gMotorPressureStopBits |= (1 << 0);
            } else {
                gMotorPressureStopBits &= (0xFF - (1 << 0));
            }
            break;
        case eMotor_Fun_PRE_BARCODE:
            if (b > 0) {
                gMotorPressureStopBits |= (1 << 1);
            } else {
                gMotorPressureStopBits &= (0xFF - (1 << 1));
            }
            break;
        case eMotor_Fun_PRE_HEATER:
            if (b > 0) {
                gMotorPressureStopBits |= (1 << 2);
            } else {
                gMotorPressureStopBits &= (0xFF - (1 << 2));
            }
            break;
        case eMotor_Fun_PRE_WHITE:
            if (b > 0) {
                gMotorPressureStopBits |= (1 << 3);
            } else {
                gMotorPressureStopBits &= (0xFF - (1 << 3));
            }
            break;
        case eMotor_Fun_PRE_ALL:
            if (b > 0) {
                gMotorPressureStopBits |= (1 << 4);
            } else {
                gMotorPressureStopBits &= (0xFF - (1 << 4));
            }
            break;
        default:
            break;
    }
}

/**
 * @brief  压力测试停止标志位 复归
 * @param  None
 * @retval None
 */
void gMotorPressureStopBits_Clear(void)
{
    gMotorPressureStopBits = 0xFF;
}

/**
 * @brief  压力测试停止标志位 复归
 * @param  None
 * @retval None
 */
uint8_t gMotorPressure_IsDoing(void)
{
    return gMotorPressureStopBits != 0xFF;
}

/**
 * @brief  温度稳定等待标志位 标记
 * @param  None
 * @retval None
 */
void gMotorTempStableWaiting_Mark(void)
{
    gMotorTempStableWaiting = 1;
}

/**
 * @brief  温度稳定等待标志位 清零
 * @param  None
 * @retval None
 */
void gMotorTempStableWaiting_Clear(void)
{
    gMotorTempStableWaiting = 0;
}

/**
 * @brief  温度稳定等待标志位 检查
 * @param  None
 * @retval 1 等待中 0 已结束
 */
uint8_t gMotorTempStableWaiting_Check(void)
{
    return (gMotorTempStableWaiting > 0) ? (1) : (0);
}

/**
 * @brief  采样时指令来源 读取
 * @retval 1 停止 0 继续
 */
eMotor_Sampl_Comm gMotor_Sampl_Comm_Get(void)
{
    return gMotor_Sampl_Comm;
}

/**
 * @brief  采样时指令来源 设置
 * @param  b eMotor_Sampl_Comm
 * @retval None
 */
void gMotor_Sampl_Comm_Set(eMotor_Sampl_Comm b)
{
    gMotor_Sampl_Comm = b;
}

/**
 * @brief  老化循环测试等待间隔 获取
 * @param  None
 * @retval 老化循环测试等待间隔
 */
uint8_t gMotor_Aging_Sleep_Get(void)
{
    return gMotor_Aging_Sleep;
}

/**
 * @brief  老化循环测试等待间隔 设置
 * @param  sleep 等待时间 单位 秒
 * @retval None
 */
void gMotor_Aging_Sleep_Set(uint8_t sleep)
{
    gMotor_Aging_Sleep = sleep;
}

/**
 * @brief  采样时指令来源 复位
 * @retval None
 */
void gMotor_Sampl_Comm_Init(void)
{
    gMotor_Sampl_Comm_Set(eMotor_Sampl_Comm_None);
}

/**
 * @brief  软定时器光耦状态初始化
 * @param  None
 * @retval None
 */
void motor_OPT_Status_Init(void)
{
    uint8_t i;
    for (i = 0; i < ARRAY_LEN(gMotor_OPT_Records); ++i) {
        gMotor_OPT_Records[i].cnt_on = 0;                               /* 断开计数 */
        gMotor_OPT_Records[i].cnt_off = 0;                              /* 遮挡计数 */
        gMotor_OPT_Records[i].threshold_on = 2;                         /* 断开次数阈值 */
        gMotor_OPT_Records[i].threshold_off = 2;                        /* 遮挡次数阈值 */
        gMotor_OPT_Records[i].status_result = eMotor_OPT_Status_None;   /* 初始状态 */
        gMotor_OPT_Records[i].opt_status_get = gOPT_Status_Get_Funs[i]; /* 硬件层光耦状态读取 */
    }
    gMotor_OPT_Records[eMotor_OPT_Index_Heater].threshold_on = 6;  /* 断开次数阈值 */
    gMotor_OPT_Records[eMotor_OPT_Index_Heater].threshold_off = 6; /* 遮挡次数阈值 */
}

/**
 * @brief  软定时器光耦状态初始化 等待完成
 * @param  None
 * @retval None
 */
uint8_t motor_OPT_Status_Init_Wait_Complete(void)
{
    uint8_t i, result, cnt;

    for (cnt = 0; cnt < 10; ++cnt) {
        result = ARRAY_LEN(gMotor_OPT_Records);
        for (i = 0; i < ARRAY_LEN(gMotor_OPT_Records); ++i) {
            if (gMotor_OPT_Records[i].status_result == eMotor_OPT_Status_None) {
                --result;
            }
        }
        if (result == ARRAY_LEN(gMotor_OPT_Records)) {
            return 0;
        }
        vTaskDelay(10);
    }
    return 1;
}

/**
 * @brief  软定时器光耦状态更新
 * @note   http://www.emcu.it/STM32/STM32Discovery-Debounce/STM32Discovery-InputWithDebounce_Output_UART_SPI_SysTick.html
 * @param  None
 * @retval None
 */
void motor_OPT_Status_Update(void)
{
    uint8_t i;
    eMotor_OPT_Status status_current;

    for (i = 0; i < ARRAY_LEN(gMotor_OPT_Records); ++i) {
        status_current = gMotor_OPT_Records[i].opt_status_get(); /* 读取当前光耦状态 */
        if (status_current == eMotor_OPT_Status_ON) {
            gMotor_OPT_Records[i].cnt_on++;
            gMotor_OPT_Records[i].cnt_off = 0;
            if (gMotor_OPT_Records[i].cnt_on >= gMotor_OPT_Records[i].threshold_on) {
                gMotor_OPT_Records[i].cnt_on = gMotor_OPT_Records[i].threshold_on + 1;
                gMotor_OPT_Records[i].status_result = eMotor_OPT_Status_ON;
            }
        } else {
            gMotor_OPT_Records[i].cnt_off++;
            gMotor_OPT_Records[i].cnt_on = 0;
            if (gMotor_OPT_Records[i].cnt_off >= gMotor_OPT_Records[i].threshold_off) {
                gMotor_OPT_Records[i].cnt_off = gMotor_OPT_Records[i].threshold_off + 1;
                gMotor_OPT_Records[i].status_result = eMotor_OPT_Status_OFF;
            }
        }
    }
}

/**
 * @brief  软定时器光耦状态结果读取
 * @param  idx 光耦索引
 * @retval 光耦状态 去抖后
 */
eMotor_OPT_Status motor_OPT_Status_Get(eMotor_OPT_Index idx)
{
    if (idx >= eMotor_OPT_Index_NUM) {
        return eMotor_OPT_Status_OFF;
    }
    return gMotor_OPT_Records[idx].status_result;
}

/**
 * @brief  启动采样并控制白板电机
 * @param  normal_report 正常退出时报告标志
 * @retval 0 正常结束 1 主动打断 2 被动打断 3 超过最大等待轮次
 */
static uint8_t motor_Sample_Deal(uint8_t normal_report)
{
    uint32_t xNotifyValue = 0;
    BaseType_t xResult = pdFALSE;

    comm_Data_Sample_Start(); /* 启动定时器同步发包 开始采样 */
    for (uint16_t i = 0; i <= gComm_Data_Sample_Max_Point_Get() * 2; ++i) {
        xResult = xTaskNotifyWait(0, 0xFFFFFFFF, &xNotifyValue, pdMS_TO_TICKS(10000)); /* 等待任务通知 */
        if (xResult != pdPASS || xNotifyValue == eMotorNotifyValue_BR) {               /* 超时 或者 收到中终止命令 */
            if (xResult != pdPASS) {                                                   /* 超时  */
                error_Emit(eError_Sample_Incomlete);                                   /* 采样板超时 */
                return 2;
            }
            error_Emit(eError_Sample_Initiative_Break); /* 主动打断 */
            return 1;
        }
        if (xNotifyValue == eMotorNotifyValue_TG) {                 /* 本次采集完成 */
            if (gComm_Data_Sample_PD_WH_Idx_Get() == 1) {           /* 当前检测白物质 */
                white_Motor_PD();                                   /* 运动白板电机 PD位置 */
            } else if (gComm_Data_Sample_PD_WH_Idx_Get() == 2) {    /* 当前检测PD */
                white_Motor_WH();                                   /* 运动白板电机 白物质位置 */
            } else if (gComm_Data_Sample_PD_WH_Idx_Get() == 0xFF) { /* 最后一次采样完成 */
                if (normal_report) {                                /* 正常退出时报告标志 */
                    error_Emit(eError_Sample_Normailly_Exit);       /* 正常退出 */
                }
                return 0;
            }
        }
    }
    if (gComm_Data_Sample_Max_Point_Get() > 0) {
        error_Emit(eError_Sample_Out_Of_Range); /* 超出最大轮次 */
    }
    return 3;
}

/**
 * @brief  电机资源初始化
 * @param  None
 * @retval None
 */
void motor_Resource_Init(void)
{

    uint8_t result;

    result = m_l6470_Init(); /* 驱动资源及参数初始化 */
    if (result & 0x01) {
        error_Emit(eError_Motor_Scan_Status_Warui);
    }
    if (result & 0x10) {
        error_Emit(eError_Motor_Tray_Status_Warui);
    }

    /* 警告 上加热体电机不抬起 不允许操作托盘电机 */
    m_drv8824_Init();                            /* 上加热体电机初始化 */
    tray_Motor_Init();                           /* 托盘电机初始化 */
    barcode_Motor_Init();                        /* 扫码电机初始化 */
    whilte_Motor_Init();                         /* 白板电机初始化 */
    barcode_Motor_Reset_Pos();                   /* 重置扫码电机位置 */
    tray_Motor_Reset_Pos();                      /* 重置托盘电机位置 */
    heat_Motor_Down();                           /* 砸下上加热体 */
    barcode_Motor_Run_By_Index(eBarcodeIndex_6); /* 二位码位置就位 */
}

/**
 * @brief  电机任务初始化
 * @param  None
 * @retval None
 */
void motor_Init(void)
{
    motor_Fun_Queue_Handle = xQueueCreate(1, sizeof(sMotor_Fun));
    if (motor_Fun_Queue_Handle == NULL) {
        Error_Handler();
    }
    if (xTaskCreate(motor_Task, "Motor Task", 288, NULL, TASK_PRIORITY_MOTOR, &motor_Task_Handle) != pdPASS) {
        Error_Handler();
    }
}

/**
 * @brief  电机任务 测量定时器通信
 * @param  info 通信信息
 * @retval pdTRUE 提交成功 pdFALSE 提交失败
 */
BaseType_t motor_Sample_Info_ISR(eMotorNotifyValue info)
{
    BaseType_t xWoken = pdFALSE;

    return xTaskNotifyFromISR(motor_Task_Handle, info, eSetValueWithoutOverwrite, &xWoken);
}

/**
 * @brief  电机任务 测量定时器通信
 * @param  info 通信信息
 * @retval pdTRUE 提交成功 pdFALSE 提交失败
 */
BaseType_t motor_Sample_Info(eMotorNotifyValue info)
{
    if (motor_Task_Handle == NULL) {
        return pdFAIL;
    }

    if (xTaskNotify(motor_Task_Handle, info, eSetValueWithoutOverwrite) != pdPASS) {
        error_Emit(eError_Motor_Notify_No_Read);
        return pdFALSE;
    }
    return pdPASS;
}

/**
 * @brief  电机任务 测量定时器通信
 * @param  info 通信信息
 * @retval pdTRUE 提交成功 pdFALSE 提交失败
 */
BaseType_t motor_Sample_Info_From_ISR(eMotorNotifyValue info)
{
    BaseType_t xk = pdFALSE;

    if (motor_Task_Handle == NULL) {
        return pdFAIL;
    }

    if (xTaskNotifyFromISR(motor_Task_Handle, info, eSetValueWithoutOverwrite, &xk) != pdPASS) {
        error_Emit_FromISR(eError_Motor_Notify_No_Read);
        return pdFALSE;
    }
    return pdPASS;
}

/**
 * @brief  电机任务 提交
 * @param  pFun_type 任务详情指针
 * @param  timeout  最长等待时间
 * @retval 0 提交成功 1 提交失败 2 杂散光测试未结束 3 等待温度稳定中
 */
uint8_t motor_Emit(sMotor_Fun * pFun_type, uint32_t timeout)
{
    if (gMotorTempStableWaiting_Check()) { /* 等待温度稳定中 */
        error_Emit(eError_Temp_BTM_Stable_Waiting);
        return 3;
    }

    if (comm_Data_Stary_Test_Is_Running()) { /* 杂散光测试中 */
        error_Emit(eError_Stary_Doing);
        return 2;
    }
    if (xQueueSendToBack(motor_Fun_Queue_Handle, pFun_type, timeout) != pdPASS) {
        error_Emit(eError_Motor_Task_Busy);
        return 1;
    }
    return 0;
}

/**
 * @brief  电机任务 提交 中断版本
 * @param  pFun_type 任务详情指针
 * @param  timeout  最长等待时间
 * @retval 0 提交成功 1 提交失败 2 杂散光测试未结束 3 等待温度稳定中
 */
uint8_t motor_Emit_FromISR(sMotor_Fun * pFun_type)
{
    if (gMotorTempStableWaiting_Check()) { /* 等待温度稳定中 */
        error_Emit_FromISR(eError_Temp_BTM_Stable_Waiting);
        return 3;
    }

    if (comm_Data_Stary_Test_Is_Running()) { /* 杂散光测试中 */
        error_Emit_FromISR(eError_Stary_Doing);
        return 2;
    }
    if (xQueueSendToBackFromISR(motor_Fun_Queue_Handle, pFun_type, NULL) != pdPASS) {
        error_Emit_FromISR(eError_Motor_Task_Busy);
        return 1;
    }
    return 0;
}

/**
 * @brief  电机任务 托盘运动 附带运动上加热体
 * @note   托盘电机运动前 处理上加热体
 * @param  index 托盘位置索引
 * @retval None
 */
static void motor_Tray_Move_By_Index(eTrayIndex index)
{
    uint8_t buffer[8], flag = 0, opt = 2;

    opt = TRAY_MOTOR_IS_OPT_1; /* 加热体未抬起就读取光耦状态 以免抬起后光耦发生变化 */

    if (heat_Motor_Position_Is_Up() == 0) {
        flag = 1; /* 上加热体电机处于砸下状态 */
    } else {
        flag = 0; /* 上加热体电机处于抬升状态 */
    }
    if (flag && heat_Motor_Up() != 0) { /* 上加热体光耦位置未被阻挡 则抬起上加热体电机 */
        buffer[0] = 0x00;               /* 抬起上加热体失败 */
        comm_Main_SendTask_QueueEmitWithBuildCover(eProtocolRespPack_Client_DISH, buffer, 1); /* 上报失败报文 */
        comm_Out_SendTask_QueueEmitWithModify(buffer, 8, 0);                                  /* 转发至外串口但不允许阻塞 */
        return;
    }

    switch (index) {
        case eTrayIndex_2: /* 出仓方向使能温度调整 */
            heater_Outdoor_Flag_Set(eHeater_BTM, 1);
            heater_Outdoor_Flag_Set(eHeater_TOP, 1);
            break;
        case eTrayIndex_0: /* 入仓方向失能温度调整 */
        case eTrayIndex_1: /* 入仓方向失能温度调整 */
            heater_Outdoor_Flag_Set(eHeater_BTM, 0);
            heater_Outdoor_Flag_Set(eHeater_TOP, 0);
            break;
    }

    /* 托盘保持力矩不足 托盘容易位置会发生变化 实际位置与驱动记录位置不匹配 每次移动托盘电机必须重置 */
    if ((index == eTrayIndex_0 && opt == 0)                        /* 从光耦外回到原点 */
        || (index == eTrayIndex_2 && opt && flag)) {               /* 或者 起点时上加热体砸下 从光耦处离开 */
    } else if ((index == eTrayIndex_1 && flag == 0 && opt == 0)) { /* 从出仓位移动到扫码位置 */
        if (tray_Motor_Scan_Reverse_Get()) {                       /* 侦测到出仓后托盘位移 */
            tray_Motor_Scan_Reverse_Clear();                       /* 清除标志位 */
            tray_Move_By_Index(eTrayIndex_0, 5000);                /* 复归到原点 */
            tray_Move_By_Index(eTrayIndex_2, 5000);                /* 出仓 */
        }
        if (tray_Move_By_Index(eTrayIndex_1, 5000) != eTrayState_OK) {                            /* 运动托盘电机 */
            buffer[0] = 0x00;                                                                     /* 托盘电机运动失败 */
            comm_Main_SendTask_QueueEmitWithBuildCover(eProtocolRespPack_Client_DISH, buffer, 1); /* 上报失败报文 */
            comm_Out_SendTask_QueueEmitWithModify(buffer, 8, 0);                                  /* 转发至外串口但不允许阻塞 */
        }
        return;
    } else {                                                                                      /* 其他情况需要回到原点 */
        tray_Motor_Init();                                                                        /* 托盘电机初始化 */
        if (tray_Motor_Reset_Pos() != 0) {                                                        /* 重置托盘电机位置 */
            buffer[0] = 0x00;                                                                     /* 托盘电机运动失败 */
            comm_Main_SendTask_QueueEmitWithBuildCover(eProtocolRespPack_Client_DISH, buffer, 1); /* 上报失败报文 */
            comm_Out_SendTask_QueueEmitWithModify(buffer, 8, 0);                                  /* 转发至外串口但不允许阻塞 */
            return;
        }
        if (index == eTrayIndex_0) {
            buffer[0] = 0x01;
            comm_Main_SendTask_QueueEmitWithBuildCover(eProtocolRespPack_Client_DISH, buffer, 1);
            comm_Out_SendTask_QueueEmitWithModify(buffer, 8, 0); /* 转发至外串口但不允许阻塞 */
            return;
        }
        vTaskDelay(100);
    }
    if (tray_Move_By_Index(index, 5000) == eTrayState_OK) { /* 运动托盘电机 */
        if (index == eTrayIndex_0) {                        /* 托盘在检测位置 */
            buffer[0] = 0x01;
            comm_Main_SendTask_QueueEmitWithBuildCover(eProtocolRespPack_Client_DISH, buffer, 1);
            comm_Out_SendTask_QueueEmitWithModify(buffer, 8, 0); /* 转发至外串口但不允许阻塞 */
        } else if (index == eTrayIndex_2) {                      /* 托盘在加样位置 */
            buffer[0] = 0x02;
            comm_Main_SendTask_QueueEmitWithBuildCover(eProtocolRespPack_Client_DISH, buffer, 1);
            comm_Out_SendTask_QueueEmitWithModify(buffer, 8, 0); /* 转发至外串口但不允许阻塞 */
        }
        return;
    } else {
        buffer[0] = 0x00;                                                                     /* 托盘电机运动失败 */
        comm_Main_SendTask_QueueEmitWithBuildCover(eProtocolRespPack_Client_DISH, buffer, 1); /* 上报失败报文 */
        comm_Out_SendTask_QueueEmitWithModify(buffer, 8, 0);                                  /* 转发至外串口但不允许阻塞 */
        return;
    }
}

/**
 * @brief  定标 等待参数存储完毕
 * @param  None
 * @retval None
 */
uint8_t motor_Wait_Stroge_Correct(uint32_t timeout)
{
    TickType_t xTick;

    xTick = xTaskGetTickCount();
    do {
        vTaskDelay(100);
        if (gStorgeIllumineCnt_Check(6) == 1) {
            return 1;
        }
    } while (xTaskGetTickCount() - xTick < timeout);
    return 0;
}

/**
 * @brief  定标 采样完成清理
 * @param  None
 * @retval None
 */
void motor_Sample_Owari_Correct(void)
{
    white_Motor_WH();                            /* 运动白板电机 白板位置 */
    barcode_Motor_Run_By_Index(eBarcodeIndex_0); /* 复位 */
    barcode_Motor_Run_By_Index(eBarcodeIndex_6); /* 二维码位置就位 */
    gComm_Data_Sample_Max_Point_Clear();         /* 清除需要测试点数 */
    led_Mode_Set(eLED_Mode_Kirakira_Green);      /* LED 绿灯闪烁 */
    barcode_Interrupt_Flag_Clear();              /* 清除打断标志位 */
    protocol_Temp_Upload_Resume();               /* 恢复温度上送 */
    led_Mode_Set(eLED_Mode_Keep_Green);          /* LED 绿灯常亮 */
    comm_Data_GPIO_Init();                       /* 初始化通讯管脚 */
    gMotor_Sampl_Comm_Init();                    /* 复位来源标记 */
    heater_BTM_Output_Start();                   /* 恢复下加热体 */
}

/**
 * @brief  采样完成清理
 * @param  None
 * @retval None
 */
void motor_Sample_Owari(void)
{
    uint8_t cnt = 0;

    heater_Overshoot_Flag_Set(eHeater_BTM, 0);           /* 取消下加热体过冲加热标志 */
    heater_Overshoot_Flag_Set(eHeater_TOP, 0);           /* 取消上加热体过冲加热标志 */
    while (++cnt < 10 && Miscellaneous_Task_Is_Busy()) { /* 白板电机未完成 */
        vTaskDelay(500);                                 /* 轮询等待 释放白板电机资源 */
    }
    white_Motor_WH();                            /* 运动白板电机 白板位置 */
    if (protocol_Debug_SampleMotorTray() == 0) { /* 非托盘电机调试 */
        motor_Tray_Move_By_Index(eTrayIndex_2);  /* 出仓 */
    }
    heat_Motor_Up();                             /* 采样结束 抬起加热体电机 */
    barcode_Motor_Run_By_Index(eBarcodeIndex_0); /* 复位 */
    barcode_Motor_Run_By_Index(eBarcodeIndex_6); /* 二维码位置就位 */
    gComm_Data_Sample_Max_Point_Clear();         /* 清除需要测试点数 */
    protocol_Temp_Upload_Resume();               /* 恢复温度上送 */
    led_Mode_Set(eLED_Mode_Keep_Green);          /* LED 绿灯常亮 */
    barcode_Interrupt_Flag_Clear();              /* 清除打断标志位 */
    comm_Data_Sample_Owari();                    /* 上送采样结束报文 */
    comm_Data_GPIO_Init();                       /* 初始化通讯管脚 */
    gMotor_Sampl_Comm_Init();                    /* 复位来源标记 */
    heater_BTM_Output_Start();                   /* 恢复下加热体 */
}

/**
 * @brief  杂散光测试
 * @param  None
 * @retval None
 */
static void motor_Stary_Test(void)
{
    uint8_t result, cnt = 0, buffer[12];
    BaseType_t xResult = pdFALSE;
    uint32_t xNotifyValue = 0;
    TickType_t xTick;

    motor_Tray_Move_By_Index(eTrayIndex_1);                       /* 扫码位置 */
    barcode_Scan_QR();                                            /* 扫描二维条码 */
    if (barcode_Scan_Decode_Correct_Info_From_Result() != 0xFF) { /* 杂散光二维码不正确 */
        error_Emit(eError_Stary_QR_Invalid);                      /* 发送异常 杂散光二维码不正确 */
        motor_Tray_Move_By_Index(eTrayIndex_2);                   /* 出仓 */
        return;
    }
    motor_Tray_Move_By_Index(eTrayIndex_0);           /* 入仓 */
    heat_Motor_Down();                                /* 砸下加热体 */
    led_Mode_Set(eLED_Mode_Kirakira_Red);             /* LED 红灯闪烁 */
    gMotorTempStableWaiting_Mark();                   /* 初始化温度稳定等待标记 */
    result = temp_Wait_Stable_BTM(36, 38, 600);       /* 等待下加热体温度稳定 */
    gMotorTempStableWaiting_Clear();                  /* 等待温度稳定结束 */
    if (result == 1) {                                /* 等待温度稳定超时 */
        error_Emit(eError_Temp_BTM_Stable_Timeout);   /* 发送异常 下加热体温度稳定等待超时 */
        error_Emit(eError_Stary_Incomlete);           /* 发送异常 杂散光测试未完成 */
    } else {                                          /* 下加热体温度未稳定 */
        white_Motor_PD();                             /* 白板电机 PD位置 */
        if (comm_Data_Start_Stary_Test() != pdPASS) { /* 开始杂散光测试 */
            cnt = 1;
        } else {
            heater_BTM_Output_Stop();                                                      /* 关闭下加热体 */
            led_Mode_Set(eLED_Mode_Kirakira_Green);                                        /* LED 绿灯闪烁 */
            xTick = xTaskGetTickCount();                                                   /* 起始计时 */
            xResult = xTaskNotifyWait(0, 0xFFFFFFFF, &xNotifyValue, pdMS_TO_TICKS(15000)); /* 等待杂散光完成通知 */
            if (xResult != pdPASS) {                                                       /* 杂散光测试超时 */
                cnt = 2;                                                                   /* 标记 2 */
            } else if (xNotifyValue == eMotorNotifyValue_SP) {                             /* 正常通知量 */
                cnt = 0;                                                                   /* 标记 0 */
            } else {                                                                       /* 异常通知量 */
                cnt = 3;                                                                   /* 标记 3 */
                vTaskDelayUntil(&xTick, pdMS_TO_TICKS(15000));                             /* 补全等待时间 */
            }
            heater_BTM_Output_Start();    /* 恢复下加热体 */
            comm_Data_Stary_Test_Clear(); /* finally 清除杂散光测试标记 */
        }
        if (cnt > 0) {
            error_Emit(eError_Stary_Incomlete); /* 发送异常 杂散光测试未完成 */
        } else {
            buffer[0] = eMotor_Correct_Stary; /* 杂散光测试 */
            buffer[1] = 0;                    /* 正常 */
            comm_Main_SendTask_QueueEmitWithBuild(eProtocolEmitPack_Client_CMD_Correct, buffer, 2, 600);
            comm_Out_SendTask_QueueEmitWithModify(buffer, 2 + 7, 0); /* 转发至外串口但不允许阻塞 */
        }
        white_Motor_WH(); /* 运动白板电机 白板位置 */
    }
    heater_BTM_Output_Start();              /* 恢复下加热体 */
    motor_Tray_Move_By_Index(eTrayIndex_2); /* 出仓 */
    led_Mode_Set(eLED_Mode_Keep_Green);     /* LED 绿灯常亮 */
}

/**
 * @brief  测试过程中温度检查
 * @param  None
 * @retval None
 */
static void motor_Sample_Temperature_Check(void)
{
    float temperature;

    temperature = temp_Get_Temp_Data_BTM();       /* 读取下加热体温度 */
    if (temperature < 36 || temperature > 38) {   /* 不在范围内 */
        error_Emit(eError_Temp_BTM_Not_In_Range); /* 上报提示 */
    }
    temperature = temp_Get_Temp_Data_TOP();       /* 读取上加热体温度 */
    if (temperature < 36 || temperature > 38) {   /* 不在范围内 */
        error_Emit(eError_Temp_TOP_Not_In_Range); /* 上报提示 */
    }

    temperature = temp_Get_Temp_Data_ENV();    /* 读取环境温度 */
    heater_Overshoot_Init(temperature);        /* 根据环境温度配置过冲参数 */
    heater_Overshoot_Flag_Set(eHeater_BTM, 1); /* 下加热体过冲标志设置 */
    heater_Overshoot_Flag_Set(eHeater_TOP, 1); /* 上加热体过冲标志设置 */
}

/**
 * @brief  测试过程中扫码处理
 * @param  None
 * @retval 0 正常 1 中途被打断
 */
static uint8_t motor_Sample_Barcode_Scan(void)
{
    eBarcodeState barcode_result;

    if (protocol_Debug_SampleBarcode() == 0) { /* 非调试模式 */
        if (protocol_Debug_SampleMotorTray() == 0) {
            motor_Tray_Move_By_Index(eTrayIndex_1); /* 扫码位置 */
        }
        barcode_result = barcode_Scan_QR();              /* 扫描二维条码 */
        if (barcode_result == eBarcodeState_Interrupt) { /* 中途打断 */
            error_Emit(eError_Sample_Initiative_Break);  /* 主动打断 */
            motor_Sample_Owari();                        /* 清理 */
            return 1;                                    /* 提前结束 */
        }
    }
    if (protocol_Debug_SampleMotorTray() == 0) { /* 非调试模式 */
        motor_Tray_Move_By_Index(eTrayIndex_0);  /* 入仓 */
        heat_Motor_Down();                       /* 砸下上加热体 */
    }
    if (protocol_Debug_SampleBarcode() || Miscellaneous_Task_Notify(0) != pdPASS) { /* 通知杂项任务同步运动白板电机 通知失败即亲自运动 */
        white_Motor_PD();                                                           /* 运动白板电机 PD位置 */
        white_Motor_WH();                                                           /* 运动白板电机 白物质位置 */
    }

    if (protocol_Debug_SampleBarcode() == 0) {           /* 非调试模式 */
        if (barcode_result == eBarcodeState_OK) {        /* 二维条码扫描成功 */
            barcode_Motor_Run_By_Index(eBarcodeIndex_0); /* 回归原点 */
        } else {
            /* tray_Move_By_Relative(eMotorDir_REV, 800, 500);  //进仓10毫米 */
            barcode_result = barcode_Scan_Bar();             /* 扫描一维条码 */
            if (barcode_result == eBarcodeState_Interrupt) { /* 中途打断 */
                error_Emit(eError_Sample_Initiative_Break);  /* 主动打断 */
                motor_Sample_Owari();                        /* 清理 */
                return 1;                                    /* 提前结束 */
            }
        }
    }

    return 0;
}

/**
 * @brief  电机任务
 * @param  argument: Not used
 * @retval None
 */
static void motor_Task(void * argument)
{
    BaseType_t xResult = pdFALSE;
    uint32_t xNotifyValue, cnt = 0;
    sMotor_Fun mf;
    uint8_t buffer[64], stage, error;
    TickType_t xTick;
    eComm_Data_Sample_Radiant radiant = eComm_Data_Sample_Radiant_610;

    led_Mode_Set(eLED_Mode_Keep_Green);    /* LED 绿灯常亮 */
    motor_OPT_Status_Init_Wait_Complete(); /* 等待光耦结果完成 */
    motor_Resource_Init();                 /* 电机驱动、位置初始化 */
    barcode_Init();                        /* 扫码枪初始化 */
    tray_Motor_EE_Clear();                 /* 清除托盘丢步标志位 */
    heater_Overshoot_Init(0);              /* 初始化过冲参数 */

    for (;;) {
        xResult = xQueuePeek(motor_Fun_Queue_Handle, &mf, portMAX_DELAY);
        if (xResult != pdPASS) {
            continue;
        }
        cnt = 0;
        fan_IC_Error_Report_Disable();
        switch (mf.fun_type) {
            case eMotor_Fun_In:             /* 入仓 */
                if (heat_Motor_Up() != 0) { /* 抬起上加热体电机 失败 */
                    break;
                };
                motor_Tray_Move_By_Index(eTrayIndex_0); /* 入仓 */
                heat_Motor_Down();                      /* 砸下上加热体 */
                break;
            case eMotor_Fun_Out:                        /* 出仓 */
                motor_Tray_Move_By_Index(eTrayIndex_2); /* 出仓 */
                tray_Motor_EE_Mark();                   /* 标记托盘丢步标志位 */
                break;
            case eMotor_Fun_Debug_Tray_Scan: /* 扫码 */
                if (heat_Motor_Up() != 0) {  /* 抬起上加热体电机 失败 */
                    break;
                };
                tray_Move_By_Index(eTrayIndex_1, 5000); /* 运动托盘电机 */
                barcode_Scan_QR();                      /* 执行扫码 */
                break;
            case eMotor_Fun_Debug_Heater: /* 上加热体电机 */
                if (mf.fun_param_1 == 0) {
                    heat_Motor_Up(); /* 上加热体抬升 */
                } else {
                    heat_Motor_Down(); /* 上加热体砸下 */
                }
                break;
            case eMotor_Fun_Debug_White: /* 白板电机 */
                if (mf.fun_param_1 == 0) {
                    white_Motor_PD(); /* PD位置 */
                } else {
                    white_Motor_WH(); /* 白板位置 */
                }
                break;
            case eMotor_Fun_Debug_Scan:                                                         /* 扫码电机 */
                barcode_Scan_Bantch((uint8_t)(mf.fun_param_1 >> 8), (uint8_t)(mf.fun_param_1)); /* 位置掩码 扫码使能掩码 */
                break;
            case eMotor_Fun_Sample_Start:                         /* 准备测试 */
                xTick = HAL_GetTick();                            /* 记录总体准备起始时间 */
                barcode_Interrupt_Flag_Clear();                   /* 清除打断标志 */
                xTaskNotifyWait(0, 0xFFFFFFFF, &xNotifyValue, 0); /* 清空通知 */
                comm_Data_Conf_Sem_Wait(0);                       /* 清除配置信息信号量 */
                barcode_Result_Init();                            /* 扫码结果初始化 */
                led_Mode_Set(eLED_Mode_Kirakira_Green);           /* LED 绿灯闪烁 */

                motor_Sample_Temperature_Check();      /* 采样前温度检查 */
                if (motor_Sample_Barcode_Scan() > 0) { /* 扫码处理 */
                    motor_Sample_Owari();              /* 清理 */
                    break;                             /* 收到打断信息 提前结束 */
                }
                if (protocol_Debug_SampleBarcode() == 0) {        /* 非调试模式 */
                    if (barcode_Result_Valid_Cnt() == 0) {        /* 有效条码数量为0 */
                        error_Emit(eError_Barcode_Content_Empty); /* 提示没有扫到任何条码 */
                        motor_Sample_Owari();                     /* 清理 */
                        break;                                    /* 没有任何条码 提前结束 */
                    }
                }
                if (comm_Data_Conf_Sem_Wait(pdMS_TO_TICKS(750)) != pdPASS) { /* 等待配置信息 */
                    error_Emit(eError_Comm_Data_Not_Conf);                   /* 提交错误信息 采样配置信息未下达 */
                    motor_Sample_Owari();                                    /* 清理 */
                    break;                                                   /* 提前结束 */
                } else {                                                     /* 配置信息下发完成 */
                    if (gComm_Data_Sample_Max_Point_Get() == 0) {            /* 无效配置信息 */
                        error_Emit(eError_Comm_Data_Invalid_Conf);           /* 提交错误信息 采样配置信息无效 */
                        motor_Sample_Owari();                                /* 清理 */
                        break;                                               /* 提前结束 */
                    }
                }
                if (barcode_Interrupt_Flag_Get()) {             /* 中途打断 */
                    error_Emit(eError_Sample_Initiative_Break); /* 主动打断 */
                    motor_Sample_Owari();                       /* 清理 */
                    break;                                      /* 提前结束 */
                }
                if (protocol_Debug_SampleBarcode() == 0) {                                                       /* 非调试模式 */
                    cnt = HAL_GetTick() - xTick;                                                                 /* 耗时时间 */
                    if (cnt < pdMS_TO_TICKS(15 * 1000)) {                                                        /* 等待补全15秒 */
                        xResult = xTaskNotifyWait(0, 0xFFFFFFFF, &xNotifyValue, pdMS_TO_TICKS(15 * 1000) - cnt); /* 等待任务通知 */
                        if (xResult == pdPASS && xNotifyValue == eMotorNotifyValue_BR) {                         /* 收到中终止命令 */
                            error_Emit(eError_Sample_Initiative_Break);                                          /* 主动打断 */
                            motor_Sample_Owari();                                                                /* 清理 */
                            break;
                        }
                    }
                }
                comm_Data_RecordInit(); /* 初始化数据记录 */
                motor_Sample_Deal(1);   /* 启动采样并控制白板电机 */
                motor_Sample_Owari();   /* 清理 */
                break;
            case eMotor_Fun_AgingLoop: /* 老化测试 */
                cnt = 0;
                do {
                    barcode_Interrupt_Flag_Clear();                   /* 清除打断标志 */
                    xTaskNotifyWait(0, 0xFFFFFFFF, &xNotifyValue, 0); /* 清空通知 */
                    xTick = xTaskGetTickCount();                      /* 记录总体准备起始时间 */
                    comm_Data_Conf_Sem_Wait(0);                       /* 清除配置信息信号量 */
                    led_Mode_Set(eLED_Mode_Kirakira_Green);           /* LED 绿灯闪烁 */

                    motor_Sample_Temperature_Check();      /* 采样前温度检查 */
                    if (motor_Sample_Barcode_Scan() > 0) { /* 扫码处理 */
                        break;                             /* 收到打断信息 提前结束 */
                    }
                    if (comm_Data_Conf_Sem_Wait(pdMS_TO_TICKS(400)) != pdPASS) { /* 等待配置信息 */
                        if (cnt == 0) {                                          /* 首次配置信息 */
                            error_Emit(eError_Comm_Data_Not_Conf);               /* 提交错误信息 采样配置信息未下达 */
                            motor_Sample_Owari();                                /* 清理 */
                            break;                                               /* 提前结束 */
                        } else {
                            comm_Data_Sample_Send_Conf_Re(); /* 使用上次配置信息 */
                        }
                    } else {                                           /* 配置信息下发完成 */
                        if (gComm_Data_Sample_Max_Point_Get() == 0) {  /* 无效配置信息 */
                            error_Emit(eError_Comm_Data_Invalid_Conf); /* 提交错误信息 采样配置信息无效 */
                            motor_Sample_Owari();                      /* 清理 */
                            break;                                     /* 提前结束 */
                        }
                    }
                    if (barcode_Interrupt_Flag_Get()) {             /* 中途打断 */
                        motor_Sample_Owari();                       /* 清理 */
                        error_Emit(eError_Sample_Initiative_Break); /* 主动打断 */
                        break;                                      /* 提前结束 */
                    }
                    if (protocol_Debug_SampleBarcode() == 0) {             /* 非调试模式 */
                        vTaskDelayUntil(&xTick, pdMS_TO_TICKS(15 * 1000)); /* 等待补全15秒 */
                    }
                    comm_Data_RecordInit(); /* 初始化数据记录 */
                    motor_Sample_Deal(0);   /* 启动采样并控制白板电机 */
                    motor_Sample_Owari();   /* 清理 */
                    xResult = xTaskNotifyWait(0, 0xFFFFFFFF, &xNotifyValue, pdMS_TO_TICKS(gMotor_Aging_Sleep_Get() * 1000)); /* 等待任务通知 */
                    if (xResult == pdPASS && xNotifyValue == eMotorNotifyValue_BR) {                                         /* 收到中终止命令 */
                        error_Emit(eError_Sample_Initiative_Break);                                                          /* 主动打断 */
                        break;
                    }
                    ++cnt;
                } while (protocol_Debug_AgingLoop());
                protocol_Debug_Clear(eProtocol_Debug_AgingLoop); /* 清除老化测试标志位 */
                break;
            case eMotor_Fun_SYK:            /* 交错 */
                if (heat_Motor_Up() != 0) { /* 抬起上加热体电机 失败 */
                    break;
                };
                tray_Move_By_Index(eTrayIndex_2, 5000); /* 运动托盘电机 */
                heat_Motor_Down();                      /* 砸下上加热体 */
                white_Motor_WH();                       /* 运动白板电机 */
                barcode_Scan_By_Index(eBarcodeIndex_0);
                break;
            case eMotor_Fun_RLB: /* 回滚 */
                motor_Resource_Init();
                break;
            case eMotor_Fun_PRE_TRAY:       /* 压力测试 托盘 */
                gComm_Mian_Block_Disable(); /* 主串口非阻塞 */
                do {
                    buffer[0] = 0;
                    buffer[5] = 0;
                    switch (cnt % 3) {
                        case 0:
                            motor_Tray_Move_By_Index(eTrayIndex_0);
                            break;
                        case 1:
                            motor_Tray_Move_By_Index(eTrayIndex_1);
                            break;
                        case 2:
                            motor_Tray_Move_By_Index(eTrayIndex_2);
                            break;
                    }
                    ++cnt;
                    memcpy(buffer + 1, (uint8_t *)(&cnt), 4);
                    comm_Out_SendTask_QueueEmitWithBuildCover(0xD0, buffer, 6);
                } while (gMotorPressureStopBits_Get(mf.fun_type) == 0);
                break;
            case eMotor_Fun_PRE_BARCODE:    /* 压力测试 扫码 */
                gComm_Mian_Block_Disable(); /* 主串口非阻塞 */
                motor_Tray_Move_By_Index(eTrayIndex_1);
                do {
                    buffer[0] = 1;
                    switch (cnt % 7) {
                        case 0:
                            buffer[5] = barcode_Scan_By_Index(eBarcodeIndex_0);
                            break;
                        case 1:
                            buffer[5] = barcode_Scan_By_Index(eBarcodeIndex_1);
                            break;
                        case 2:
                            buffer[5] = barcode_Scan_By_Index(eBarcodeIndex_2);
                            break;
                        case 3:
                            buffer[5] = barcode_Scan_By_Index(eBarcodeIndex_3);
                            break;
                        case 4:
                            buffer[5] = barcode_Scan_By_Index(eBarcodeIndex_4);
                            break;
                        case 5:
                            buffer[5] = barcode_Scan_By_Index(eBarcodeIndex_5);
                            break;
                        case 6:
                            buffer[5] = barcode_Scan_By_Index(eBarcodeIndex_6);
                            break;
                    }
                    ++cnt;
                    memcpy(buffer + 1, (uint8_t *)(&cnt), 4);
                    comm_Out_SendTask_QueueEmitWithBuildCover(0xD0, buffer, 6);
                } while (gMotorPressureStopBits_Get(mf.fun_type) == 0);
                break;
            case eMotor_Fun_PRE_HEATER:     /* 压力测试 上加热体 */
                gComm_Mian_Block_Disable(); /* 主串口非阻塞 */
                do {
                    buffer[0] = 2;
                    if (cnt % 2 == 0) {
                        buffer[5] = heat_Motor_Down(); /* 砸下上加热体 */
                    } else {
                        buffer[5] = heat_Motor_Up(); /* 抬起加热体电机 */
                    }
                    ++cnt;
                    memcpy(buffer + 1, (uint8_t *)(&cnt), 4);
                    comm_Out_SendTask_QueueEmitWithBuildCover(0xD0, buffer, 6);
                } while (gMotorPressureStopBits_Get(mf.fun_type) == 0);
                break;
            case eMotor_Fun_PRE_WHITE:      /* 压力测试 白板 */
                gComm_Mian_Block_Disable(); /* 主串口非阻塞 */
                do {
                    ++cnt;
                    buffer[0] = 3;
                    buffer[5] = white_Motor_Toggle(); /* 切换白板电机位置 */
                    memcpy(buffer + 1, (uint8_t *)(&cnt), 4);
                    comm_Out_SendTask_QueueEmitWithBuildCover(0xD0, buffer, 6);
                    vTaskDelay(1000);
                } while (gMotorPressureStopBits_Get(mf.fun_type) == 0);
                break;
            case eMotor_Fun_PRE_ALL:        /* 压力测试 */
                gComm_Mian_Block_Disable(); /* 主串口非阻塞 */
                do {
                    buffer[0] = 4;
                    buffer[5] = 0;
                    switch (cnt % 2) {
                        case 0:
                            motor_Tray_Move_By_Index(eTrayIndex_0);
                            barcode_Scan_By_Index(eBarcodeIndex_0);
                            heat_Motor_Down(); /* 砸下上加热体 */
                            break;
                        case 1:
                            motor_Tray_Move_By_Index(eTrayIndex_2);
                            barcode_Scan_By_Index(eBarcodeIndex_6);
                            break;
                    }
                    white_Motor_Toggle(); /* 切换白板电机位置 */
                    ++cnt;
                    memcpy(buffer + 1, (uint8_t *)(&cnt), 4);
                    comm_Out_SendTask_QueueEmitWithBuildCover(0xD0, buffer, 6);
                } while (gMotorPressureStopBits_Get(mf.fun_type) == 0);
                break;
            case eMotor_Fun_Self_Check: /* 自检测试 */
                fan_Enter_Self_Test();
                motor_Self_Check_Motor_Tray(buffer);
                motor_Self_Check_Motor_White(buffer);
                motor_Self_Check_Motor_Heater(buffer);
                motor_Self_Check_Motor_Scan(buffer);
                motor_Tray_Move_By_Index(eTrayIndex_0); /* 入仓 */
                motor_Self_Check_Scan(buffer);
                if (TRAY_MOTOR_IS_OPT_1) {
                    heat_Motor_Down();
                }
                motor_Self_Check_PD(buffer, 0x07);
                motor_Self_Check_FAN(buffer);
                fan_Leave_Self_Test();
                motor_Tray_Move_By_Index(eTrayIndex_2); /* 出仓 */
                break;
            case eMotor_Fun_Self_Check_FA: /* 自检测试  生产板厂 */
                motor_Self_Check_Motor_Tray(buffer);
                motor_Self_Check_Motor_White(buffer);
                motor_Self_Check_Motor_Heater(buffer);
                motor_Self_Check_Motor_Scan(buffer);
                motor_Self_Check_Scan(buffer);
                motor_Tray_Move_By_Index(eTrayIndex_2); /* 出仓 */
                break;
            case eMotor_Fun_Self_Check_Motor_White: /* 自检测试 单项 白板电机 */
                motor_Self_Check_Motor_White(buffer);
                break;
            case eMotor_Fun_Self_Check_Motor_Heater: /* 自检测试 单项 上加热体电机 */
                motor_Self_Check_Motor_Heater(buffer);
                break;
            case eMotor_Fun_Self_Check_Motor_Tray: /* 自检测试 单项 托盘电机 */
                motor_Self_Check_Motor_Tray(buffer);
                break;
            case eMotor_Fun_Self_Check_Motor_Scan: /* 自检测试 单项 扫码电机*/
                motor_Self_Check_Motor_Scan(buffer);
                break;
            case eMotor_Fun_Self_Check_Scan: /* 自检测试 单项 扫码头 */
                motor_Self_Check_Scan(buffer);
                break;
            case eMotor_Fun_Self_Check_PD: /* 自检测试 单项 PD */
                motor_Self_Check_PD(buffer, 0x07 & mf.fun_param_1);
                break;
            case eMotor_Fun_Self_Check_FAN: /* 自检测试 风扇 */
                fan_Enter_Self_Test();
                vTaskDelay(3000);
                motor_Self_Check_FAN(buffer);
                fan_Leave_Self_Test();
                break;
            case eMotor_Fun_Stary_Test: /* 杂散光测试 */
                motor_Stary_Test();     /* 杂散光测试 */
                break;
            case eMotor_Fun_Correct:                        /* 定标 */
                if (protocol_Debug_SampleBarcode() == 0) {  /* 非调试模式 */
                    motor_Tray_Move_By_Index(eTrayIndex_1); /* 扫码位置 */
                }
                led_Mode_Set(eLED_Mode_Kirakira_Red);                                                               /* LED 红灯闪烁 */
                if (barcode_Scan_QR() != eBarcodeState_OK || barcode_Scan_Decode_Correct_Info_From_Result() != 0) { /* 扫码失败或者解析失败 */
                    error_Emit(eError_Correct_Info_Lost);                                                           /* 定标信息不足 */
                    motor_Sample_Owari_Correct();                                                                   /* 清理 */
                    motor_Tray_Move_By_Index(eTrayIndex_2);                                                         /* 出仓 */
                    gComm_Data_Correct_Flag_Clr();                                                                  /* 退出定标状态 */
                    break;
                } else {
                    stage = barcode_Scan_Get_Correct_Stage(); /* 抽取通道校正段索引 */
                    for (cnt = 1; cnt <= 6; ++cnt) {
                        comm_Data_Set_Corretc_Stage(cnt, stage); /* 定标段索引配置 */
                    }
                }

                if (protocol_Debug_SampleMotorTray() == 0) { /* 非调试模式 */
                    motor_Tray_Move_By_Index(eTrayIndex_0);  /* 入仓 */
                    heat_Motor_Down();                       /* 砸下上加热体 */
                }
                white_Motor_PD(); /* 运动白板电机 PD位置 清零位置 */

                for (radiant = eComm_Data_Sample_Radiant_610; radiant <= eComm_Data_Sample_Radiant_405; ++radiant) {
                    comm_Data_Sample_Send_Conf_Correct(buffer, radiant, MOTOR_CORRECT_POINT_NUM, eComm_Data_Outbound_CMD_TEST); /* 配置波长 点数 */
                    white_Motor_WH();           /* 运动白板电机 白物质位置 */
                    gStorgeIllumineCnt_Clr();   /* 清除标记 */
                    if (motor_Sample_Deal(0)) { /* 启动采样并控制白板电机 */
                        break;                  /* 定标异常 */
                    }
                    motor_Wait_Stroge_Correct(3000);                                   /* 等待设置存储完成 */
                    storgeTaskNotification(eStorgeNotifyConf_Dump_Params, eComm_Out);  /* 通知存储任务 保存参数 */
                    gStorgeTaskInfoLockWait(3000);                                     /* 等待参数保存完毕 */
                    storgeTaskNotification(eStorgeNotifyConf_Dump_Correct, eComm_Out); /* 保存原始数据 */
                    gStorgeTaskInfoLockWait(3000);                                     /* 等待参数保存完毕 */
                    buffer[0] = radiant;                                               /* 波长 */
                    buffer[1] = 0;                                                     /* 正常 */
                    buffer[2] = stage;                                                 /* 校正段索引 */
                    if (radiant == eComm_Data_Sample_Radiant_610 || radiant == eComm_Data_Sample_Radiant_550) {
                        cnt = 6;
                    } else {
                        cnt = 1;
                    }
                    for (unsigned char i = 0; i < cnt; ++i) {
                        storge_ParamReadSingle(storge_Param_Illumine_CC_Get_Index(i + 1, radiant) + stage, (&buffer[3 + 2 * i]));
                    }
                    comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Correct, buffer, 3 + 2 * cnt);
                }

                motor_Sample_Owari_Correct();           /* 清理 */
                comm_Data_Sample_Owari();               /* 上送采样结束报文 */
                motor_Tray_Move_By_Index(eTrayIndex_2); /* 出仓 */
                gComm_Data_Correct_Flag_Clr();          /* 退出定标状态 */
                break;
            case eMotor_Fun_Lamp_BP:
                xTaskNotifyWait(0, 0xFFFFFFFF, &xNotifyValue, 0); /* 清空通知 */
                gComm_Data_Lamp_BP_Flag_Mark();                   /* 标记灯BP状态 */
                comm_Data_Sample_Send_Clear_Conf();               /* 清除配置 */
                if (protocol_Debug_SampleMotorTray() == 0) {      /* 非调试模式 */
                    motor_Tray_Move_By_Index(eTrayIndex_0);       /* 入仓 */
                    heat_Motor_Down();                            /* 砸下上加热体 */
                }
                for (radiant = eComm_Data_Sample_Radiant_610; radiant <= eComm_Data_Sample_Radiant_405; ++radiant) {
                    comm_Data_Sample_Send_Conf_Correct(buffer, radiant, 6, eComm_Data_Outbound_CMD_TEST); /* 配置波长 点数 */
                    vTaskDelay(500);
                    for (cnt = 0; cnt < 6; ++cnt) {
                        white_Motor_WH();                                                   /* 运动白板电机 白物质位置 */
                        comm_Data_ISR_Tran(0);                                              /* 采集白板 */
                        xTaskNotifyWait(0, 0xFFFFFFFF, &xNotifyValue, pdMS_TO_TICKS(2000)); /* 等待通知 */
                        white_Motor_PD();                                                   /* 运动白板电机 PD位置 */
                        comm_Data_ISR_Tran(1);                                              /* 采集PD */
                        xTaskNotifyWait(0, 0xFFFFFFFF, &xNotifyValue, pdMS_TO_TICKS(2000)); /* 等待通知 */
                    }
                    white_Motor_WH(); /* 运动白板电机 白物质位置 */
                    vTaskDelay(1000);
                    comm_Data_Sample_Send_Clear_Conf();
                    vTaskDelay(1000);
                }
                comm_Data_Sample_Owari();               /* 发送完成采样报文 */
                motor_Tray_Move_By_Index(eTrayIndex_2); /* 出仓 */
                gComm_Data_Lamp_BP_Flag_Clr();          /* 清除灯BP状态 */
                break;
            case eMotor_Fun_SP_LED:
                error = 0;
                xTaskNotifyWait(0, 0xFFFFFFFF, &xNotifyValue, 0);                                                    /* 清空通知 */
                comm_Data_Get_LED_Voltage();                                                                         /* 获取采样板LED电压配置 */
                storgeTaskNotification(eStorgeNotifyConf_Load_Sample_LED, eComm_Data);                               /* 通知存储任务 加载记录 */
                led_Mode_Set(eLED_Mode_Red_Green);                                                                   /* 红绿交替 */
                for (radiant = eComm_Data_Sample_Radiant_610; radiant <= eComm_Data_Sample_Radiant_405; ++radiant) { /* 逐个波长校正 */
                    switch (radiant) {
                        case eComm_Data_Sample_Radiant_610:
                            gComm_Data_LED_Voltage_Interval_Set(COMM_DATA_LED_VOLTAGE_UNIT_610); /* 调整间隔初始化 */
                            cnt = COMM_DATA_LED_VOLTAGE_INIT_610;                                /* 初始化电压值 */
                            break;
                        case eComm_Data_Sample_Radiant_550:
                            gComm_Data_LED_Voltage_Interval_Set(COMM_DATA_LED_VOLTAGE_UNIT_550); /* 调整间隔初始化 */
                            cnt = COMM_DATA_LED_VOLTAGE_INIT_550;                                /* 初始化电压值 */
                            break;
                        case eComm_Data_Sample_Radiant_405:
                            gComm_Data_LED_Voltage_Interval_Set(COMM_DATA_LED_VOLTAGE_UNIT_405); /* 调整间隔初始化 */
                            cnt = COMM_DATA_LED_VOLTAGE_INIT_405;                                /* 初始化电压值 */
                            break;
                    }
                    comm_Data_Set_LED_Voltage(radiant, cnt);                                    /* 设置初始化电压值 */
                    for (uint8_t i = 0; i < 20; ++i) {                                          /* 循环测试-检测-调整电压 */
                        comm_Data_RecordInit();                                                 /* 初始化数据记录 */
                        gComm_Data_SP_LED_Flag_Mark(radiant);                                   /* 标记校正采样板LED电压状态 */
                        comm_Data_Sample_Send_Conf_Correct(buffer, radiant,                     /* 配置波长 */
                                                           gComm_Data_LED_Voltage_Points_Get(), /* 点数 */
                                                           eComm_Data_Outbound_CMD_TEST);       /* 上送 PD 值 */
                        vTaskDelay(300);                                                        /* 等待回应报文 */
                        white_Motor_WH();                                                       /* 运动白板电机 白板位置 */
                        if (motor_Sample_Deal(0)) {                                             /* 启动采样并控制白板电机 */
                            break;                                                              /* 定标异常 */
                        }
                        white_Motor_WH();                                                                        /* 运动白板电机 白板位置 */
                        comm_Data_Wait_Data((radiant != eComm_Data_Sample_Radiant_405) ? (0x3F) : (0x01), 1200); /* 等待采样结果上送 */
                        stage = comm_Data_Check_LED(radiant, cnt, i);                                            /* 检查采样值 */
                        cnt += gComm_Data_LED_Voltage_Interval_Get();                                            /* 回退电压值 */
                        if (cnt > 1200) {                                                                        /* 电压越限 */
                            error_Emit(eError_LED_Correct_Out_Of_Range_610 + radiant - eComm_Data_Sample_Radiant_610);
                            error |= (1 << (radiant - 1));
                            break;
                        }
                        comm_Data_Set_LED_Voltage(radiant, cnt);                                  /* 调整电压值 */
                        if (stage == 0) {                                                         /* 合格即跳出 */
                            storgeTaskNotification(eStorgeNotifyConf_Dump_Sample_LED, eComm_Out); /* 通知存储任务 保存记录 */
                            break;
                        }
                        if (i == 20 - 1) {
                            error_Emit(eError_LED_Correct_Max_Retry_610 + radiant - eComm_Data_Sample_Radiant_610);
                            error |= (8 << (radiant - 1));
                        }
                        comm_Data_Get_LED_Voltage(); /* 获取采样板LED电压配置 */
                    }
                }
                storgeTaskNotification(eStorgeNotifyConf_Load_Sample_LED, eComm_Out); /* 通知存储任务 加载记录 */
                if (error == 0) {
                    error_Emit(eError_SP_LED_Success);
                }
                motor_Tray_Move_By_Index(eTrayIndex_2); /* 出仓 */
                gComm_Data_SP_LED_Flag_Clr();           /* 清除校正采样板LED电压状态 */
                led_Mode_Set(eLED_Mode_Keep_Green);     /* LED 绿灯常亮 */
                break;
            default:
                break;
        }
        fan_IC_Error_Report_Enable();
        gComm_Mian_Block_Enable();
        xTaskNotifyWait(0, 0xFFFFFFFF, &xNotifyValue, 0); /* 清除任务通知 */
        xQueueReceive(motor_Fun_Queue_Handle, &mf, 0);
    }
}

/**
 * @brief  自检测试 单项 白板电机
 * @param  pBuffer   数据指针
 * @retval None
 */
static void motor_Self_Check_Motor_White(uint8_t * pBuffer)
{
    white_Motor_WH();                                                                       /* 推出光耦准备测试 */
    pBuffer[0] = eMotor_Fun_Self_Check_Motor_White - eMotor_Fun_Self_Check_Motor_White + 6; /* 自检测试 单项 白板电机 */
    pBuffer[2] = white_Motor_PD();                                                          /* PD 位置测试 */
    pBuffer[3] = white_Motor_WH();                                                          /* 白物质测试 */
    if (pBuffer[2] == 0 && pBuffer[3] == 0) {
        pBuffer[1] = 0; /* 通过结论 */
    } else {
        pBuffer[1] = 1; /* 故障结论 */
    }
    if (comm_Main_SendTask_Queue_GetFree() > 0) {
        comm_Main_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 4);
        comm_Out_SendTask_QueueEmitWithModify(pBuffer, 4 + 7, 0); /* 转发至外串口但不允许阻塞 */
    } else {
        comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 4);
    }
}

/**
 * @brief  自检测试 单项 上加热体电机
 * @param  pBuffer   数据指针
 * @retval None
 */
static void motor_Self_Check_Motor_Heater(uint8_t * pBuffer)
{
    heat_Motor_Up();                                                                         /* 抬起上加热体准备测试 */
    pBuffer[0] = eMotor_Fun_Self_Check_Motor_Heater - eMotor_Fun_Self_Check_Motor_White + 6; /* 自检测试 单项 上加热体电机 */
    if (motor_OPT_Status_Get(eMotor_OPT_Index_Heater) == eMotor_OPT_Status_OFF) {
        pBuffer[3] = heat_Motor_Down(); /* 砸下上加热体 */
        pBuffer[2] = heat_Motor_Up();   /* 抬起上加热体*/
    } else {
        pBuffer[2] = heat_Motor_Up();   /* 抬起上加热体*/
        pBuffer[3] = heat_Motor_Down(); /* 砸下上加热体 */
    }

    if (pBuffer[2] == 0 && pBuffer[3] == 0) {
        pBuffer[1] = 0; /* 通过结论 */
    } else {
        pBuffer[1] = 1; /* 故障结论 */
    }
    if (comm_Main_SendTask_Queue_GetFree() > 0) {
        comm_Main_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 4);
        comm_Out_SendTask_QueueEmitWithModify(pBuffer, 4 + 7, 0); /* 转发至外串口但不允许阻塞 */
    } else {
        comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 4);
    }
    heat_Motor_Up(); /* 保持上加热体抬起 */
}

/**
 * @brief  自检测试 单项 托盘电机
 * @param  pBuffer   数据指针
 * @retval None
 */
static void motor_Self_Check_Motor_Tray(uint8_t * pBuffer)
{
    heat_Motor_Up();                                                                       /* 抬起上加热体 */
    tray_Motor_Init();                                                                     /* 托盘电机初始化 */
    vTaskDelay(100);                                                                       /* 延时 */
    tray_Motor_Reset_Pos();                                                                /* 初始化位置 */
    vTaskDelay(100);                                                                       /* 延时 */
    pBuffer[0] = eMotor_Fun_Self_Check_Motor_Tray - eMotor_Fun_Self_Check_Motor_White + 6; /* 自检测试 单项 托盘电机 */
    pBuffer[2] = tray_Move_By_Index(eTrayIndex_2, 3000);                                   /* 出仓 */
    pBuffer[3] = tray_Move_By_Index(eTrayIndex_0, 3000);                                   /* 入仓 */
    if (pBuffer[2] == 0 && pBuffer[3] == 0) {
        pBuffer[1] = 0; /* 通过结论 */
    } else {
        pBuffer[1] = 1; /* 故障结论 */
    }
    if (comm_Main_SendTask_Queue_GetFree() > 0) {
        comm_Main_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 4);
        comm_Out_SendTask_QueueEmitWithModify(pBuffer, 4 + 7, 0); /* 转发至外串口但不允许阻塞 */
    } else {
        comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 4);
    }
}

/**
 * @brief  自检测试 单项 扫码电机
 * @param  pBuffer   数据指针
 * @retval None
 */
static void motor_Self_Check_Motor_Scan(uint8_t * pBuffer)
{
    barcode_Motor_Init();                                                                  /* 扫码电机初始化 */
    barcode_Motor_Reset_Pos();                                                             /* 初始化位置 */
    pBuffer[0] = eMotor_Fun_Self_Check_Motor_Scan - eMotor_Fun_Self_Check_Motor_White + 6; /* 自检测试 单项 扫码电机 */
    pBuffer[2] = barcode_Motor_Run_By_Index(eBarcodeIndex_6);                              /* QR码位置 */
    pBuffer[3] = barcode_Motor_Run_By_Index(eBarcodeIndex_0);                              /* 初始位置 */
    if (pBuffer[2] == 0 && pBuffer[3] == 0) {
        pBuffer[1] = 0; /* 通过结论 */
    } else {
        pBuffer[1] = 1; /* 故障结论 */
    }
    if (comm_Main_SendTask_Queue_GetFree() > 0) {
        comm_Main_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 4);
        comm_Out_SendTask_QueueEmitWithModify(pBuffer, 4 + 7, 0); /* 转发至外串口但不允许阻塞 */
    } else {
        comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 4);
    }
}

/**
 * @brief  自检测试 单项 扫码头
 * @param  pBuffer   数据指针
 * @retval None
 */
static void motor_Self_Check_Scan(uint8_t * pBuffer)
{
    pBuffer[0] = eMotor_Fun_Self_Check_Scan - eMotor_Fun_Self_Check_Motor_White + 6; /* 自检测试 单项 扫码头 */
    barcode_Motor_Run_By_Index(eBarcodeIndex_0);                                     /* 移动到初始位置 */
    pBuffer[1] = barcode_Read_From_Serial(pBuffer + 2, pBuffer + 3, 10, 1000);       /* 读取扫码结果 长度为10 */
    if (comm_Main_SendTask_Queue_GetFree() > 0) {
        comm_Main_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, pBuffer[2] + 3);
        comm_Out_SendTask_QueueEmitWithModify(pBuffer, pBuffer[2] + 3 + 7, 0); /* 转发至外串口但不允许阻塞 */
    } else {
        comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, pBuffer[2] + 3);
    }
}

/**
 * @brief  自检测试 单项 PD
 * @param  pBuffer   数据指针
 * @param  mask 掩码 0～7
 * @retval None
 */
static void motor_Self_Check_PD(uint8_t * pBuffer, uint8_t mask)
{
    eComm_Data_Sample_Radiant radiant;
    uint32_t record[13] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    for (radiant = eComm_Data_Sample_Radiant_610; radiant <= eComm_Data_Sample_Radiant_405; ++radiant) { /* 逐个波长校正 */
        if (((1 << (radiant - 1)) & mask) == 0) {                                                        /* 非测试项 */
            continue;
        }
        gComm_Data_SelfCheck_PD_Flag_Mark(radiant);                                              /* 标记自检测试 单项 PD状态 */
        comm_Data_RecordInit();                                                                  /* 初始化数据记录 */
        comm_Data_Sample_Send_Conf_Correct(pBuffer, radiant,                                     /* 配置波长 */
                                           1,                                                    /* 点数 */
                                           eComm_Data_Outbound_CMD_TEST);                        /* 上送 PD 值 */
        vTaskDelay(300);                                                                         /* 等待回应报文 */
        white_Motor_WH();                                                                        /* 运动白板电机 白板位置 */
        motor_Sample_Deal(0);                                                                    /* 启动采样并控制白板电机 */
        white_Motor_WH();                                                                        /* 运动白板电机 白板位置 */
        comm_Data_Wait_Data((radiant != eComm_Data_Sample_Radiant_405) ? (0x3F) : (0x01), 1200); /* 等待采样结果上送 */
        comm_Data_Copy_Data_U32((radiant != eComm_Data_Sample_Radiant_405) ? (0x3F) : (0x01),
                                &record[(radiant - eComm_Data_Sample_Radiant_610) * 6]); /* 复制测试数据 */
    }

    gComm_Data_SelfCheck_PD_Flag_Clr();                                            /* 清除自检测试 单项 PD状态 */
    pBuffer[0] = eMotor_Fun_Self_Check_PD - eMotor_Fun_Self_Check_Motor_White + 6; /* 自检测试 单项 PD */
    pBuffer[1] = mask;                                                             /* 掩码值 */
    memcpy(&pBuffer[2], (uint8_t *)(record), ARRAY_LEN(record) * 4);
    if (comm_Main_SendTask_Queue_GetFree() > 0) {
        comm_Main_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 54); /* 上报主串口 */
        comm_Out_SendTask_QueueEmitWithModify(pBuffer, 54 + 7, 0);                                              /* 转发至外串口但不允许阻塞 */
    } else {
        comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 54); /* 上报主串口 */
    }
}

/**
 * @brief  自检测试 单项 风扇
 * @param  pBuffer   数据指针
 * @retval None
 */
static void motor_Self_Check_FAN(uint8_t * pBuffer)
{
    uint32_t freq;

    freq = fan_IC_Freq_Get();
    pBuffer[0] = eMotor_Fun_Self_Check_FAN - eMotor_Fun_Self_Check_Motor_White + 6; /* 自检测试 单项 风扇 */
    if (freq < 3600) {
        pBuffer[1] = 1;
    } else if (freq > 5000) {
        pBuffer[1] = 2;
    } else {
        pBuffer[1] = 0;
    }
    memcpy(pBuffer + 2, (uint8_t *)(&freq), 4);

    if (comm_Main_SendTask_Queue_GetFree() > 0) {
        comm_Main_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 6);
        comm_Out_SendTask_QueueEmitWithModify(pBuffer, 6 + 7, 0); /* 转发至外串口但不允许阻塞 */
    } else {
        comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 6);
    }
}

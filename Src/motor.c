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

/* Extern variables ----------------------------------------------------------*/
extern TIM_HandleTypeDef htim6;
extern TIM_HandleTypeDef htim7;

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
xQueueHandle motor_Fun_Queue_Handle = NULL; /* 电机功能队列 */
xTaskHandle motor_Task_Handle = NULL;       /* 电机任务句柄 */

/* Private constants ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
static void motor_Task(void * argument);
static void motor_Tray_Move_By_Index(eTrayIndex index);

/* Private user code ---------------------------------------------------------*/

typedef struct {
    uint8_t cnt_on;
    uint8_t threshold_on;
    uint8_t cnt_off;
    uint8_t threshold_off;
    eMotor_OPT_Status status_result;
    eMotor_OPT_Status (*opt_status_get)(void);
} sMotor_OPT_Record;

static sMotor_OPT_Record gMotor_OPT_Records[4];
static eMotor_OPT_Status motor_OPT_Status_Get_Scan(void);
static eMotor_OPT_Status motor_OPT_Status_Get_Tray(void);
static eMotor_OPT_Status motor_OPT_Status_Get_Heater(void);
static eMotor_OPT_Status motor_OPT_Status_Get_White(void);

const eMotor_OPT_Status (*gOPT_Status_Get_Funs[])(void) = {motor_OPT_Status_Get_Scan, motor_OPT_Status_Get_Tray, motor_OPT_Status_Get_Heater,
                                                           motor_OPT_Status_Get_White};

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
static eMotor_OPT_Status motor_OPT_Status_Get_Tray(void)
{
    if (HAL_GPIO_ReadPin(OPTSW_OUT1_GPIO_Port, OPTSW_OUT1_Pin) == GPIO_PIN_RESET) {
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
static eMotor_OPT_Status motor_OPT_Status_Get_White(void)
{
    if (HAL_GPIO_ReadPin(OPTSW_OUT4_GPIO_Port, OPTSW_OUT4_Pin) == GPIO_PIN_RESET) {
        return eMotor_OPT_Status_OFF;
    }
    return eMotor_OPT_Status_ON;
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
        gMotor_OPT_Records[i].threshold_on = 3;                         /* 断开次数阈值 */
        gMotor_OPT_Records[i].threshold_off = 3;                        /* 遮挡次数阈值 */
        gMotor_OPT_Records[i].status_result = eMotor_OPT_Status_None;   /* 初始状态 */
        gMotor_OPT_Records[i].opt_status_get = gOPT_Status_Get_Funs[i]; /* 硬件层光耦状态读取 */
    }
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
    switch (idx) {
        case eMotor_OPT_Index_Scan:
            return gMotor_OPT_Records[0].status_result;
        case eMotor_OPT_Index_Tray:
            return gMotor_OPT_Records[1].status_result;
        case eMotor_OPT_Index_Heater:
            return gMotor_OPT_Records[2].status_result;
        case eMotor_OPT_Index_White:
            return gMotor_OPT_Records[3].status_result;
    }
    return eMotor_OPT_Status_OFF;
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
        error_Emit(eError_Peripheral_Motor_Scan, eError_Motor_Status_Warui);
    }
    if (result & 0x10) {
        error_Emit(eError_Peripheral_Motor_Tray, eError_Motor_Status_Warui);
    }

    /* 警告 上加热体电机不抬起 不允许操作托盘电机 */
    m_drv8824_Init();                            /* 上加热体电机初始化 */
    tray_Motor_Init();                           /* 托盘电机初始化 */
    barcode_Motor_Init();                        /* 扫码电机初始化 */
    barcode_Motor_Reset_Pos();                   /* 重置扫码电机位置 */
    tray_Motor_Reset_Pos();                      /* 重置托盘电机位置 */
    heat_Motor_Down();                           /* 砸下上加热体电机 */
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
    if (xTaskCreate(motor_Task, "Motor Task", 256, NULL, TASK_PRIORITY_MOTOR, &motor_Task_Handle) != pdPASS) {
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

    return xTaskNotifyFromISR(motor_Task_Handle, info, eSetBits, &xWoken);
}

/**
 * @brief  电机任务 测量定时器通信
 * @param  info 通信信息
 * @retval pdTRUE 提交成功 pdFALSE 提交失败
 */
BaseType_t motor_Sample_Info(eMotorNotifyValue info)
{
    return xTaskNotify(motor_Task_Handle, info, eSetBits);
}

/**
 * @brief  电机任务 提交
 * @param  pFun_type 任务详情指针
 * @param  timeout  最长等待时间
 * @retval 0 提交成功 1 提交失败
 */
uint8_t motor_Emit(sMotor_Fun * pFun_type, uint32_t timeout)
{
    if (xQueueSendToBack(motor_Fun_Queue_Handle, pFun_type, timeout) != pdTRUE) {
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
    uint8_t buffer[8];

    if (heat_Motor_Position_Is_Up() == 0 && heat_Motor_Up() != 0) { /* 上加热体光耦位置未被阻挡 则抬起上加热体电机 */
        buffer[0] = 0x00;                                           /* 抬起上加热体失败 */
        comm_Main_SendTask_QueueEmitWithBuildCover(eProtocoleRespPack_Client_DISH, buffer, 1); /* 上报失败报文 */
        comm_Out_SendTask_QueueEmitWithModify(buffer, 8, 0);                                   /* 转发至外串口但不允许阻塞 */
        beep_Start_With_Conf(eBeep_Freq_do, 300, 0, 1);
        return;
    }
    /* 托盘保持力矩不足 托盘容易位置会发生变化 实际位置与驱动记录位置不匹配 每次移动托盘电机必须重置 */
    tray_Motor_Init();                                                                         /* 托盘电机初始化 */
    if (tray_Motor_Reset_Pos() != 0) {                                                         /* 重置托盘电机位置 */
        buffer[0] = 0x00;                                                                      /* 托盘电机运动失败 */
        comm_Main_SendTask_QueueEmitWithBuildCover(eProtocoleRespPack_Client_DISH, buffer, 1); /* 上报失败报文 */
        comm_Out_SendTask_QueueEmitWithModify(buffer, 8, 0);                                   /* 转发至外串口但不允许阻塞 */
        beep_Start_With_Conf(eBeep_Freq_fa, 300, 0, 1);
        return;
    }
    if (tray_Move_By_Index(index, 5000) == eTrayState_OK) { /* 运动托盘电机 */
        if (index == eTrayIndex_0) {                        /* 托盘在检测位置 */
            buffer[0] = 0x01;
            comm_Main_SendTask_QueueEmitWithBuildCover(eProtocoleRespPack_Client_DISH, buffer, 1);
            comm_Out_SendTask_QueueEmitWithModify(buffer, 8, 0); /* 转发至外串口但不允许阻塞 */
            beep_Start_With_Conf(eBeep_Freq_re, 300, 0, 1);
        } else if (index == eTrayIndex_2) { /* 托盘在加样位置 */
            buffer[0] = 0x02;
            comm_Main_SendTask_QueueEmitWithBuildCover(eProtocoleRespPack_Client_DISH, buffer, 1);
            comm_Out_SendTask_QueueEmitWithModify(buffer, 8, 0); /* 转发至外串口但不允许阻塞 */
            beep_Start_With_Conf(eBeep_Freq_mi, 300, 0, 1);
        }
        return;
    } else {
        buffer[0] = 0x00;                                                                      /* 托盘电机运动失败 */
        comm_Main_SendTask_QueueEmitWithBuildCover(eProtocoleRespPack_Client_DISH, buffer, 1); /* 上报失败报文 */
        comm_Out_SendTask_QueueEmitWithModify(buffer, 8, 0);                                   /* 转发至外串口但不允许阻塞 */
        beep_Start_With_Conf(eBeep_Freq_fa, 300, 0, 1);
        return;
    }
}

void motor_Sample_Owari(void)
{
    heat_Motor_Up();                             /* 采样结束 抬起加热体电机 */
    white_Motor_PD();                            /* 运动白板电机 PD位置 清零位置 */
    motor_Tray_Move_By_Index(eTrayIndex_2);      /* 出仓 */
    barcode_Motor_Run_By_Index(eBarcodeIndex_0); /* 复位 */
    barcode_Motor_Run_By_Index(eBarcodeIndex_6); /* 二维码位置就位 */
    gComm_Data_Sample_Max_Point_Clear();         /* 清除需要测试点数 */
    protocol_Temp_Upload_Resume();               /* 恢复温度上送 */
    led_Mode_Set(eLED_Mode_Keep_Green);          /* LED 绿灯常亮 */
    barcode_Interrupt_Flag_Clear();              /* 清除打断标志位 */
    comm_Data_Sample_Owari();                    /* 上送采样结束报文 */
}

/**
 * @brief  电机任务
 * @param  argument: Not used
 * @retval None
 */
static void motor_Task(void * argument)
{
    BaseType_t xResult = pdFALSE;
    uint32_t xNotifyValue, xTick, xTick2;
    sMotor_Fun mf;
    uint32_t cnt;
    uint8_t buffer[9];
    eBarcodeState barcode_result;

    motor_OPT_Status_Init_Wait_Complete(); /* 等待光耦结果完成 */
    motor_Resource_Init();                 /* 电机驱动、位置初始化 */
    barcode_Init();                        /* 扫码枪初始化 */

    for (;;) {
        xResult = xQueuePeek(motor_Fun_Queue_Handle, &mf, portMAX_DELAY);
        if (xResult != pdPASS) {
            continue;
        }
        cnt = 0;
        switch (mf.fun_type) {
            case eMotor_Fun_In:             /* 入仓 */
                if (heat_Motor_Up() != 0) { /* 抬起上加热体电机 失败 */
                    break;
                };
                motor_Tray_Move_By_Index(eTrayIndex_0); /* 测试位置 */
                heat_Motor_Down();                      /* 砸下上加热体电机 */
                break;
            case eMotor_Fun_Out: /* 出仓 */
                motor_Tray_Move_By_Index(eTrayIndex_2);
                break;
            case eMotor_Fun_Scan:           /* 扫码 */
                if (heat_Motor_Up() != 0) { /* 抬起上加热体电机 失败 */
                    break;
                };
                tray_Move_By_Index(eTrayIndex_1, 5000); /* 运动托盘电机 */
                barcode_Scan_Whole();                   /* 执行扫码 */
                break;
            case eMotor_Fun_PD:                         /* PD值测试 */
                motor_Tray_Move_By_Index(eTrayIndex_0); /* 运动托盘电机 */
                heat_Motor_Down();                      /* 砸下上加热体电机 */
                white_Motor_PD();                       /* 运动白板电机 */
                break;
            case eMotor_Fun_WH:                         /* 白底值测试 */
                motor_Tray_Move_By_Index(eTrayIndex_0); /* 运动托盘电机 */
                heat_Motor_Down();                      /* 砸下上加热体电机 */
                white_Motor_WH();                       /* 运动白板电机 */
                break;
            case eMotor_Fun_Sample_Start:                        /* 准备测试 */
                led_Mode_Set(eLED_Mode_Kirakira_Green);          /* LED 绿灯闪烁 */
                motor_Tray_Move_By_Index(eTrayIndex_1);          /* 扫码位置 */
                barcode_result = barcode_Scan_Whole();           /* 执行扫码 */
                if (barcode_result == eBarcodeState_Interrupt) { /* 中途打断 */
                    motor_Sample_Owari();                        /* 清理 */
                    break;                                       /* 提前结束 */
                }
                motor_Tray_Move_By_Index(eTrayIndex_0);       /* 入仓 */
                heat_Motor_Down();                            /* 砸下上加热体电机 */
                if (gComm_Data_Sample_Max_Point_Get() == 0) { /* 无测试项目 */
                    protocol_Temp_Upload_Resume();            /* 恢复温度上送 */
                    led_Mode_Set(eLED_Mode_Keep_Green);       /* LED 绿灯常亮 */
                    barcode_Interrupt_Flag_Clear();           /* 清除打断标志位 */
                    break;                                    /* 提前结束 */
                }
                white_Motor_WH();                   /* 运动白板电机 白物质位置 */
                if (barcode_Interrupt_Flag_Get()) { /* 中途打断 */
                    motor_Sample_Owari();           /* 清理 */
                    break;                          /* 提前结束 */
                }
                comm_Data_Sample_Start(); /* 启动定时器同步发包 开始采样 */
                for (;;) {
                    xResult = xTaskNotifyWait(0, 0xFFFFFFFF, &xNotifyValue, pdMS_TO_TICKS(8500)); /* 等待任务通知 */
                    if (xResult != pdTRUE || xNotifyValue == eMotorNotifyValue_BR) {              /* 超时 或 收到中终止命令 直接退出循环 */
                        beep_Start_With_Conf(eBeep_Freq_mi, 500, 500, 3);                         /* 蜂鸣器输出调试 */
                        break;
                    }
                    if (xNotifyValue == eMotorNotifyValue_PD) {           /* 准备移动到PD位置 */
                        beep_Conf_Set_Period_Cnt(1);                      /* 蜂鸣器配置 */
                        beep_Start_With_Loop();                           /* 蜂鸣器输出调试 */
                        white_Motor_PD();                                 /* 运动白板电机 PD位置 清零位置 */
                        comm_Data_PD_Next_Flag_Mark();                    /* 标记发送下一包 */
                    } else if (xNotifyValue == eMotorNotifyValue_WH) {    /* 准备移动到白板位置 */
                        beep_Conf_Set_Period_Cnt(1);                      /* 蜂鸣器配置 */
                        beep_Start_With_Loop();                           /* 蜂鸣器输出调试 */
                        white_Motor_WH();                                 /* 运动白板电机 白物质位置 */
                    } else if (xNotifyValue == eMotorNotifyValue_LO) {    /* 等待最后一次测量完成 */
                        beep_Start_With_Conf(eBeep_Freq_re, 100, 100, 5); /* 蜂鸣器输出调试 */
                        break;
                    } else {
                        break;
                    }
                }
                motor_Sample_Owari(); /* 清理 */
                break;
            case eMotor_Fun_SYK:            /* 交错 */
                if (heat_Motor_Up() != 0) { /* 抬起上加热体电机 失败 */
                    break;
                };
                tray_Move_By_Index(eTrayIndex_2, 5000); /* 运动托盘电机 */
                heat_Motor_Down();                      /* 砸下上加热体电机 */
                white_Motor_Run(eMotorDir_FWD, 3000);   /* 运动白板电机 */
                barcode_Scan_By_Index(eBarcodeIndex_0);
                break;
            case eMotor_Fun_RLB: /* 回滚 */
                motor_Resource_Init();
                break;
            case eMotor_Fun_PRE_TRAY: /* 压力测试 托盘 */
                do {
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
                } while (1);
                break;
            case eMotor_Fun_PRE_BARCODE: /* 压力测试 扫码 */
                do {
                    switch (cnt % 7) {
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
                    }
                    ++cnt;
                } while (1);
                break;
            case eMotor_Fun_PRE_HEATER: /* 压力测试 上加热体 */
                do {
                    if (cnt % 2 == 0) {
                        heat_Motor_Down(); /* 砸下上加热体电机 */
                    } else {
                        heat_Motor_Up(); /* 抬起加热体电机 */
                    }
                    ++cnt;
                } while (1);
                break;
            case eMotor_Fun_PRE_WHITE: /* 压力测试 白板 */
                xTick = xTaskGetTickCount();
                do {
                    white_Motor_Toggle(3000); /* 切换白板电机位置 */
                    ++cnt;
                    xTick2 = xTaskGetTickCount();
                    buffer[5] = (xTick2 >> 24) & 0xFF;
                    buffer[6] = (xTick2 >> 16) & 0xFF;
                    buffer[7] = (xTick2 >> 8) & 0xFF;
                    buffer[8] = (xTick2 >> 0) & 0xFF;
                    xTick = xTick2 - xTick;
                    buffer[0] = cnt;
                    buffer[1] = (xTick >> 24) & 0xFF;
                    buffer[2] = (xTick >> 16) & 0xFF;
                    buffer[3] = (xTick >> 8) & 0xFF;
                    buffer[4] = (xTick >> 0) & 0xFF;
                    comm_Out_SendTask_QueueEmit(buffer, ARRAY_LEN(buffer), 0);
                    xTick = xTaskGetTickCount();
                } while (1);
                break;
            case eMotor_Fun_PRE_ALL: /* 压力测试 */
                do {
                    switch (cnt % 3) {
                        case 0:
                            motor_Tray_Move_By_Index(eTrayIndex_0);
                            barcode_Scan_By_Index(eBarcodeIndex_0);
                            heat_Motor_Down(); /* 砸下上加热体电机 */
                            break;
                        case 1:
                            motor_Tray_Move_By_Index(eTrayIndex_1);
                            barcode_Scan_By_Index(eBarcodeIndex_6);
                            break;
                        case 2:
                            motor_Tray_Move_By_Index(eTrayIndex_2);
                            barcode_Scan_By_Index(eBarcodeIndex_3);
                            break;
                    }
                    white_Motor_Toggle(3000); /* 切换白板电机位置 */
                    ++cnt;
                } while (1);
                break;

            default:
                break;
        }
        xTaskNotifyWait(0, 0xFFFFFFFF, &xNotifyValue, 0); /* 清除任务通知 */
        xQueueReceive(motor_Fun_Queue_Handle, &mf, 0);
    }
}

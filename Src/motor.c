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

/**
 * @brief  电机资源初始化
 * @param  None
 * @retval None
 */
void motor_Resource_Init(void)
{
    m_l6470_Init(); /* 驱动资源及参数初始化 */

    barcode_Motor_Init(); /* 扫码电机初始化 */

    /* 警告 上加热体电机不抬起 不允许操作托盘电机 */
    m_drv8824_Init();  /* 上加热体电机初始化 */
    tray_Motor_Init(); /* 托盘电机初始化 */

    barcode_Motor_Reset_Pos(); /* 重置扫码电机位置 */
    tray_Motor_Reset_Pos();    /* 重置托盘电机位置 */

    barcode_Init();                         /* 扫码枪初始化 */
    motor_Tray_Move_By_Index(eTrayIndex_1); /* 扫码位置 */
    barcode_Scan_Whole();                   /* 执行扫码 */

    motor_Tray_Move_By_Index(eTrayIndex_0); /* 测试位置 */
    heat_Motor_Run(eMotorDir_REV, 3000);    /* 砸下上加热体电机 */
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
    if (xTaskCreate(motor_Task, "Motor Task", 160, NULL, TASK_PRIORITY_MOTOR, &motor_Task_Handle) != pdPASS) {
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

    if (heat_Motor_Position_Is_Up() == 0 && heat_Motor_Run(eMotorDir_FWD, 3000) != 0) { /* 上加热体光耦位置未被阻挡 则抬起上加热体电机 */
        buffer[0] = 0x00;                                                               /* 抬起上加热体失败 */
        comm_Out_SendTask_QueueEmitWithBuildCover(eProtocoleRespPack_Client_DISH, buffer, 1); /* 上报失败报文 */
        return;
    };
    if (tray_Move_By_Index(index, 5000) == eTrayState_OK) { /* 运动托盘电机 */
        buffer[0] = 0x01;                                   /* 托盘在检测位置 */
        comm_Out_SendTask_QueueEmitWithBuildCover(eProtocoleRespPack_Client_DISH, buffer, 1);
    } else {
        buffer[0] = 0x00;
        comm_Out_SendTask_QueueEmitWithBuildCover(eProtocoleRespPack_Client_DISH, buffer, 1);
    }
}

/**
 * @brief  电机任务
 * @param  argument: Not used
 * @retval None
 */
static void motor_Task(void * argument)
{
    BaseType_t xResult = pdFALSE;
    uint32_t xNotifyValue;
    sMotor_Fun mf;
    uint32_t cnt = 0;

    motor_Resource_Init();

    for (;;) {
        xResult = xQueueReceive(motor_Fun_Queue_Handle, &mf, portMAX_DELAY);
        if (xResult != pdPASS) {
            continue;
        }
        switch (mf.fun_type) {
            case eMotor_Fun_In:                                 /* 入仓 */
                if (heat_Motor_Run(eMotorDir_FWD, 3000) != 0) { /* 抬起上加热体电机 失败 */
                    break;
                };
                motor_Tray_Move_By_Index(eTrayIndex_1);
                barcode_Scan_Whole();
                break;
            case eMotor_Fun_Out: /* 出仓 */
                motor_Tray_Move_By_Index(eTrayIndex_2);
                break;
            case eMotor_Fun_Scan:                               /* 扫码 */
                if (heat_Motor_Run(eMotorDir_FWD, 3000) != 0) { /* 抬起上加热体电机 失败 */
                    break;
                };
                tray_Move_By_Index(eTrayIndex_1, 5000); /* 运动托盘电机 */
                barcode_Scan_Whole();                   /* 执行扫码 */
                break;
            case eMotor_Fun_PD:                         /* PD值测试 */
                motor_Tray_Move_By_Index(eTrayIndex_0); /* 运动托盘电机 */
                heat_Motor_Run(eMotorDir_REV, 3000);    /* 砸下上加热体电机 */
                white_Motor_PD();                       /* 运动白板电机 */
                break;
            case eMotor_Fun_WH:                         /* 白底值测试 */
                motor_Tray_Move_By_Index(eTrayIndex_0); /* 运动托盘电机 */
                heat_Motor_Run(eMotorDir_REV, 3000);    /* 砸下上加热体电机 */
                white_Motor_WH();                       /* 运动白板电机 */
                break;
            case eMotor_Fun_Sample_Start:               /* 准备测试 */
                motor_Tray_Move_By_Index(eTrayIndex_2); /* 运动托盘电机 */
                heat_Motor_Run(eMotorDir_REV, 3000);    /* 砸下上加热体电机 */
                white_Motor_WH();                       /* 运动白板电机 白物质位置 */
                comm_Data_Sample_Start();               /* 启动定时器同步发包 开始采样 */
                for (;;) {
                    xResult = xTaskNotifyWait(0, 0xFFFFFFFF, &xNotifyValue, pdMS_TO_TICKS(6000)); /* 等待任务通知 */
                    if (xResult != pdTRUE || xNotifyValue == eMotorNotifyValue_BR) {              /* 超时 或 收到中终止命令 直接退出循环 */
                        break;
                    }
                    if (xNotifyValue == eMotorNotifyValue_PD) {
                        vTaskDelay(1500); /* 延时等待测量完成 */
                        white_Motor_PD(); /* 运动白板电机 PD位置 清零位置 */
                    } else if (xNotifyValue == eMotorNotifyValue_WH) {
                        vTaskDelay(1500); /* 延时等待测量完成 */
                        white_Motor_WH(); /* 运动白板电机 白物质位置 */
                    } else if (xNotifyValue == eMotorNotifyValue_TG) {
                        vTaskDelay(1500);         /* 延时等待测量完成 */
                        white_Motor_Toggle(3000); /* 运动白板电机 切换位置 */
                    } else if (xNotifyValue == eMotorNotifyValue_LO) {
                        vTaskDelay(1500);         /* 延时等待测量完成 */
                        white_Motor_Toggle(3000); /* 运动白板电机 切换位置 */
                        break;
                    } else {
                        break;
                    }
                }
                soft_timer_Temp_Resume();               /* 恢复温度上送 */
                heat_Motor_Run(eMotorDir_FWD, 3000);    /* 采样结束 抬起加热体电机 */
                white_Motor_PD();                       /* 运动白板电机 PD位置 清零位置 */
                motor_Tray_Move_By_Index(eTrayIndex_2); /* 运动托盘电机 */
                comm_Data_Sample_Owari();
                break;
            case eMotor_Fun_SYK:                                /* 交错 */
                if (heat_Motor_Run(eMotorDir_FWD, 3000) != 0) { /* 抬起上加热体电机 失败 */
                    break;
                };
                tray_Move_By_Index(eTrayIndex_2, 5000); /* 运动托盘电机 */
                heat_Motor_Run(eMotorDir_REV, 3000);    /* 砸下上加热体电机 */
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
                    heat_Motor_Run(eMotorDir_FWD + (cnt % 2), 3000); /* 砸下上加热体电机 */
                    ++cnt;
                } while (1);
                break;
            case eMotor_Fun_PRE_WHITE: /* 压力测试 白板 */
                do {
                    white_Motor_Toggle(3000); /* 切换白板电机位置 */
                    ++cnt;
                } while (1);
                break;
            case eMotor_Fun_PRE_ALL: /* 压力测试 */
                do {
                    switch (cnt % 3) {
                        case 0:
                            motor_Tray_Move_By_Index(eTrayIndex_0);
                            barcode_Scan_By_Index(eBarcodeIndex_0);
                            heat_Motor_Run(eMotorDir_REV, 3000); /* 砸下上加热体电机 */
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
    }
}

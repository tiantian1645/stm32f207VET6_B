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

/* Extern variables ----------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
xQueueHandle motor_Fun_Queue_Handle = NULL; /* 电机功能队列 */

/* Private constants ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
static void motor_Task(void * argument);
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

    barcode_Scan_By_Index(eBarcodeIndex_6); /* QR Code 位置 */
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
    if (xTaskCreate(motor_Task, "Motor Task", 160, NULL, TASK_PRIORITY_MOTOR, NULL) != pdPASS) {
        Error_Handler();
    }
}

/**
 * @brief  电机任务 提交
 * @param  pFun_type 任务详情指针
 * @param  timeout  最长等待时间
 * @retval 0 提交成功 1 提交失败
 */
uint8_t motor_Emit(eMotor_Fun * pFun_type, uint32_t timeout)
{
    if (xQueueSendToBack(motor_Fun_Queue_Handle, pFun_type, timeout) != pdTRUE) {
        return 1;
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
    sMotor_Fun mf;

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
                tray_Move_By_Index(eTrayIndex_0, 5000); /* 运动托盘电机 */
                heat_Motor_Run(eMotorDir_REV, 3000);    /* 砸下上加热体电机 */
                break;
            case eMotor_Fun_Out:                                /* 出仓 */
                if (heat_Motor_Run(eMotorDir_FWD, 3000) != 0) { /* 抬起上加热体电机 失败 */
                    break;
                };
                tray_Move_By_Index(eTrayIndex_2, 5000); /* 运动托盘电机 */
                break;
            case eMotor_Fun_Scan:                               /* 扫码 */
                if (heat_Motor_Run(eMotorDir_FWD, 3000) != 0) { /* 抬起上加热体电机 失败 */
                    break;
                };
                tray_Move_By_Index(eTrayIndex_1, 5000); /* 运动托盘电机 */
                barcode_Scan_Whole();                   /* 执行扫码 */
                break;
            case eMotor_Fun_PD:                                 /* PD值测试 */
                if (heat_Motor_Run(eMotorDir_FWD, 3000) != 0) { /* 抬起上加热体电机 失败 */
                    break;
                };
                tray_Move_By_Index(eTrayIndex_0, 5000); /* 运动托盘电机 */
                heat_Motor_Run(eMotorDir_REV, 3000);    /* 砸下上加热体电机 */
                white_Motor_Run(eMotorDir_FWD, 3000);   /* 运动白板电机 */
                break;
            case eMotor_Fun_WH:                                 /* 白底值测试 */
                if (heat_Motor_Run(eMotorDir_FWD, 3000) != 0) { /* 抬起上加热体电机 失败 */
                    break;
                };
                tray_Move_By_Index(eTrayIndex_0, 5000); /* 运动托盘电机 */
                heat_Motor_Run(eMotorDir_REV, 3000);    /* 砸下上加热体电机 */
                white_Motor_Run(eMotorDir_REV, 3000);   /* 运动白板电机 */
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
            default:
                break;
        }
    }
}

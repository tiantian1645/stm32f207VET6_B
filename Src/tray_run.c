/**
 * @file    tray_scan.c
 * @brief   托盘电机控制
 *
 * 只有初始位置有一个光耦
 * 托盘电机运动控制流程
 *  1 初始光耦位置作为参照起点
 *      1.1 起始位置处于光耦位置 --> 移动固定步数
 *      1.2 起始位置不处于光耦位置 --> 先移动到光耦位置 --> 再移动到固定步数
 *  2 电机当前运动绝对步数作为参照起点
 *      2.1 起始位置处于光耦位置 --> 设置当前电机运动步数 --> 移动固定步数
 *      2.2 起始位置不处于光耦位置 --> 检查在光耦位置时是否设置过电机运动步数
 *          2.2.1 光耦位置时没记录过电机运动步数 --> 1
 *          2.2.2 根据电机当前运动步数移动步数量
 *
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "m_l6470.h"
#include "motor.h"
#include "tray_run.h"

/* Extern variables ----------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/


/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/
#define TRAY_MOTOR_IS_OPT_1 (HAL_GPIO_ReadPin(OPTSW_OUT1_GPIO_Port, OPTSW_OUT1_Pin) == GPIO_PIN_RESET) /* 光耦输入 */
#define TRAY_MOTOR_IS_OPT_2 (HAL_GPIO_ReadPin(OPTSW_OUT2_GPIO_Port, OPTSW_OUT2_Pin) == GPIO_PIN_RESET) /* 光耦输入 */
#define TRAY_MOTOR_IS_BUSY (dSPIN_Busy_HW())                                                           /* 托盘电机忙碌位读取 */
#define TRAY_MOTOR_IS_FLAG (dSPIN_Flag())                                                              /* 托盘电机标志脚读取 */
#define TRAY_MOTOR_MAX_DISP 16000                                                                      /* 托盘电机运动最大步数 物理限制步数 */

/* Private variables ---------------------------------------------------------*/
static sMotorRunStatus gTrayMotorRunStatus;
static sMoptorRunCmdInfo gTrayMotorRunCmdInfo;

/* Private function prototypes -----------------------------------------------*/

/* Private user code ---------------------------------------------------------*/

/**
 * @brief  托盘电机 读取驱动中记录的步数
 * @param  None
 * @retval 驱动中记录的步数
 */
int32_t tray_Motor_Read_Position(void)
{
    int32_t position;
    position = (((int32_t)((dSPIN_Get_Param(dSPIN_ABS_POS)) << 10)) >> 10);
    return position;
}

/**
 * @brief  托盘电机停车
 * @param  None
 * @retval 运动结果 0 正常 1 异常
 */
uint8_t tray_Motor_Brake(void)
{
    dSPIN_Hard_Stop();
    return 0;
}

/**
 * @brief  电机运动前回调
 * @param  None
 * @retval 运动结果 0 正常 1 异常
 */
eTrayState tray_Motor_Enter(void)
{
    if (m_l6470_Index_Switch(eM_L6470_Index_1, 500) != 0) { /* 获取SPI总线资源 */
        return eTrayState_Tiemout;
    }

    if (dSPIN_Busy_HW()) { /* 软件检测忙碌状态 */
        return eTrayState_Busy;
    } else {
        return eTrayState_OK;
    }
}

/**
 * @brief  根据电机驱动状态设置电机状态
 * @note
 * @param  status 电机驱动状态值 dSPIN_STATUS_Masks_TypeDef
 * @retval None
 */
void tray_Motor_Deal_Status()
{
    uint16_t status;

    status = dSPIN_Get_Status(); /* 读取电机状态 */

    if (((status & dSPIN_STATUS_STEP_LOSS_A) == 0) || ((status & dSPIN_STATUS_STEP_LOSS_B) == 0)) { /* 发生失步 */
        // m_l6470_Reset_HW();                                                                         /* 硬件重置 */
        // m_l6470_Params_Init();                                                                      /* 初始化参数 */
        return;
    }
    if (((status & dSPIN_STATUS_UVLO)) == 0) { /* 低压 */
                                               // m_l6470_Reset_HW();                    /* 硬件重置 */
        // m_l6470_Params_Init();                 /* 初始化参数 */
        return;
    }
    if (((status & dSPIN_STATUS_TH_WRN)) == 0 || ((status & dSPIN_STATUS_TH_SD)) == 0 || ((status & dSPIN_STATUS_OCD)) == 0) { /* 高温 超温 过流 */
        // m_l6470_Reset_HW();                                                                                                    /* 硬件重置 */
        // m_l6470_Params_Init();                                                                                                 /* 初始化参数 */
        return;
    }
    return;
}

/**
 * @brief  电机运动后回调 光耦位置硬件检测停车
 * @note   光耦位置处重置电机状态记录中步数值 重置电机驱动中步数记录值
 * @param  None
 * @retval 运动结果 0 正常 1 运动超时
 */
eTrayState tray_Motor_Leave_On_OPT(void)
{
    while (xTaskGetTickCount() - motor_Status_Get_TickInit(&gTrayMotorRunStatus) < motor_CMD_Info_Get_Tiemout(&gTrayMotorRunCmdInfo)) { /* 超时等待 */
        if (TRAY_MOTOR_IS_BUSY == 0 || TRAY_MOTOR_IS_OPT_2) { /* 已配置硬件检测停车 电机驱动空闲状态 */
            tray_Motor_Brake();                               /* 刹车 */
            dSPIN_Reset_Pos();                                /* 重置电机驱动步数记录 */
            tray_Motor_Deal_Status();
            motor_Status_Set_Position(&gTrayMotorRunStatus, 0); /* 重置电机状态步数记录 */
            return eTrayState_OK;
        }
        vTaskDelay(1); /* 延时 */
    }
    tray_Motor_Brake(); /* 刹车 */
    tray_Motor_Deal_Status();
    return eTrayState_Error; /* 超时返回错误 */
}

/**
 * @brief  电机运动后回调 被动检查停车状态
 * @param  None
 * @retval 运动结果 0 正常 1 异常
 */
eTrayState tray_Motor_Leave_On_Busy_Bit(void)
{
    while (xTaskGetTickCount() - motor_Status_Get_TickInit(&gTrayMotorRunStatus) < motor_CMD_Info_Get_Tiemout(&gTrayMotorRunCmdInfo)) { /* 超时等待 */
        if (TRAY_MOTOR_IS_BUSY == 0) {                                                   /* 电机驱动空闲状态 */
            tray_Motor_Brake();                                                          /* 刹车 */
            motor_Status_Set_Position(&gTrayMotorRunStatus, tray_Motor_Read_Position()); /* 更新电机状态步数记录 */
            return eTrayState_OK;
        }
        vTaskDelay(1); /* 延时 */
    }
    motor_Status_Set_Position(&gTrayMotorRunStatus, tray_Motor_Read_Position()); /* 更新电机状态步数记录 */
    return eTrayState_Error;                                                     /* 超时返回错误 */
}

/**
 * @brief  托盘电机运动
 * @param  runInfo 运动信息
 * @retval 运动结果 0 正常 1 异常
 */
eTrayState tray_Motor_Run(void)
{
    eTrayState result;

    result = gTrayMotorRunCmdInfo.pfEnter();
    if (result != eTrayState_OK) { /* 入口回调 */
        if (result != eTrayState_Tiemout) {
            m_l6470_release(); /* 释放SPI总线资源*/
        }
        return result;
    }

    motor_Status_Set_TickInit(&gTrayMotorRunStatus, xTaskGetTickCount()); /* 记录开始时钟脉搏数 */

    switch (gTrayMotorRunCmdInfo.dir) {
        case eMotorDir_REV:
            dSPIN_Move(REV, gTrayMotorRunCmdInfo.step); /* 向驱动发送指令 */
            break;
        case eMotorDir_FWD:
        default:
            dSPIN_Move(FWD, gTrayMotorRunCmdInfo.step); /* 向驱动发送指令 */
            break;
    }

    result = gTrayMotorRunCmdInfo.pfLeave(); /* 出口回调 */
    tray_Motor_Deal_Status();                /* 读取电机驱动状态清除标志 */
    m_l6470_release();                       /* 释放SPI总线资源*/
    return result;
}

/**
 * @brief  初始化电机位置
 * @param  None
 * @retval 0 成功 1 失败
 */
eTrayState tray_Motor_Init(void)
{
    eTrayState result;
    uint8_t cnt = 0;

    result = tray_Motor_Enter();
    if (result != eTrayState_OK) { /* 入口回调 */
        if (result != eTrayState_Tiemout) {
            m_l6470_release(); /* 释放SPI总线资源*/
        }
        return result;
    }

    if (TRAY_MOTOR_IS_OPT_1) {
        dSPIN_Move(FWD, 300);
        do {
            vTaskDelay(100);
        } while (TRAY_MOTOR_IS_BUSY && ++cnt < 1000);
        cnt = 0;
    }
    if (TRAY_MOTOR_IS_FLAG) {
        tray_Motor_Deal_Status();
    }

    dSPIN_Go_Until(ACTION_RESET, REV, 8000);

    do {
        vTaskDelay(100);
    } while (TRAY_MOTOR_IS_BUSY && ++cnt < 1000);
    tray_Motor_Brake(); /* 刹车 */
    m_l6470_release();  /* 释放SPI总线资源*/
    return eTrayState_OK;
}

/**
 * @brief  计算需要移动的方向和步数
 * @param  target_step 目标位置距离初始位置运动的步数
 * @retval 若移动目标位置超过物理限制 返回 1 否则 0
 */
void tray_Motor_Calculate(uint32_t target_step)
{
    int32_t current_position;

    current_position = motor_Status_Get_Position(&gTrayMotorRunStatus);

    if (target_step >= current_position) {
        motor_CMD_Info_Set_Step(&gTrayMotorRunCmdInfo, (target_step - current_position) % TRAY_MOTOR_MAX_DISP);
        motor_CMD_Info_Set_Dir(&gTrayMotorRunCmdInfo, eMotorDir_FWD);
    } else {
        motor_CMD_Info_Set_Step(&gTrayMotorRunCmdInfo, (current_position - target_step) % TRAY_MOTOR_MAX_DISP);
        motor_CMD_Info_Set_Dir(&gTrayMotorRunCmdInfo, eMotorDir_REV);
    }
}

/**
 * @brief  扫码执行
 * @param  index   条码位置索引
 * @param  pdata   结果存放指针
 * @param  length  最大结果长度
 * @param  timeout 串口接收等待时间
 * @retval 扫码结果数据长度
 */
eTrayState tray_Move_By_Index(eTrayIndex index, uint32_t timeout)
{
    eTrayState result;

    motor_CMD_Info_Set_PF_Enter(&gTrayMotorRunCmdInfo, tray_Motor_Enter);             /* 配置启动前回调 */
    tray_Motor_Calculate((index >> 5) << 3);                                          /* 计算运动距离 及方向 */
    motor_CMD_Info_Set_Tiemout(&gTrayMotorRunCmdInfo, timeout);                       /* 运动超时时间 1000mS */
    motor_CMD_Info_Set_PF_Leave(&gTrayMotorRunCmdInfo, tray_Motor_Leave_On_Busy_Bit); /* 等待驱动状态位空闲 */

    result = tray_Motor_Run(); /* 执行电机运动 */
    return result;
}

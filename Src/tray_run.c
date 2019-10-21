/**
 * @file    tray_scan.c
 * @brief   托盘电机控制
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
#define TRAY_MOTOR_MAX_DISP 6400                                                                       /* 出仓步数 (1/8) 物理限制步数 */
#define TRAY_MOTOR_SCAN_DISP 1400                                                                      /* 扫码步数 (1/8) */

/* Private variables ---------------------------------------------------------*/
static sMotorRunStatus gTray_Motor_Run_Status;
static sMoptorRunCmdInfo gTray_Motor_Run_CMD_Info;
static uint8_t gTray_Motor_Lock = 0;

/* Private function prototypes -----------------------------------------------*/

/* Private user code ---------------------------------------------------------*/

/**
 * @brief  托盘电机运动锁 获取
 * @param  None
 * @retval 托盘电机运动锁
 */
uint8_t gTray_Motor_Lock_Get(void)
{
    return gTray_Motor_Lock;
}

/**
 * @brief  托盘电机运动锁 设置
 * @param  lock 托盘电机运动锁
 * @retval None
 */
void gTray_Motor_Lock_Set(uint8_t lock)
{
    gTray_Motor_Lock = lock;
}

/**
 * @brief  托盘电机运动锁 设置
 * @param  lock 托盘电机运动锁
 * @retval None
 */
uint8_t tray_Motor_Lock_Check(void)
{
    return gTray_Motor_Lock_Get() == 0;
}

/**
 * @brief  托盘电机运动锁 设置
 * @param  lock 托盘电机运动锁
 * @retval None
 */
void tray_Motor_Lock_Occupy(void)
{
    gTray_Motor_Lock_Set(1);
}

/**
 * @brief  托盘电机运动锁 设置
 * @param  lock 托盘电机运动锁
 * @retval None
 */
void tray_Motor_Lock_Release(void)
{
    gTray_Motor_Lock_Set(0);
}

/**
 * @brief  托盘电机 读取驱动中记录的步数
 * @param  None
 * @retval 驱动中记录的步数
 */
int32_t tray_Motor_Read_Position(void)
{
    int32_t position;
    position = (((int32_t)((dSPIN_Get_Param(dSPIN_ABS_POS)) << 10)) >> 10);
    return position * -1;
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
    while (xTaskGetTickCount() - motor_Status_Get_TickInit(&gTray_Motor_Run_Status) < motor_CMD_Info_Get_Tiemout(&gTray_Motor_Run_CMD_Info)) { /* 超时等待 */
        if (TRAY_MOTOR_IS_BUSY == 0 || TRAY_MOTOR_IS_OPT_1) { /* 已配置硬件检测停车 电机驱动空闲状态 */
            tray_Motor_Brake();                               /* 刹车 */
            dSPIN_Reset_Pos();                                /* 重置电机驱动步数记录 */
            tray_Motor_Deal_Status();
            motor_Status_Set_Position(&gTray_Motor_Run_Status, 0); /* 重置电机状态步数记录 */
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
    while (xTaskGetTickCount() - motor_Status_Get_TickInit(&gTray_Motor_Run_Status) < motor_CMD_Info_Get_Tiemout(&gTray_Motor_Run_CMD_Info)) { /* 超时等待 */
        if (TRAY_MOTOR_IS_BUSY == 0) {                                                      /* 电机驱动空闲状态 */
            tray_Motor_Brake();                                                             /* 刹车 */
            motor_Status_Set_Position(&gTray_Motor_Run_Status, tray_Motor_Read_Position()); /* 更新电机状态步数记录 */
            return eTrayState_OK;
        }
        vTaskDelay(1); /* 延时 */
    }
    motor_Status_Set_Position(&gTray_Motor_Run_Status, tray_Motor_Read_Position()); /* 更新电机状态步数记录 */
    return eTrayState_Error;                                                        /* 超时返回错误 */
}

/**
 * @brief  托盘电机运动
 * @param  runInfo 运动信息
 * @retval 运动结果 0 正常 1 异常
 */
eTrayState tray_Motor_Run(void)
{
    eTrayState result;
    uint32_t cnt = 0;

    if (gTray_Motor_Run_CMD_Info.step == 0) {
        return eTrayState_OK;
    }

    result = gTray_Motor_Run_CMD_Info.pfEnter();
    if (result != eTrayState_OK) { /* 入口回调 */
        if (result != eTrayState_Tiemout) {
            m_l6470_release(); /* 释放SPI总线资源*/
        }
        return result;
    }

    motor_Status_Set_TickInit(&gTray_Motor_Run_Status, xTaskGetTickCount()); /* 记录开始时钟脉搏数 */
    if (gTray_Motor_Run_CMD_Info.step < 0xFFFFFF) {
        switch (gTray_Motor_Run_CMD_Info.dir) {
            case eMotorDir_REV:
                dSPIN_Move(FWD, gTray_Motor_Run_CMD_Info.step); /* 向驱动发送指令 */
                break;
            case eMotorDir_FWD:
            default:
                dSPIN_Move(REV, gTray_Motor_Run_CMD_Info.step); /* 向驱动发送指令 */
                break;
        }
        vTaskDelay(5);
    } else {
        cnt = 0;
        if (TRAY_MOTOR_IS_OPT_1) {
            dSPIN_Move(REV, 350); /* 0.5 毫米 */
            while (TRAY_MOTOR_IS_BUSY && ++cnt <= 600000)
                ;
        }
        dSPIN_Go_Until(ACTION_RESET, FWD, 40000);
        cnt = 0;
        while (TRAY_MOTOR_IS_BUSY && ++cnt <= 60000)
            ;
        dSPIN_Move(FWD, 24); /* 0.5 毫米 */
    }

    result = gTray_Motor_Run_CMD_Info.pfLeave(); /* 出口回调 */
    tray_Motor_Deal_Status();                    /* 读取电机驱动状态清除标志 */
    m_l6470_release();                           /* 释放SPI总线资源*/
    return result;
}

/**
 * @brief  重置电机状态位置
 * @param  timeout 停车等待超时
 * @retval 0 成功 1 失败
 */

uint8_t tray_Motor_Reset_Pos()
{
    TickType_t xTick;

    xTick = xTaskGetTickCount();
    while ((!TRAY_MOTOR_IS_OPT_1) && xTaskGetTickCount() - xTick < 5000) { /* 检测光耦是否被遮挡 */
        vTaskDelay(1);
    }

    if (TRAY_MOTOR_IS_OPT_1) {
        if (tray_Motor_Enter() != eTrayState_Tiemout) {
            tray_Motor_Brake();                                    /* 刹车 */
            dSPIN_Reset_Pos();                                     /* 重置电机驱动步数记录 */
            m_l6470_release();                                     /* 释放SPI总线资源*/
            motor_Status_Set_Position(&gTray_Motor_Run_Status, 0); /* 重置电机状态步数记录 */
            return 0;
        }
        return 2;
    }
    return 1;
}

/**
 * @brief  初始化电机位置
 * @param  None
 * @retval 0 成功 1 失败
 */
eTrayState tray_Motor_Init(void)
{
    eTrayState result;
    TickType_t xTick;

    result = tray_Motor_Enter();
    if (result != eTrayState_OK) { /* 入口回调 */
        if (result != eTrayState_Tiemout) {
            m_l6470_release(); /* 释放SPI总线资源*/
        }
        return result;
    }

    if (TRAY_MOTOR_IS_OPT_1) {
        dSPIN_Move(REV, 350);
        xTick = xTaskGetTickCount();
        while (TRAY_MOTOR_IS_OPT_1 && xTaskGetTickCount() - xTick < 5000) {
            vTaskDelay(1);
        }
        tray_Motor_Brake(); /* 刹车 */
    }
    if (TRAY_MOTOR_IS_FLAG) {
        tray_Motor_Deal_Status();
    }

    dSPIN_Go_Until(ACTION_RESET, FWD, 40000);
    m_l6470_release(); /* 释放SPI总线资源*/
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

    current_position = motor_Status_Get_Position(&gTray_Motor_Run_Status);

    if (target_step >= current_position) {
        motor_CMD_Info_Set_Step(&gTray_Motor_Run_CMD_Info, (target_step - current_position));
        motor_CMD_Info_Set_Dir(&gTray_Motor_Run_CMD_Info, eMotorDir_FWD);
    } else {
        motor_CMD_Info_Set_Step(&gTray_Motor_Run_CMD_Info, (current_position - target_step));
        motor_CMD_Info_Set_Dir(&gTray_Motor_Run_CMD_Info, eMotorDir_REV);
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

    if (!tray_Motor_Lock_Check()) {
        return eTrayState_Busy;
    }

    motor_CMD_Info_Set_PF_Enter(&gTray_Motor_Run_CMD_Info, tray_Motor_Enter); /* 配置启动前回调 */
    motor_CMD_Info_Set_Tiemout(&gTray_Motor_Run_CMD_Info, timeout);           /* 运动超时时间 1000mS */
    if (index != eTrayIndex_0) {
        tray_Motor_Calculate((index >> 5) << 3);                                              /* 计算运动距离 及方向 32细分转8细分 */
        motor_CMD_Info_Set_PF_Leave(&gTray_Motor_Run_CMD_Info, tray_Motor_Leave_On_Busy_Bit); /* 等待驱动状态位空闲 */
    } else {
        motor_CMD_Info_Set_PF_Leave(&gTray_Motor_Run_CMD_Info, tray_Motor_Leave_On_OPT); /* 等待驱动状态位空闲 */
        motor_CMD_Info_Set_Step(&gTray_Motor_Run_CMD_Info, 0xFFFFFF);
    }
    result = tray_Motor_Run(); /* 执行电机运动 */
    return result;
}

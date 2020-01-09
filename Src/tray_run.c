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
#define TRAY_MOTOR_IS_BUSY (dSPIN_Busy_SW()) /* 托盘电机忙碌位读取 */
#define TRAY_MOTOR_IS_FLAG (dSPIN_Flag())    /* 托盘电机标志脚读取 */
#define TRAY_PATCH (24)

/* Private variables ---------------------------------------------------------*/
static sMotorRunStatus gTray_Motor_Run_Status;
static sMoptorRunCmdInfo gTray_Motor_Run_CMD_Info;

static uint8_t gTray_Motor_Scan_EE = 0;
static uint8_t gTray_Motor_Scan_Reverse = 0;
/* Private function prototypes -----------------------------------------------*/

/* Private user code ---------------------------------------------------------*/

/**
 * @brief  托盘电机 丢步异常使能标记
 * @param  None
 * @retval None
 */
void tray_Motor_EE_Mark(void)
{
    gTray_Motor_Scan_EE = 1;
}

/**
 * @brief  托盘电机 丢步异常使能清零
 * @param  None
 * @retval None
 */
void tray_Motor_EE_Clear(void)
{
    gTray_Motor_Scan_EE = 0;
}

/**
 * @brief  托盘电机 丢步异常使能获取
 * @param  None
 * @retval gTray_Motor_Scan_EE 大于0 返回1 其余返回 0
 */
uint8_t tray_Motor_EE_Get(void)
{
    return (gTray_Motor_Scan_EE > 0) ? (1) : (0);
}

/**
 * @brief  托盘电机 丢步发生后反向补偿标记标记
 * @param  None
 * @retval None
 */
void tray_Motor_Scan_Reverse_Mark(void)
{
    gTray_Motor_Scan_Reverse = 1;
}

/**
 * @brief  托盘电机 丢步发生后反向补偿标记清零
 * @param  None
 * @retval None
 */
void tray_Motor_Scan_Reverse_Clear(void)
{
    gTray_Motor_Scan_Reverse = 0;
}

/**
 * @brief  托盘电机 丢步发生后反向补偿标记获取
 * @param  None
 * @retval gTray_Motor_Scan_Reverse 大于0 返回1 其余返回 0
 */
uint8_t tray_Motor_Scan_Reverse_Get(void)
{
    return (gTray_Motor_Scan_Reverse > 0) ? (1) : (0);
}

/**
 * @brief  托盘电机 获取电机运动记录中位置
 * @param  None
 * @retval 运动状态记录的步数
 */
int32_t tray_Motor_Get_Status_Position(void)
{
    return motor_Status_Get_Position(&gTray_Motor_Run_Status);
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
        m_l6470_Reset_HW();                                                                         /* 硬件重置 */
        m_l6470_Params_Init();                                                                      /* 初始化参数 */
        error_Emit(eError_Motor_Tray_Status_Warui);                                                 /* 提交错误信息 */
        return;
    }
    if (((status & dSPIN_STATUS_UVLO)) == 0) {      /* 低压 */
        m_l6470_Reset_HW();                         /* 硬件重置 */
        m_l6470_Params_Init();                      /* 初始化参数 */
        error_Emit(eError_Motor_Tray_Status_Warui); /* 提交错误信息 */
        return;
    }
    if (((status & dSPIN_STATUS_TH_WRN)) == 0 || ((status & dSPIN_STATUS_TH_SD)) == 0 || ((status & dSPIN_STATUS_OCD)) == 0) { /* 高温 超温 过流 */
        m_l6470_Reset_HW();                                                                                                    /* 硬件重置 */
        m_l6470_Params_Init();                                                                                                 /* 初始化参数 */
        error_Emit(eError_Motor_Tray_Status_Warui);                                                                            /* 提交错误信息 */
        return;
    }
    error_Emit(eError_Motor_Tray_Debug); /* 压力测试 */
    return;
}

/**
 * @brief  电机运动前回调
 * @param  None
 * @retval 运动结果 0 正常 1 异常
 */
eTrayState tray_Motor_Enter(void)
{
    if (m_l6470_Index_Switch(eM_L6470_Index_1, 2500) != 0) { /* 获取SPI总线资源 */
        return eTrayState_Tiemout;
    }

    if (dSPIN_Flag()) {
        tray_Motor_Deal_Status();
    }

    if (dSPIN_Busy_HW()) { /* 软件检测忙碌状态 */
        return eTrayState_Busy;
    } else {
        return eTrayState_OK;
    }
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
        vTaskDelay(5); /* 延时 */
    }
    tray_Motor_Brake(); /* 刹车 */
    tray_Motor_Deal_Status();
    return eTrayState_Error; /* 超时返回错误 */
}

/**
 * @brief  电机运动后回调 光耦位置硬件检测停车 扫码位置
 * @note   光耦位置处重置电机状态记录中步数值 重置电机驱动中步数记录值
 * @param  None
 * @retval 运动结果 0 正常 1 运动超时
 */
eTrayState tray_Motor_Leave_On_OPT_2(void)
{
    TickType_t xTick;

    while (xTaskGetTickCount() - motor_Status_Get_TickInit(&gTray_Motor_Run_Status) < motor_CMD_Info_Get_Tiemout(&gTray_Motor_Run_CMD_Info)) { /* 超时等待 */
        if (TRAY_MOTOR_IS_BUSY == 0 || TRAY_MOTOR_IS_OPT_2) {                               /* 已配置硬件检测停车 电机驱动空闲状态 */
            tray_Motor_Brake();                                                             /* 刹车 */
            tray_Motor_Deal_Status();                                                       /* 状态脚处理 */
            motor_Status_Set_Position(&gTray_Motor_Run_Status, tray_Motor_Read_Position()); /* 更新电机状态步数记录 */
            tray_Motor_Brake();                                                             /* 刹车 */
            dSPIN_Move(FWD, 1200);                                                          /* 扫码光耦补偿 */
            xTick = xTaskGetTickCount();                                                    /* 计时起点 */
            do {
                vTaskDelay(100);
            } while (TRAY_MOTOR_IS_BUSY && (xTaskGetTickCount() - xTick < 500));
            tray_Motor_Brake(); /* 刹车 */
            return eTrayState_OK;
        }
        vTaskDelay(5); /* 延时 */
    }
    tray_Motor_Brake();       /* 刹车 */
    tray_Motor_Deal_Status(); /* 状态脚处理 */
    return eTrayState_Error;  /* 超时返回错误 */
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
        vTaskDelay(5); /* 延时 */
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
    TickType_t xTick = 0;

    error_Emit(eError_Motor_Tray_Debug);
    if (gTray_Motor_Run_CMD_Info.step == 0) {
        return eTrayState_OK;
    }

    result = gTray_Motor_Run_CMD_Info.pfEnter();
    if (result != eTrayState_OK) { /* 入口回调 */
        if (result != eTrayState_Tiemout) {
            m_l6470_release(); /* 释放SPI总线资源*/
        }
        if (result == eTrayState_Busy) {
            error_Emit(eError_Motor_Tray_Busy);
        } else if (result == eTrayState_Tiemout) {
            error_Emit(eError_Motor_Tray_Timeout);
        }
        return result;
    }

    motor_Status_Set_TickInit(&gTray_Motor_Run_Status, xTaskGetTickCount()); /* 记录开始时钟脉搏数 */
    if (gTray_Motor_Run_CMD_Info.step < 0xFFFFFF) {
        switch (gTray_Motor_Run_CMD_Info.dir) {
            case eMotorDir_REV:
                tray_Motor_EE_Clear();                                                /* 清除托盘丢步标志位 */
                dSPIN_Set_Param(dSPIN_MAX_SPEED, Index_1_dSPIN_CONF_PARAM_MAX_SPEED); /* 进仓恢复最大速度 */
                dSPIN_Move(FWD, gTray_Motor_Run_CMD_Info.step);                       /* 向驱动发送指令 */
                break;
            case eMotorDir_FWD:
            default:
                dSPIN_Set_Param(dSPIN_MAX_SPEED, Index_1_dSPIN_CONF_PARAM_MAX_SPEED / 2); /* 出仓速度减半 */
                dSPIN_Move(REV, gTray_Motor_Run_CMD_Info.step);                           /* 向驱动发送指令 */
                break;
        }
    } else {
        if (TRAY_MOTOR_IS_OPT_1) {
            dSPIN_Move(REV, 350);
            xTick = xTaskGetTickCount();
            while (TRAY_MOTOR_IS_OPT_1 && xTaskGetTickCount() - xTick < 5000) {
                vTaskDelay(100);
            }
            tray_Motor_Brake(); /* 刹车 */
        }
        if (TRAY_MOTOR_IS_FLAG) {
            tray_Motor_Deal_Status();
        }
        dSPIN_Set_Param(dSPIN_MAX_SPEED, Index_1_dSPIN_CONF_PARAM_MAX_SPEED); /* 进仓恢复最大速度 */

        tray_Motor_EE_Clear(); /* 清除托盘丢步标志位 */
        dSPIN_Move(FWD, (eTrayIndex_2 >> 5) << 3);
        xTick = xTaskGetTickCount();
        do {
            vTaskDelay(100);
        } while (TRAY_MOTOR_IS_BUSY && (xTaskGetTickCount() - xTick < 2500));
        dSPIN_Move(FWD, TRAY_PATCH); /* 弥补偏差 */
        xTick = xTaskGetTickCount();
        do {
            vTaskDelay(100);
        } while (TRAY_MOTOR_IS_BUSY && (xTaskGetTickCount() - xTick < 200));
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
    if (tray_Motor_Enter() == eTrayState_Tiemout) { /* 获取资源失败 */
        return 2;
    }

    tray_Motor_Brake();                                        /* 无条件刹车 */
    if (TRAY_MOTOR_IS_OPT_1) {                                 /* 光耦被遮挡 */
        dSPIN_Reset_Pos();                                     /* 重置电机驱动步数记录 */
        tray_Motor_Deal_Status();                              /* 状态处理 */
        motor_Status_Set_Position(&gTray_Motor_Run_Status, 0); /* 重置电机状态步数记录 */
        vTaskDelay(400);                                       /* 延时 */
        m_l6470_release();                                     /* 释放SPI总线资源*/
        return 0;
    }
    m_l6470_release(); /* 释放SPI总线资源*/
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

    error_Emit(eError_Motor_Tray_Debug);
    result = tray_Motor_Enter();
    if (result != eTrayState_OK) { /* 入口回调 */
        if (result != eTrayState_Tiemout) {
            m_l6470_release(); /* 释放SPI总线资源*/
        }
        if (result == eTrayState_Busy) {
            error_Emit(eError_Motor_Tray_Busy);
        } else if (result == eTrayState_Tiemout) {
            error_Emit(eError_Motor_Tray_Timeout);
        }
        return result;
    }

    if (TRAY_MOTOR_IS_OPT_1) {
        dSPIN_Move(REV, 350);
        xTick = xTaskGetTickCount();
        do {
            vTaskDelay(100);
        } while (TRAY_MOTOR_IS_OPT_1 && xTaskGetTickCount() - xTick < 5000);
        tray_Motor_Brake(); /* 刹车 */
    }
    if (TRAY_MOTOR_IS_FLAG) {
        tray_Motor_Deal_Status();
    }

    tray_Motor_EE_Clear(); /* 清除托盘丢步标志位 */
    xTick = xTaskGetTickCount();
    dSPIN_Move(FWD, (eTrayIndex_2 >> 5) << 3);
    do {
        vTaskDelay(100);
    } while (TRAY_MOTOR_IS_BUSY && (xTaskGetTickCount() - xTick < 2500));
    dSPIN_Move(FWD, TRAY_PATCH); /* 弥补偏差 */
    xTick = xTaskGetTickCount();
    do {
        vTaskDelay(100);
    } while (TRAY_MOTOR_IS_BUSY && (xTaskGetTickCount() - xTick < 200));
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

    motor_CMD_Info_Set_PF_Enter(&gTray_Motor_Run_CMD_Info, tray_Motor_Enter); /* 配置启动前回调 */
    motor_CMD_Info_Set_Tiemout(&gTray_Motor_Run_CMD_Info, timeout);           /* 运动超时时间 1000mS */
    switch (index) {
        case eTrayIndex_0:
            motor_CMD_Info_Set_PF_Leave(&gTray_Motor_Run_CMD_Info, tray_Motor_Leave_On_OPT); /* 等待驱动状态位空闲 */
            motor_CMD_Info_Set_Step(&gTray_Motor_Run_CMD_Info, 0xFFFFFF);
            break;
        case eTrayIndex_1:
            tray_Motor_Calculate(0);                                                           /* 计算运动距离 及方向 32细分转8细分 */
            motor_CMD_Info_Set_PF_Leave(&gTray_Motor_Run_CMD_Info, tray_Motor_Leave_On_OPT_2); /* 等待驱动状态位空闲 */
            break;
        case eTrayIndex_2:
            tray_Motor_Calculate((index >> 5) << 3);                                              /* 计算运动距离 及方向 32细分转8细分 */
            motor_CMD_Info_Set_PF_Leave(&gTray_Motor_Run_CMD_Info, tray_Motor_Leave_On_Busy_Bit); /* 等待驱动状态位空闲 */
            break;
        default:
            return eTrayState_Error;
    }
    result = tray_Motor_Run(); /* 执行电机运动 */
    return result;
}

/**
 * @brief  扫码光耦中断处理
 * @param  None
 * @retval None
 */
void tray_Motor_ISR_Deal(void)
{
    if (HAL_GPIO_ReadPin(OPTSW_OUT2_GPIO_Port, OPTSW_OUT2_Pin) == GPIO_PIN_RESET) { /* 光耦被遮挡 */
        if (tray_Motor_EE_Get()) {
            error_Emit_FromISR(eError_Tray_Motor_Lose);
            tray_Motor_Scan_Reverse_Mark();
        }
    }
}

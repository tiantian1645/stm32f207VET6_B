/**
 * @file    white_motor.c
 * @brief   上白板电机控制
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "motor.h"
#include "m_drv8824.h"

/* Extern variables ----------------------------------------------------------*/
extern TIM_HandleTypeDef htim1;

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/
#define WHITE_MOTOR_PCS_MAX 40000
#define WHITE_MOTOR_PCS_MIN 24000
#define WHITE_MOTOR_PCS_GAP 300
#define WHITE_MOTOR_PCS_UNT 5
#define WHITE_MOTOR_PCS_SUM 476

/* Private variables ---------------------------------------------------------*/
static eMotorDir gWhite_Motor_Dir = eMotorDir_FWD;
static uint32_t gWhite_Motor_Position = 0xFFFFFFFF;
static uint32_t gWhite_Motor_SRC_Buffer[3] = {0, 0, 0};
static uint8_t gWhite_Motor_Lock = 0;

/* Private constants ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
static void gWhite_Motor_Position_Set(uint32_t position);
static void gWhite_Motor_Position_Inc(uint32_t position);
static void gWhite_Motor_Position_Clr(void);

/* Private user code ---------------------------------------------------------*/

/**
 * @brief  白板电机运动锁 获取
 * @param  None
 * @retval 白板电机运动锁
 */
uint8_t gWhite_Motor_Lock_Get(void)
{
    return gWhite_Motor_Lock;
}

/**
 * @brief  白板电机运动锁 设置
 * @param  lock 白板电机运动锁
 * @retval None
 */
void gWhite_Motor_Lock_Set(uint8_t lock)
{
    gWhite_Motor_Lock = lock;
}

/**
 * @brief  白板电机运动锁 设置
 * @param  lock 白板电机运动锁
 * @retval None
 */
uint8_t white_Motor_Lock_Check(void)
{
    return gWhite_Motor_Lock_Get() == 0;
}

/**
 * @brief  白板电机运动锁 设置
 * @param  lock 白板电机运动锁
 * @retval None
 */
void white_Motor_Lock_Occupy(void)
{
    gWhite_Motor_Lock_Set(1);
}

/**
 * @brief  白板电机运动锁 设置
 * @param  lock 白板电机运动锁
 * @retval None
 */
void white_Motor_Lock_Release(void)
{
    gWhite_Motor_Lock_Set(0);
}

/**
 * @brief  白板电机方向 获取
 * @param  None
 * @retval 白板电机方向
 */
eMotorDir gWhite_Motor_Dir_Get(void)
{
    return gWhite_Motor_Dir;
}

/**
 * @brief  白板电机方向 设置
 * @param  dir 白板电机方向
 * @retval None
 */
void gWhite_Motor_Dir_Set(eMotorDir dir)
{
    gWhite_Motor_Dir = dir;
}

/**
 * @brief  白板电机位置 获取
 * @param  None
 * @retval 白板电机位置
 */
uint32_t gWhite_Motor_Position_Get(void)
{
    return gWhite_Motor_Position;
}

/**
 * @brief  白板电机位置 使能
 * @param  None
 * @retval None
 */
void white_Motor_Active(void)
{
    HAL_GPIO_WritePin(STEP_NCS1_GPIO_Port, STEP_NCS1_Pin, GPIO_PIN_RESET);
}

/**
 * @brief  白板电机位置 失能
 * @param  None
 * @retval None
 */
void white_Motor_Deactive(void)
{
    HAL_GPIO_WritePin(STEP_NCS1_GPIO_Port, STEP_NCS1_Pin, GPIO_PIN_SET);
}

/**
 * @brief  白板电机位置 检查是否已经处于伸展状态
 * @note   已运动步数超过极限位置80%
 * @param  None
 * @retval 白板电机位置
 */
uint8_t white_Motor_Position_Is_Out(void)
{
    return (gWhite_Motor_Position_Get() != 0xFFFFFFFF) && (gWhite_Motor_Position_Get() > WHITE_MOTOR_PCS_SUM * WHITE_MOTOR_PCS_UNT * 90 / 100);
}

/**
 * @brief  白板电机位置 检查是否已经处于收缩状态
 * @note   已运动步数为0
 * @param  None
 * @retval 白板电机位置
 */
uint8_t white_Motor_Position_Is_In(void)
{
    if (HAL_GPIO_ReadPin(OPTSW_OUT4_GPIO_Port, OPTSW_OUT4_Pin) == GPIO_PIN_RESET) {
        white_Motor_Deactive();
        return 1;
    }
    return 0;
}

/**
 * @brief  白板电机位置 设置
 * @param  白板电机位置
 * @retval None
 */
static void gWhite_Motor_Position_Set(uint32_t position)
{
    gWhite_Motor_Position = position;
}

/**
 * @brief  白板电机位置 增量
 * @param  白板电机位置
 * @retval None
 */
static void gWhite_Motor_Position_Inc(uint32_t position)
{
    gWhite_Motor_Position_Set(gWhite_Motor_Position_Get() + position);
}

/**
 * @brief  白板电机位置 清零
 * @param  None
 * @retval None
 */
static void gWhite_Motor_Position_Clr(void)
{
    gWhite_Motor_Position_Set(0);
}

/**
 * @brief  白板电机位置 重置
 * @param  None
 * @retval None
 */
static void gWhite_Motor_Position_Rst(void)
{
    gWhite_Motor_Position_Set(0xFFFFFFFF);
}

/**
 * @brief 白板电机 停车确认
 * @param  None
 * @retval None
 */
uint8_t white_Motor_Wait_Stop(uint32_t timeout)
{
    switch (gWhite_Motor_Dir_Get()) {
        case eMotorDir_REV:
            do {
                if (white_Motor_Position_Is_In()) {
                    PWM_AW_Stop();
                    m_drv8824_release();
                    gWhite_Motor_Position_Clr();
                    return 0;
                }
            } while (--timeout);
            return 1;
        case eMotorDir_FWD:
        default:
            do {
                if (white_Motor_Position_Is_Out()) {
                    return 0;
                }
            } while (--timeout);
            return 1;
    }
    return 2;
}

/**
 * @brief  启动DMA PWM输出
 * @param  None
 * @retval 启动结果
 */
uint8_t white_Motor_Run(eMotorDir dir, uint32_t timeout)
{
    if (white_Motor_Position_Is_In()) { /* 光耦被遮挡 处于抬起状态 */
        gWhite_Motor_Position_Clr();    /* 清空位置记录 */
        if (dir == eMotorDir_REV) {     /* 仍然收到向上运动指令 */
            return 0;
        }
    } else if (dir == eMotorDir_FWD && white_Motor_Position_Is_Out()) { /* 向下运动指令 但已运动步数超过极限位置80% */
        return 0;
    }

    if (!white_Motor_Lock_Check()) { /* 检查运动锁 */
        return 1;
    }

    gWhite_Motor_Position_Rst();                               /* 重置位置记录置非法值 0xFFFFFFFF */
    m_drv8824_Index_Switch(eM_DRV8824_Index_0, portMAX_DELAY); /* 等待PWM资源 */
    m_drv8824_SetDir(dir);                                     /* 运动方向设置 硬件管脚 */
    gWhite_Motor_Dir_Set(dir);                                 /* 运动方向设置 目标方向 */

    gPWM_TEST_AW_CNT_Clear();                                 /* PWM数目清零 */
    __HAL_TIM_CLEAR_IT(&htim1, TIM_IT_UPDATE);                /* 清除更新事件标志位 */
    __HAL_TIM_SET_COUNTER(&htim1, 0);                         /* 清零定时器计数寄存器 */
    if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1) != HAL_OK) { /* 启动PWM输出 */
        return 2;
    }

    PWM_AW_IRQ_CallBcak();

    if (white_Motor_Wait_Stop(6000000)) {
        m_drv8824_release();
        return 0;
    }
    return 3;
}

/**
 * @brief  白板电机向上运动 PWM输出控制
 * @param  None
 * @retval 0 输出完成 1 输出未完成
 */
uint8_t white_Motor_PWM_Gen_In(void)
{
    uint32_t cnt = 0;

    cnt = gPWM_TEST_AW_CNT_Get();

    if (cnt > WHITE_MOTOR_PCS_SUM * 110 / 100 || white_Motor_Position_Is_In()) {
        PWM_AW_Stop();
        return 0;
    }

    gWhite_Motor_SRC_Buffer[0] = (WHITE_MOTOR_PCS_MAX - WHITE_MOTOR_PCS_MIN > WHITE_MOTOR_PCS_GAP * cnt) ? (WHITE_MOTOR_PCS_MAX - WHITE_MOTOR_PCS_GAP * cnt)
                                                                                                         : (WHITE_MOTOR_PCS_MIN); /* 周期长度 */
    gWhite_Motor_SRC_Buffer[1] = WHITE_MOTOR_PCS_UNT;                                                                             /* 重复次数 */
    gWhite_Motor_SRC_Buffer[2] = (gWhite_Motor_SRC_Buffer[0] + 1) / 2;                                                            /* 占空比 默认50% */
    /* burst模式修改时基单元 */
    HAL_TIM_DMABurst_WriteStart(&htim1, TIM_DMABASE_ARR, TIM_DMA_UPDATE, (uint32_t *)gWhite_Motor_SRC_Buffer, TIM_DMABURSTLENGTH_3TRANSFERS);
    gPWM_TEST_AW_CNT_Inc(); /* 自增脉冲计数 */
    return 1;
}

/**
 * @brief  白板电机向下运动 PWM输出控制
 * @param  None
 * @retval 0 输出完成 1 输出未完成
 */
uint8_t white_Motor_PWM_Gen_Out(void)
{
    uint32_t cnt;

    cnt = gPWM_TEST_AW_CNT_Get(); /* 获取当前脉冲计数 */

    if (cnt >= WHITE_MOTOR_PCS_SUM) { /* 停止输出 */
        PWM_AW_Stop();
        return 0;
    } else {
        gWhite_Motor_SRC_Buffer[0] = (WHITE_MOTOR_PCS_MAX - WHITE_MOTOR_PCS_MIN > WHITE_MOTOR_PCS_GAP * cnt) ? (WHITE_MOTOR_PCS_MAX - WHITE_MOTOR_PCS_GAP * cnt)
                                                                                                             : (WHITE_MOTOR_PCS_MIN); /* 周期长度 */
        gWhite_Motor_SRC_Buffer[1] = WHITE_MOTOR_PCS_UNT;                                                                             /* 重复次数 */
        gWhite_Motor_SRC_Buffer[2] = (gWhite_Motor_SRC_Buffer[0] + 1) / 2;                                                            /* 占空比 默认50% */
        /* burst模式修改时基单元 */
        HAL_TIM_DMABurst_WriteStart(&htim1, TIM_DMABASE_ARR, TIM_DMA_UPDATE, (uint32_t *)gWhite_Motor_SRC_Buffer, TIM_DMABURSTLENGTH_3TRANSFERS);
        gWhite_Motor_Position_Inc(gWhite_Motor_SRC_Buffer[1]); /* 自增位置记录 */
    }
    gPWM_TEST_AW_CNT_Inc(); /* 自增脉冲计数 */
    return 1;
}
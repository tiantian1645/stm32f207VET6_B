/**
 * @file    heat_motor.c
 * @brief   上加热体电机控制
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
#define HEAT_MOTOR_PCS_MAX 24048
#define HEAT_MOTOR_PCS_MIN 14500
#define HEAT_MOTOR_PCS_GAP 78
#define HEAT_MOTOR_PCS_UNT 20
#define HEAT_MOTOR_PCS_SUM 110

/* Private variables ---------------------------------------------------------*/
static eMotorDir gHeat_Motor_Dir = eMotorDir_FWD;
static uint32_t gHeat_Motor_Position = 0xFFFFFFFF;
static uint32_t gHeat_Motor_SRC_Buffer[3] = {0, 0, 0};
static uint8_t gHeat_Motor_Lock = 0;

/* Private constants ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
static void gHeat_Motor_Position_Set(uint32_t position);
static void gHeat_Motor_Position_Inc(uint32_t position);
static void gHeat_Motor_Position_Clr(void);

/* Private user code ---------------------------------------------------------*/

/**
 * @brief  加热体电机运动锁 获取
 * @param  None
 * @retval 加热体电机运动锁
 */
uint8_t gHeat_Motor_Lock_Get(void)
{
    return gHeat_Motor_Lock;
}

/**
 * @brief  加热体电机运动锁 设置
 * @param  lock 加热体电机运动锁
 * @retval None
 */
void gHeat_Motor_Lock_Set(uint8_t lock)
{
    gHeat_Motor_Lock = lock;
}

/**
 * @brief  加热体电机运动锁 设置
 * @param  lock 加热体电机运动锁
 * @retval None
 */
uint8_t heat_Motor_Lock_Check(void)
{
    return gHeat_Motor_Lock_Get() == 0;
}

/**
 * @brief  加热体电机运动锁 设置
 * @param  lock 加热体电机运动锁
 * @retval None
 */
void heat_Motor_Lock_Occupy(void)
{
    gHeat_Motor_Lock_Set(1);
}

/**
 * @brief  加热体电机运动锁 设置
 * @param  lock 加热体电机运动锁
 * @retval None
 */
void heat_Motor_Lock_Release(void)
{
    gHeat_Motor_Lock_Set(0);
}

/**
 * @brief  加热体电机方向 获取
 * @param  None
 * @retval 加热体电机方向
 */
eMotorDir gHeat_Motor_Dir_Get(void)
{
    return gHeat_Motor_Dir;
}

/**
 * @brief  加热体电机方向 设置
 * @param  dir 加热体电机方向
 * @retval None
 */
void gHeat_Motor_Dir_Set(eMotorDir dir)
{
    gHeat_Motor_Dir = dir;
}

/**
 * @brief  加热体电机位置 获取
 * @param  None
 * @retval 加热体电机位置
 */
uint32_t gHeat_Motor_Position_Get(void)
{
    return gHeat_Motor_Position;
}

/**
 * @brief  加热体电机位置 使能
 * @param  None
 * @retval None
 */
void heat_Motor_Active(void)
{
    HAL_GPIO_WritePin(STEP_NCS2_GPIO_Port, STEP_NCS2_Pin, GPIO_PIN_RESET);
}

/**
 * @brief  加热体电机位置 失能
 * @param  None
 * @retval None
 */
void heat_Motor_Deactive(void)
{
    HAL_GPIO_WritePin(STEP_NCS2_GPIO_Port, STEP_NCS2_Pin, GPIO_PIN_SET);
}

/**
 * @brief  加热体电机位置 检查是否已经处于被压下状态
 * @note   已运动步数超过极限位置80%
 * @param  None
 * @retval 加热体电机位置
 */
uint8_t heat_Motor_Position_Is_Down(void)
{
    return (gHeat_Motor_Position_Get() != 0xFFFFFFFF) && (gHeat_Motor_Position_Get() > HEAT_MOTOR_PCS_SUM * HEAT_MOTOR_PCS_UNT * 80 / 100);
}

/**
 * @brief  加热体电机位置 检查是否已经处于被抬起状态
 * @note   已运动步数为0
 * @param  None
 * @retval 加热体电机位置
 */
uint8_t heat_Motor_Position_Is_Up(void)
{
    if (HAL_GPIO_ReadPin(OPTSW_OUT3_GPIO_Port, OPTSW_OUT3_Pin) == GPIO_PIN_RESET) {
        heat_Motor_Deactive();
        return 1;
    }
    return 0;
}

/**
 * @brief  加热体电机位置 设置
 * @param  加热体电机位置
 * @retval None
 */
static void gHeat_Motor_Position_Set(uint32_t position)
{
    gHeat_Motor_Position = position;
}

/**
 * @brief  加热体电机位置 增量
 * @param  加热体电机位置
 * @retval None
 */
static void gHeat_Motor_Position_Inc(uint32_t position)
{
    gHeat_Motor_Position_Set(gHeat_Motor_Position_Get() + position);
}

/**
 * @brief  加热体电机位置 清零
 * @param  None
 * @retval None
 */
static void gHeat_Motor_Position_Clr(void)
{
    gHeat_Motor_Position_Set(0);
}

/**
 * @brief  加热体电机位置 重置
 * @param  None
 * @retval None
 */
static void gHeat_Motor_Position_Rst(void)
{
    gHeat_Motor_Position_Set(0xFFFFFFFF);
}

/**
 * @brief 加热体电机 停车确认
 * @param  None
 * @retval None
 */
uint8_t heat_Motor_Wait_Stop(uint32_t timeout)
{
    switch (gHeat_Motor_Dir_Get()) {
        case eMotorDir_FWD:
            do {
                if (heat_Motor_Position_Is_Up()) {
                    PWM_AW_Stop();
                    m_drv8824_release();
                    gHeat_Motor_Position_Clr();
                    return 0;
                }
            } while (--timeout);
            return 1;
        case eMotorDir_REV:
        default:
            do {
                if (heat_Motor_Position_Is_Down()) {
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
uint8_t heat_Motor_Run(eMotorDir dir, uint32_t timeout)
{
    if (heat_Motor_Position_Is_Up()) { /* 光耦被遮挡 处于抬起状态 */
        gHeat_Motor_Position_Clr();    /* 清空位置记录 */
        if (dir == eMotorDir_FWD) {    /* 仍然收到向上运动指令 */
            return 0;
        }
    } else if (dir == eMotorDir_REV && heat_Motor_Position_Is_Down()) { /* 向下运动指令 但已运动步数超过极限位置80% */
        return 0;
    }

    if (!heat_Motor_Lock_Check()) { /* 检查运动锁 */
        return 1;
    }

    gHeat_Motor_Position_Rst();                                /* 重置位置记录置非法值 0xFFFFFFFF */
    m_drv8824_Index_Switch(eM_DRV8824_Index_1, portMAX_DELAY); /* 等待PWM资源 */
    m_drv8824_SetDir(dir);                                     /* 运动方向设置 硬件管脚 */
    gHeat_Motor_Dir_Set(dir);                                  /* 运动方向设置 目标方向 */

    gPWM_TEST_AW_CNT_Clear();                                 /* PWM数目清零 */
    __HAL_TIM_CLEAR_IT(&htim1, TIM_IT_UPDATE);                /* 清除更新事件标志位 */
    __HAL_TIM_SET_COUNTER(&htim1, 0);                         /* 清零定时器计数寄存器 */
    if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1) != HAL_OK) { /* 启动PWM输出 */
        return 2;
    }

    PWM_AW_IRQ_CallBcak();

    if (heat_Motor_Wait_Stop(6000000)) {
        return 0;
    }
    return 3;
}

/**
 * @brief  加热体电机向上运动 PWM输出控制
 * @param  None
 * @retval 0 输出完成 1 输出未完成
 */
uint8_t heat_Motor_PWM_Gen_Up(void)
{
    uint32_t total = 0;

    total = gPWM_TEST_AW_CNT_Get();

    if (total > HEAT_MOTOR_PCS_SUM * 4 || heat_Motor_Position_Is_Up()) {
        PWM_AW_Stop();
        return 0;
    }

    if (total > 0 && (total % (2 * HEAT_MOTOR_PCS_SUM) == 0)) {
        HAL_GPIO_TogglePin(STEP_DIR2_GPIO_Port, STEP_DIR2_Pin); /* 切换方向 托盘出仓位置时 旋转方向有可能会被顶住 */
    }

    gHeat_Motor_SRC_Buffer[0] = (HEAT_MOTOR_PCS_MAX - HEAT_MOTOR_PCS_MIN > HEAT_MOTOR_PCS_GAP * (total % HEAT_MOTOR_PCS_SUM))
                                    ? (HEAT_MOTOR_PCS_MAX - HEAT_MOTOR_PCS_GAP * (total % HEAT_MOTOR_PCS_SUM))
                                    : (HEAT_MOTOR_PCS_MAX);          /* 周期长度 */
    gHeat_Motor_SRC_Buffer[1] = HEAT_MOTOR_PCS_UNT;                  /* 重复次数 */
    gHeat_Motor_SRC_Buffer[2] = (gHeat_Motor_SRC_Buffer[0] + 1) / 2; /* 占空比 默认50% */
    /* burst模式修改时基单元 */
    HAL_TIM_DMABurst_WriteStart(&htim1, TIM_DMABASE_ARR, TIM_DMA_UPDATE, (uint32_t *)gHeat_Motor_SRC_Buffer, TIM_DMABURSTLENGTH_3TRANSFERS);
    gPWM_TEST_AW_CNT_Inc(); /* 自增脉冲计数 */
    return 1;
}

/**
 * @brief  加热体电机向下运动 PWM输出控制
 * @param  None
 * @retval 0 输出完成 1 输出未完成
 */
uint8_t heat_Motor_PWM_Gen_Down(void)
{
    uint32_t cnt;

    cnt = gPWM_TEST_AW_CNT_Get(); /* 获取当前脉冲计数 */

    if (cnt >= HEAT_MOTOR_PCS_SUM) { /* 停止输出 */
        PWM_AW_Stop();
        return 0;
    } else {
        gHeat_Motor_SRC_Buffer[0] = (HEAT_MOTOR_PCS_MAX - HEAT_MOTOR_PCS_MIN > HEAT_MOTOR_PCS_GAP * cnt) ? (HEAT_MOTOR_PCS_MAX - HEAT_MOTOR_PCS_GAP * cnt)
                                                                                                         : (HEAT_MOTOR_PCS_MIN); /* 周期长度 */
        gHeat_Motor_SRC_Buffer[1] = HEAT_MOTOR_PCS_UNT;                                                                          /* 重复次数 */
        gHeat_Motor_SRC_Buffer[2] = (gHeat_Motor_SRC_Buffer[0] + 1) / 2;                                                         /* 占空比 默认50% */
        /* burst模式修改时基单元 */
        HAL_TIM_DMABurst_WriteStart(&htim1, TIM_DMABASE_ARR, TIM_DMA_UPDATE, (uint32_t *)gHeat_Motor_SRC_Buffer, TIM_DMABURSTLENGTH_3TRANSFERS);
        gHeat_Motor_Position_Inc(gHeat_Motor_SRC_Buffer[1]); /* 自增位置记录 */
    }
    gPWM_TEST_AW_CNT_Inc(); /* 自增脉冲计数 */
    return 1;
}

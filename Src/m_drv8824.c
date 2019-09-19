/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "motor.h"
#include "m_drv8824.h"

/* Extern variables ----------------------------------------------------------*/
extern TIM_HandleTypeDef htim1;

/* Private includes ----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
#define PWM_PCS_MAX 24048
#define PWM_PCS_MIN 14500
#define PWM_PCS_GAP 78
#define PWM_PCS_UNT 20
#define PWM_PCS_SUM 110

/* Private macro -------------------------------------------------------------*/
#define DRV8824_HEAT_IS_OPT (HAL_GPIO_ReadPin(OPTSW_OUT3_GPIO_Port, OPTSW_OUT3_Pin) == GPIO_PIN_RESET) /* 光耦输入 */

/* Private typedef -----------------------------------------------------------*/
typedef struct {
    uint32_t pcs;
    uint32_t rcr;
    uint32_t ccr;
    uint16_t dup;
} sPWM_AW_Conf;

/* Private variables ---------------------------------------------------------*/
static eM_DRV8824_Index gMDRV8824Index = eM_DRV8824_Index_0;
static uint32_t aSRC_Buffer[3] = {0, 0, 0};
static uint32_t gPWM_TEST_AW_CNT = 0;
static SemaphoreHandle_t m_drv8824_spi_sem = NULL;
static eMotorDir gMDRV8824_Heat_Dir = eMotorDir_FWD;
static uint32_t gMDRV8824_Heat_Position = 0;

/* Private function prototypes -----------------------------------------------*/
static uint8_t m_drv8824_acquire(uint32_t timeout);

/* Private user code ---------------------------------------------------------*/

/**
 * @brief  失能 单个电机驱动
 * @param  None
 * @retval None
 */
void m_drv8824_Deactive(void)
{
    switch (gMDRV8824Index) {
        case eM_DRV8824_Index_0:
            HAL_GPIO_WritePin(STEP_NCS1_GPIO_Port, STEP_NCS1_Pin, GPIO_PIN_SET);
            break;
        case eM_DRV8824_Index_1:
            HAL_GPIO_WritePin(STEP_NCS2_GPIO_Port, STEP_NCS2_Pin, GPIO_PIN_SET);
        default:
            break;
    }
}

/**
 * @brief  失能 所有电机驱动
 * @param  None
 * @retval None
 */
void m_drv8824_Deactive_All(void)
{
    HAL_GPIO_WritePin(STEP_NCS1_GPIO_Port, STEP_NCS1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(STEP_NCS2_GPIO_Port, STEP_NCS2_Pin, GPIO_PIN_SET);
}

/**
 * @brief  失能 单个电机驱动
 * @param  None
 * @retval None
 */
uint8_t m_drv8824_Get_Flag(void)
{
    switch (gMDRV8824Index) {
        case eM_DRV8824_Index_0:
            if (HAL_GPIO_ReadPin(STEP_NFLG1_GPIO_Port, STEP_NFLG1_Pin) == GPIO_PIN_RESET) {
                return 0x01;
            }
            return 0x00;
        case eM_DRV8824_Index_1:
            if (HAL_GPIO_ReadPin(STEP_NFLG2_GPIO_Port, STEP_NFLG2_Pin) == GPIO_PIN_RESET) {
                return 0x01;
            }
            return 0x00;
        default:
            return 0x00;
    }
}

/**
 * @brief  重置 所有电机驱动
 * @param  None
 * @retval None
 */
void m_drv8824_Reset_All(void)
{
    HAL_GPIO_WritePin(STEP_NRST_GPIO_Port, STEP_NCS1_Pin, GPIO_PIN_RESET);
    vTaskDelay(1);
    HAL_GPIO_WritePin(STEP_NRST_GPIO_Port, STEP_NCS1_Pin, GPIO_PIN_SET);
}

/**
 * @brief  PWM资源锁初始化
 * @param  None
 * @retval None
 */
void m_drv8824_Init(void)
{
    m_drv8824_Deactive_All();
    m_drv8824_Reset_All();
    m_drv8824_spi_sem = xSemaphoreCreateBinary();
    if (m_drv8824_spi_sem == NULL || xSemaphoreGive(m_drv8824_spi_sem) != pdPASS) {
        Error_Handler();
    }
    heat_Motor_Run(eMotorDir_FWD);
}

/**
 * @brief  方向设置
 * @param  dir 方向枚举
 * @retval None
 */
void m_drv8824_SetDir(eMotorDir dir)
{
    switch (dir) {
        case eMotorDir_FWD:
            switch (gMDRV8824Index) {
                case eM_DRV8824_Index_0:
                    HAL_GPIO_WritePin(STEP_DIR1_GPIO_Port, STEP_DIR1_Pin, GPIO_PIN_SET);
                    break;
                case eM_DRV8824_Index_1:
                    HAL_GPIO_WritePin(STEP_DIR2_GPIO_Port, STEP_DIR2_Pin, GPIO_PIN_SET);
                default:
                    break;
            }
            break;

        default:
        case eMotorDir_REV:
            switch (gMDRV8824Index) {
                case eM_DRV8824_Index_0:
                    HAL_GPIO_WritePin(STEP_DIR1_GPIO_Port, STEP_DIR1_Pin, GPIO_PIN_RESET);
                    break;
                case eM_DRV8824_Index_1:
                    HAL_GPIO_WritePin(STEP_DIR2_GPIO_Port, STEP_DIR2_Pin, GPIO_PIN_RESET);
                default:
                    break;
            }
            break;
    }
}

/**
 * @brief  获取PWM资源
 * @param  timeout 等待资源超时时间
 * @retval None
 */
static uint8_t m_drv8824_acquire(uint32_t timeout)
{
    if (xSemaphoreTake(m_drv8824_spi_sem, timeout) == pdPASS) {
        return 0;
    }
    return 1;
}

/**
 * @brief  释放PWM资源
 * @param  None
 * @retval None
 */
uint8_t m_drv8824_release(void)
{
    m_drv8824_Deactive_All();
    if (xSemaphoreGive(m_drv8824_spi_sem) == pdPASS) {
        return 0;
    }
    return 1;
}

/**
 * @brief  释放PWM资源 中断版本
 * @param  None
 * @retval None
 */
uint8_t m_drv8824_release_ISR(void)
{
    BaseType_t xHigherPriorityTaskWoken;

    m_drv8824_Deactive_All();
    if (xSemaphoreGiveFromISR(m_drv8824_spi_sem, &xHigherPriorityTaskWoken) == pdPASS) {
        return 0;
    }
    return 1;
}

/**
 * @brief  索引切换
 * @param  index       索引值
 * @note   index 参考 eM_DRV8824_Index 顺手切换使能管脚
 * @retval None
 */
uint8_t m_drv8824_Index_Switch(eM_DRV8824_Index index, uint32_t timeout)
{
    if (m_drv8824_acquire(timeout) == 0) {
        gMDRV8824Index = index;
        switch (gMDRV8824Index) {
            case eM_DRV8824_Index_0:
                HAL_GPIO_WritePin(STEP_NCS1_GPIO_Port, STEP_NCS1_Pin, GPIO_PIN_RESET);
                HAL_GPIO_WritePin(STEP_NCS2_GPIO_Port, STEP_NCS2_Pin, GPIO_PIN_SET);
                break;
            case eM_DRV8824_Index_1:
                HAL_GPIO_WritePin(STEP_NCS2_GPIO_Port, STEP_NCS2_Pin, GPIO_PIN_RESET);
                HAL_GPIO_WritePin(STEP_NCS1_GPIO_Port, STEP_NCS1_Pin, GPIO_PIN_SET);
            default:
                break;
        }
        return 0;
    }
    return 1;
}

/**
 * @brief  脉冲计数 获取
 * @param  None
 * @retval 脉冲计数
 */
uint32_t gPWM_TEST_AW_CNT_Get(void)
{
    return gPWM_TEST_AW_CNT;
}

/**
 * @brief  脉冲计数 设置
 * @param  None
 * @retval None
 */
void gPWM_TEST_AW_CNT_Set(uint32_t data)
{
    gPWM_TEST_AW_CNT = data;
}

/**
 * @brief  脉冲计数 +1
 * @param  None
 * @retval None
 */
void gPWM_TEST_AW_CNT_Inc(void)
{
    ++gPWM_TEST_AW_CNT;
}

/**
 * @brief  脉冲计数 清零
 * @param  None
 * @retval None
 */
void gPWM_TEST_AW_CNT_Clear(void)
{
    gPWM_TEST_AW_CNT = 0;
}

/**
 * @brief  加热体电机方向 获取
 * @param  None
 * @retval 加热体电机方向
 */
eMotorDir gMDRV8824_Heat_Dir_Get(void)
{
    return gMDRV8824_Heat_Dir;
}

/**
 * @brief  加热体电机方向 设置
 * @param  加热体电机方向
 * @retval None
 */
void gMDRV8824_Heat_Dir_Set(eMotorDir dir)
{
    gMDRV8824_Heat_Dir = dir;
}

/**
 * @brief  加热体电机位置 获取
 * @param  None
 * @retval 加热体电机位置
 */
uint32_t gMDRV8824_Heat_Position_Get(void)
{
    return gMDRV8824_Heat_Position;
}

/**
 * @brief  加热体电机位置 检查是否已经处于被压下状态
 * @note   已运动步数超过极限位置80%
 * @param  None
 * @retval 加热体电机位置
 */
uint8_t heat_Motor_Position_Is_Down(void)
{
    return gMDRV8824_Heat_Position_Get() > PWM_PCS_SUM * PWM_PCS_UNT * 80 / 100;
}

/**
 * @brief  加热体电机位置 检查是否已经处于被抬起状态
 * @note   已运动步数为0
 * @param  None
 * @retval 加热体电机位置
 */
uint8_t heat_Motor_Position_Is_Up(void)
{
    return gMDRV8824_Heat_Position_Get() == 0;
}

/**
 * @brief  加热体电机位置 设置
 * @param  加热体电机位置
 * @retval None
 */
static void gMDRV8824_Heat_Position_Set(uint32_t position)
{
    gMDRV8824_Heat_Position = position;
}

/**
 * @brief  加热体电机位置 增量
 * @param  加热体电机位置
 * @retval None
 */
static void gMDRV8824_Heat_Position_Inc(uint32_t position)
{
    gMDRV8824_Heat_Position_Set(gMDRV8824_Heat_Position_Get() + position);
}

/**
 * @brief  加热体电机位置 清零
 * @param  None
 * @retval None
 */
static void gMDRV8824_Heat_Position_Clr(void)
{
    gMDRV8824_Heat_Position_Set(0);
}

/**
 * @brief  启动DMA PWM输出
 * @param  None
 * @retval 启动结果
 */
HAL_StatusTypeDef heat_Motor_Run(eMotorDir dir)
{
    HAL_StatusTypeDef status;

    if (DRV8824_HEAT_IS_OPT) {         /* 光耦被遮挡 处于抬起状态 */
        gMDRV8824_Heat_Position_Clr(); /* 清空位置记录 */
        if (dir == eMotorDir_FWD) {    /* 仍然收到向上运动指令 */
            return HAL_OK;
        }
    } else if (dir == eMotorDir_REV && heat_Motor_Position_Is_Down()) { /* 向下运动指令 但已运动步数超过极限位置80% */
        return HAL_OK;
    }

    m_drv8824_Index_Switch(eM_DRV8824_Index_1, portMAX_DELAY);
    m_drv8824_SetDir(dir);
    gMDRV8824_Heat_Dir_Set(dir);

    gPWM_TEST_AW_CNT_Clear();
    __HAL_TIM_CLEAR_IT(&htim1, TIM_IT_UPDATE);
    status = HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    if (status != HAL_OK) {
        return status;
    }
    PWM_AW_IRQ_CallBcak();
    return status;
}

// void PWM_AW_IRQ_CallBcak_Heat_Up(void)
//{
//    static uint16_t total = 0;
//    static uint32_t cnt = 0;
//    static uint8_t cross = 0;
//    static GPIO_PinState last_gpio = GPIO_PIN_SET;
//    GPIO_PinState gpio;
//
//    gpio = HAL_GPIO_ReadPin(OPTSW_OUT3_GPIO_Port, OPTSW_OUT3_Pin);
//    if (last_gpio != gpio) {
//        if (gpio == GPIO_PIN_RESET) {
//            ++cross;
//        }
//        last_gpio = gpio;
//    }
//
//    aSRC_Buffer[0] = (PWM_PCS_MAX - PWM_PCS_MIN > PWM_PCS_GAP * total) ? (PWM_PCS_MAX - PWM_PCS_GAP * total) : (PWM_PCS_MAX); /* 周期长度 */
//    aSRC_Buffer[1] = PWM_PCS_UNT;                                                                                             /* 重复次数 */
//    aSRC_Buffer[2] = (aSRC_Buffer[0] + 1) / 2;                                                                                /* 占空比 默认50% */
//    /* burst模式修改时基单元 */
//    HAL_TIM_DMABurst_WriteStart(&htim1, TIM_DMABASE_ARR, TIM_DMA_UPDATE, (uint32_t *)aSRC_Buffer, TIM_DMABURSTLENGTH_3TRANSFERS);
//    if (cross >= 1) {
//        cnt += aSRC_Buffer[1];
//    }
//    if (cross >= 100) {
//        HAL_TIM_Base_Stop(&htim1);                 /* 停止定时器 */
//        __HAL_TIM_CLEAR_IT(&htim1, TIM_IT_UPDATE); /* 清除更新事件标志位 */
//        __HAL_TIM_SET_COUNTER(&htim1, 0);          /* 清零定时器计数寄存器 */
//        HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);   /* 停止PWM输出 */
//        m_drv8824_release_ISR();                   /* 释放PWM资源 */
//        cross = 0;
//        cnt = 0;
//        total = 0; /* 清零前区间脉冲总计数 */
//        return;
//    }
//
//    ++total;
//}

void PWM_AW_IRQ_CallBcak_Heat_Up(void)
{
    static uint16_t total = 0;

    if (DRV8824_HEAT_IS_OPT || total > PWM_PCS_SUM * 3) {
        total = 0;
        gMDRV8824_Heat_Position_Clr();             /* 清空位置记录 */
        HAL_TIM_Base_Stop(&htim1);                 /* 停止定时器 */
        __HAL_TIM_CLEAR_IT(&htim1, TIM_IT_UPDATE); /* 清除更新事件标志位 */
        __HAL_TIM_SET_COUNTER(&htim1, 0);          /* 清零定时器计数寄存器 */
        HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);   /* 停止PWM输出 */
        m_drv8824_release_ISR();                   /* 释放PWM资源 */
        return;
    }

    aSRC_Buffer[0] = (PWM_PCS_MAX - PWM_PCS_MIN > PWM_PCS_GAP * total) ? (PWM_PCS_MAX - PWM_PCS_GAP * total) : (PWM_PCS_MAX); /* 周期长度 */
    aSRC_Buffer[1] = PWM_PCS_UNT;                                                                                             /* 重复次数 */
    aSRC_Buffer[2] = (aSRC_Buffer[0] + 1) / 2;                                                                                /* 占空比 默认50% */
    /* burst模式修改时基单元 */
    HAL_TIM_DMABurst_WriteStart(&htim1, TIM_DMABASE_ARR, TIM_DMA_UPDATE, (uint32_t *)aSRC_Buffer, TIM_DMABURSTLENGTH_3TRANSFERS);
    ++total;
}

void PWM_AW_IRQ_CallBcak_Heat_Down(void)
{
    uint32_t cnt;

    cnt = gPWM_TEST_AW_CNT_Get(); /* 获取当前脉冲计数 */

    if (cnt >= PWM_PCS_SUM) {                      /* 停止输出 */
        HAL_TIM_Base_Stop(&htim1);                 /* 停止定时器 */
        __HAL_TIM_CLEAR_IT(&htim1, TIM_IT_UPDATE); /* 清除更新事件标志位 */
        __HAL_TIM_SET_COUNTER(&htim1, 0);          /* 清零定时器计数寄存器 */
        HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);   /* 停止PWM输出 */
        m_drv8824_release_ISR();                   /* 释放PWM资源 */
    } else {
        aSRC_Buffer[0] = (PWM_PCS_MAX - PWM_PCS_MIN > PWM_PCS_GAP * cnt) ? (PWM_PCS_MAX - PWM_PCS_GAP * cnt) : (PWM_PCS_MIN); /* 周期长度 */
        aSRC_Buffer[1] = PWM_PCS_UNT;                                                                                         /* 重复次数 */
        aSRC_Buffer[2] = (aSRC_Buffer[0] + 1) / 2;                                                                            /* 占空比 默认50% */
        /* burst模式修改时基单元 */
        HAL_TIM_DMABurst_WriteStart(&htim1, TIM_DMABASE_ARR, TIM_DMA_UPDATE, (uint32_t *)aSRC_Buffer, TIM_DMABURSTLENGTH_3TRANSFERS);
        gMDRV8824_Heat_Position_Inc(aSRC_Buffer[1]); /* 自增位置记录 */
    }
    gPWM_TEST_AW_CNT_Inc(); /* 自增脉冲计数 */
}

void PWM_AW_IRQ_CallBcak(void)
{
    switch (gMDRV8824_Heat_Dir_Get()) {
        case eMotorDir_FWD:
            PWM_AW_IRQ_CallBcak_Heat_Up();
            break;
        case eMotorDir_REV:
        default:
            PWM_AW_IRQ_CallBcak_Heat_Down();
            break;
    }
}

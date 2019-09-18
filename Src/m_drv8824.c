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
#define PWM_PCS_SUM 116

/* Private macro -------------------------------------------------------------*/

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
const sPWM_AW_Conf cgPWM_AW_Confs[116] = {
    {24048 - 1, 20 - 1, 25, 1}, {23970 - 1, 20 - 1, 25, 1}, {23892 - 1, 20 - 1, 25, 1}, {23814 - 1, 20 - 1, 25, 1}, {23736 - 1, 20 - 1, 25, 1},
    {23658 - 1, 20 - 1, 25, 1}, {23580 - 1, 20 - 1, 25, 1}, {23502 - 1, 20 - 1, 25, 1}, {23424 - 1, 20 - 1, 25, 1}, {23346 - 1, 20 - 1, 25, 1},
    {23268 - 1, 20 - 1, 25, 1}, {23190 - 1, 20 - 1, 25, 1}, {23112 - 1, 20 - 1, 25, 1}, {23034 - 1, 20 - 1, 25, 1}, {22956 - 1, 20 - 1, 25, 1},
    {22878 - 1, 20 - 1, 25, 1}, {22800 - 1, 20 - 1, 25, 1}, {22722 - 1, 20 - 1, 25, 1}, {22644 - 1, 20 - 1, 25, 1}, {22566 - 1, 20 - 1, 25, 1},
    {22488 - 1, 20 - 1, 25, 1}, {22410 - 1, 20 - 1, 25, 1}, {22332 - 1, 20 - 1, 25, 1}, {22254 - 1, 20 - 1, 25, 1}, {22176 - 1, 20 - 1, 25, 1},
    {22098 - 1, 20 - 1, 25, 1}, {22020 - 1, 20 - 1, 25, 1}, {21942 - 1, 20 - 1, 25, 1}, {21864 - 1, 20 - 1, 25, 1}, {21786 - 1, 20 - 1, 25, 1},
    {21708 - 1, 20 - 1, 25, 1}, {21630 - 1, 20 - 1, 25, 1}, {21552 - 1, 20 - 1, 25, 1}, {21474 - 1, 20 - 1, 25, 1}, {21396 - 1, 20 - 1, 25, 1},
    {21318 - 1, 20 - 1, 25, 1}, {21240 - 1, 20 - 1, 25, 1}, {21162 - 1, 20 - 1, 25, 1}, {21084 - 1, 20 - 1, 25, 1}, {21006 - 1, 20 - 1, 25, 1},
    {20928 - 1, 20 - 1, 25, 1}, {20850 - 1, 20 - 1, 25, 1}, {20772 - 1, 20 - 1, 25, 1}, {20694 - 1, 20 - 1, 25, 1}, {20616 - 1, 20 - 1, 25, 1},
    {20538 - 1, 20 - 1, 25, 1}, {20460 - 1, 20 - 1, 25, 1}, {20382 - 1, 20 - 1, 25, 1}, {20304 - 1, 20 - 1, 25, 1}, {20226 - 1, 20 - 1, 25, 1},
    {20148 - 1, 20 - 1, 25, 1}, {20070 - 1, 20 - 1, 25, 1}, {19992 - 1, 20 - 1, 25, 1}, {19914 - 1, 20 - 1, 25, 1}, {19836 - 1, 20 - 1, 25, 1},
    {19758 - 1, 20 - 1, 25, 1}, {19680 - 1, 20 - 1, 25, 1}, {19602 - 1, 20 - 1, 25, 1}, {19524 - 1, 20 - 1, 25, 1}, {19446 - 1, 20 - 1, 25, 1},
    {19368 - 1, 20 - 1, 25, 1}, {19290 - 1, 20 - 1, 25, 1}, {19212 - 1, 20 - 1, 25, 1}, {19134 - 1, 20 - 1, 25, 1}, {19056 - 1, 20 - 1, 25, 1},
    {18978 - 1, 20 - 1, 25, 1}, {18900 - 1, 20 - 1, 25, 1}, {18822 - 1, 20 - 1, 25, 1}, {18744 - 1, 20 - 1, 25, 1}, {18666 - 1, 20 - 1, 25, 1},
    {18588 - 1, 20 - 1, 25, 1}, {18510 - 1, 20 - 1, 25, 1}, {18432 - 1, 20 - 1, 25, 1}, {18354 - 1, 20 - 1, 25, 1}, {18276 - 1, 20 - 1, 25, 1},
    {18198 - 1, 20 - 1, 25, 1}, {18120 - 1, 20 - 1, 25, 1}, {18042 - 1, 20 - 1, 25, 1}, {17964 - 1, 20 - 1, 25, 1}, {17886 - 1, 20 - 1, 25, 1},
    {17808 - 1, 20 - 1, 25, 1}, {17730 - 1, 20 - 1, 25, 1}, {17652 - 1, 20 - 1, 25, 1}, {17574 - 1, 20 - 1, 25, 1}, {17496 - 1, 20 - 1, 25, 1},
    {17418 - 1, 20 - 1, 25, 1}, {17340 - 1, 20 - 1, 25, 1}, {17262 - 1, 20 - 1, 25, 1}, {17184 - 1, 20 - 1, 25, 1}, {17106 - 1, 20 - 1, 25, 1},
    {17028 - 1, 20 - 1, 25, 1}, {16950 - 1, 20 - 1, 25, 1}, {16872 - 1, 20 - 1, 25, 1}, {16794 - 1, 20 - 1, 25, 1}, {16716 - 1, 20 - 1, 25, 1},
    {16638 - 1, 20 - 1, 25, 1}, {16560 - 1, 20 - 1, 25, 1}, {16482 - 1, 20 - 1, 25, 1}, {16404 - 1, 20 - 1, 25, 1}, {16326 - 1, 20 - 1, 25, 1},
    {16248 - 1, 20 - 1, 25, 1}, {16170 - 1, 20 - 1, 25, 1}, {16092 - 1, 20 - 1, 25, 1}, {16014 - 1, 20 - 1, 25, 1}, {15936 - 1, 20 - 1, 25, 1},
    {15858 - 1, 20 - 1, 25, 1}, {15780 - 1, 20 - 1, 25, 1}, {15702 - 1, 20 - 1, 25, 1}, {15624 - 1, 20 - 1, 25, 1}, {15546 - 1, 20 - 1, 25, 1},
    {15468 - 1, 20 - 1, 25, 1}, {15390 - 1, 20 - 1, 25, 1}, {15312 - 1, 20 - 1, 25, 1}, {15234 - 1, 20 - 1, 25, 1}, {15156 - 1, 20 - 1, 25, 1},
    {15078 - 1, 20 - 1, 25, 1},
};
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
 * @brief  启动DMA PWM输出
 * @param  None
 * @retval 启动结果
 */
HAL_StatusTypeDef PWM_Start_AW(void)
{
    HAL_StatusTypeDef status;

    m_drv8824_Index_Switch(eM_DRV8824_Index_1, portMAX_DELAY);
    gPWM_TEST_AW_CNT_Clear();
    __HAL_TIM_CLEAR_IT(&htim1, TIM_IT_UPDATE);
    status = HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    if (status != HAL_OK) {
        return status;
    }
    PWM_AW_IRQ_CallBcak();
    return status;
}

// /**
//  * @brief  HAL_TIM_PeriodElapsedCallback 中断回调处理
//  * @param  None
//  * @retval None
//  */
// void PWM_AW_IRQ_CallBcak(void)
// {
//     uint32_t cnt;
//     uint16_t length, idx = 0xFFFF;
//     static uint16_t i = 0, sum = 0;

//     cnt = gPWM_TEST_AW_CNT_Get(); /* 获取当前脉冲计数 */

//     for (i = i; i < ARRAY_LEN(cgPWM_AW_Confs); ++i) { /* 寻找下一个脉冲段配置 */
//         length = cgPWM_AW_Confs[i].rcr + 1;           /* 计算本区间脉冲数输出数目 */
//         if ((sum <= cnt) && (cnt < sum + length)) {   /* 落中区间 */
//             idx = i;                                  /* 弹出区间索引 */
//             break;
//         }
//         sum += length; /* 记录前区间脉冲总数 */
//     }

//     if (idx >= ARRAY_LEN(cgPWM_AW_Confs)) {        /* 停止输出 */
//         i = 0;                                     /* 清零索引 */
//         sum = 0;                                   /* 清零前区间脉冲总计数 */
//         HAL_TIM_Base_Stop(&htim1);                 /* 停止定时器 */
//         __HAL_TIM_CLEAR_IT(&htim1, TIM_IT_UPDATE); /* 清除更新事件标志位 */
//         __HAL_TIM_SET_COUNTER(&htim1, 0);          /* 清零定时器计数寄存器 */
//         HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);   /* 停止PWM输出 */
//         m_drv8824_release_ISR();                   /* 释放PWM资源 */
//     } else {
//         aSRC_Buffer[0] = cgPWM_AW_Confs[i].pcs; /* 周期长度 */
//         aSRC_Buffer[1] = cgPWM_AW_Confs[i].rcr; /* 重复次数 */
//         aSRC_Buffer[2] = cgPWM_AW_Confs[i].ccr; /* 翻转点 占空比 */
//         /* burst模式修改时基单元 */
//         HAL_TIM_DMABurst_WriteStart(&htim1, TIM_DMABASE_ARR, TIM_DMA_UPDATE, (uint32_t *)aSRC_Buffer, TIM_DMABURSTLENGTH_3TRANSFERS);
//     }
//     gPWM_TEST_AW_CNT_Inc(); /* 自增脉冲计数 */
// }

void PWM_AW_IRQ_CallBcak(void)
{
    uint32_t cnt;
    uint16_t idx;
    static uint16_t i = 0, sum = 0;

    cnt = gPWM_TEST_AW_CNT_Get(); /* 获取当前脉冲计数 */
    idx = 0xFFFF;

    for (i = i; i < ARRAY_LEN(cgPWM_AW_Confs); ++i) {              /* 寻找下一个脉冲段配置 */
        if ((sum <= cnt) && (cnt < sum + cgPWM_AW_Confs[i].dup)) { /* 落中区间 */
            idx = i;                                               /* 弹出区间索引 */
            break;
        }
        sum += cgPWM_AW_Confs[i].dup; /* 记录前区间脉冲总数 */
    }

    if (idx >= ARRAY_LEN(cgPWM_AW_Confs)) {        /* 停止输出 */
        i = 0;                                     /* 清零索引 */
        sum = 0;                                   /* 清零前区间脉冲总计数 */
        HAL_TIM_Base_Stop(&htim1);                 /* 停止定时器 */
        __HAL_TIM_CLEAR_IT(&htim1, TIM_IT_UPDATE); /* 清除更新事件标志位 */
        __HAL_TIM_SET_COUNTER(&htim1, 0);          /* 清零定时器计数寄存器 */
        HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);   /* 停止PWM输出 */
        m_drv8824_release_ISR();                   /* 释放PWM资源 */
    } else {
        aSRC_Buffer[0] = cgPWM_AW_Confs[idx].pcs;  /* 周期长度 */
        aSRC_Buffer[1] = cgPWM_AW_Confs[idx].rcr;  /* 重复次数 */
        aSRC_Buffer[2] = (aSRC_Buffer[0] + 1) / 2; // cgPWM_AW_Confs[idx].ccr; /* 翻转点 占空比 */
        /* burst模式修改时基单元 */
        HAL_TIM_DMABurst_WriteStart(&htim1, TIM_DMABASE_ARR, TIM_DMA_UPDATE, (uint32_t *)aSRC_Buffer, TIM_DMABURSTLENGTH_3TRANSFERS);
    }
    gPWM_TEST_AW_CNT_Inc(); /* 自增脉冲计数 */
}

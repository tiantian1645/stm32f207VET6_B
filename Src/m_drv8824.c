/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "motor.h"
#include "m_drv8824.h"

/* Extern variables ----------------------------------------------------------*/
extern TIM_HandleTypeDef htim1;

/* Private includes ----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/
typedef struct {
    uint32_t pcs;
    uint32_t rcr;
    uint32_t ccr;
} sPWM_AW_Conf;

/* Private variables ---------------------------------------------------------*/
static eM_DRV8824_Index gMDRV8824Index = eM_DRV8824_Index_0;
static uint32_t aSRC_Buffer[3] = {0, 0, 0};
static uint32_t gPWM_TEST_AW_CNT = 0;
static SemaphoreHandle_t m_drv8824_spi_sem = NULL;
const sPWM_AW_Conf cgPWM_AW_Confs[] = {{576 - 1, 25 - 1, 288}, {144 - 1, 100 - 1, 72}};

/* Private function prototypes -----------------------------------------------*/
static uint8_t m_drv8824_acquire(uint32_t timeout);

/* Private user code ---------------------------------------------------------*/

/**
 * @brief  PWM资源锁初始化
 * @param  None
 * @retval None
 */
void m_drv8824_Init(void)
{
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

    //m_drv8824_Index_Switch(eM_DRV8824_Index_0, portMAX_DELAY);

    gPWM_TEST_AW_CNT_Clear();
    __HAL_TIM_CLEAR_IT(&htim1, TIM_IT_UPDATE);
    status = HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    if (status != HAL_OK) {
        return status;
    }
    PWM_AW_IRQ_CallBcak();
    return status;
}

/**
 * @brief  HAL_TIM_PeriodElapsedCallback 中断回调处理
 * @param  None
 * @retval None
 */
void PWM_AW_IRQ_CallBcak(void)
{
    uint32_t cnt;
    uint16_t length, idx = 0xFFFF;
    static uint16_t i = 0, sum = 0;

    cnt = gPWM_TEST_AW_CNT_Get(); /* 获取当前脉冲计数 */

    for (i = i; i < ARRAY_LEN(cgPWM_AW_Confs); ++i) { /* 寻找下一个脉冲段配置 */
        length = cgPWM_AW_Confs[i].rcr + 1;           /* 计算本区间脉冲数输出数目 */
        if ((sum <= cnt) && (cnt < sum + length)) {   /* 落中区间 */
            idx = i;                                  /* 弹出区间索引 */
            break;
        }
        sum += length; /* 记录前区间脉冲总数 */
    }

    if (idx >= ARRAY_LEN(cgPWM_AW_Confs)) {        /* 停止输出 */
        i = 0;                                     /* 清零索引 */
        sum = 0;                                   /* 清零前区间脉冲总计数 */
//        HAL_TIM_Base_Stop(&htim1);                 /* 停止定时器 */
//        __HAL_TIM_CLEAR_IT(&htim1, TIM_IT_UPDATE); /* 清除更新事件标志位 */
//        __HAL_TIM_SET_COUNTER(&htim1, 0);          /* 清零定时器计数寄存器 */
        HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);   /* 停止PWM输出 */
        m_drv8824_release_ISR();                   /* 释放PWM资源 */
    } else {
        aSRC_Buffer[0] = cgPWM_AW_Confs[i].pcs; /* 周期长度 */
        aSRC_Buffer[1] = cgPWM_AW_Confs[i].rcr; /* 重复次数 */
        aSRC_Buffer[2] = cgPWM_AW_Confs[i].ccr; /* 翻转点 占空比 */
        /* burst模式修改时基单元 */
        HAL_TIM_DMABurst_WriteStart(&htim1, TIM_DMABASE_ARR, TIM_DMA_UPDATE, (uint32_t *)aSRC_Buffer, TIM_DMABURSTLENGTH_3TRANSFERS);
    }
    gPWM_TEST_AW_CNT_Inc(); /* 自增脉冲计数 */
}

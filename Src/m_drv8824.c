/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "motor.h"
#include "m_drv8824.h"
#include "heat_motor.h"
#include "white_motor.h"

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
    uint16_t dup;
} sPWM_AW_Conf;

/* Private variables ---------------------------------------------------------*/
static eM_DRV8824_Index gMDRV8824Index = eM_DRV8824_Index_0;
static uint32_t gPWM_TEST_AW_CNT = 0;
static SemaphoreHandle_t m_drv8824_spi_sem = NULL;

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
    white_Motor_PD();
    white_Motor_WH();
    heat_Motor_Up();
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
    m_drv8824_Deactive_All();
    if (xSemaphoreGiveFromISR(m_drv8824_spi_sem, NULL) == pdPASS) {
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
 * @brief  清理故障情况
 * @param  index       索引值
 * @note   index 参考 eM_DRV8824_Index 顺手切换使能管脚
 * @retval 0 无故障 1 有故障但重置后消除 2 重置后故障仍存在
 */
uint8_t m_drv8824_Clear_Flag(void)
{
    switch (gMDRV8824Index) {
        case eM_DRV8824_Index_0:
            if (m_drv8824_Get_Flag()) {
                HAL_GPIO_WritePin(STEP_NRST_GPIO_Port, STEP_NRST_Pin, GPIO_PIN_RESET);
                HAL_Delay(1);
                HAL_GPIO_WritePin(STEP_NRST_GPIO_Port, STEP_NRST_Pin, GPIO_PIN_SET);
                HAL_Delay(1);
                if (m_drv8824_Get_Flag()) {
                    return 2;
                }
                return 1;
            }
            return 0;
            break;
        case eM_DRV8824_Index_1:
            if (m_drv8824_Get_Flag()) {
                HAL_GPIO_WritePin(STEP_NRST_GPIO_Port, STEP_NRST_Pin, GPIO_PIN_RESET);
                HAL_Delay(1);
                HAL_GPIO_WritePin(STEP_NRST_GPIO_Port, STEP_NRST_Pin, GPIO_PIN_SET);
                HAL_Delay(1);
                if (m_drv8824_Get_Flag()) {
                    return 2;
                }
                return 1;
            }
            return 0;
        default:
            break;
    }
    return 3;
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
 * @brief  停止PWM输出
 * @param  None
 * @retval None
 */
void PWM_AW_Stop(void)
{
    HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1); /* 停止PWM输出 */
    reset_Tim1();
}

/**
 * @brief  PWM输出回调
 * @param  None
 * @retval 0 输出完成 1 输出未完成
 */
uint8_t PWM_AW_IRQ_CallBcak(void)
{
    switch (gMDRV8824Index) {
        case eM_DRV8824_Index_0: /* 白板电机 */
            switch (gWhite_Motor_Dir_Get()) {
                case eMotorDir_FWD:
                    return white_Motor_PWM_Gen_Out();
                case eMotorDir_REV:
                default:
                    return white_Motor_PWM_Gen_In();
            }
            break;
        case eM_DRV8824_Index_1: /* 上加热体电机 */
        default:
            switch (gHeat_Motor_Dir_Get()) {
                case eMotorDir_FWD:
                    return heat_Motor_PWM_Gen_Up();
                case eMotorDir_REV:
                default:
                    return heat_Motor_PWM_Gen_Down();
            }
            break;
    }
}

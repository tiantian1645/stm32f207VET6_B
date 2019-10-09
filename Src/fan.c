/**
 * @file    fan.c
 * @brief   风扇控制
 *
 * 定时器8 通道1 PWM输出
 * 定时器10 通道1 输入捕捉
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fan.h"

/* Extern variables ----------------------------------------------------------*/
extern TIM_HandleTypeDef htim8;
extern TIM_HandleTypeDef htim10;

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/
#define FAN_PWM_TIM htim8
#define FAN_PWM_TIM_CHANNEL TIM_CHANNEL_1

#define FAN_FB_TIM htim10
#define FAN_FB_TIM_CHANNEL TIM_CHANNEL_1

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Private constants ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/* Private user code ---------------------------------------------------------*/

/**
 * @brief  风扇 启动
 * @param  None
 * @retval None
 */
void fan_Start(void)
{
    HAL_GPIO_WritePin(FAN_EN_GPIO_Port, FAN_EN_Pin, GPIO_PIN_SET); /* 操作使能管脚 */
    HAL_TIM_PWM_Start(&FAN_PWM_TIM, FAN_PWM_TIM_CHANNEL);          /* 操作PWM输出管脚 */
}

/**
 * @brief  风扇 停止
 * @param  None
 * @retval None
 */
void fan_Stop(void)
{
    HAL_GPIO_WritePin(FAN_EN_GPIO_Port, FAN_EN_Pin, GPIO_PIN_RESET); /* 操作使能管脚 */
    HAL_TIM_PWM_Stop(&FAN_PWM_TIM, FAN_PWM_TIM_CHANNEL);             /* 操作PWM输出管脚 */
}

/**
 * @brief  风扇 调速
 * @param  rate PWM 占空比调节电压 范围 0.0 ～ 1.0
 * @retval None
 */
void fan_Adjust(float rate)
{
    uint16_t ccr;

    if (rate < 0) {
        rate = 0;
    }
    ccr = (uint16_t)(rate * (FAN_ARR)) % (FAN_ARR + 1);
    __HAL_TIM_SET_COMPARE(&FAN_PWM_TIM, FAN_PWM_TIM_CHANNEL, ccr);
}

/**
 * @file    heater.c
 * @brief   上下加热体控制控制
 *
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "temperature.h"
#include "heater.h"
#include "pid_ctrl.h"

/* Extern variables ----------------------------------------------------------*/
extern TIM_HandleTypeDef htim4;
extern TIM_HandleTypeDef htim3;

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/
#define HEATER_BTM_SAMPLE 10
#define HEATER_TOP_SAMPLE 10

/* Private variables ---------------------------------------------------------*/
static sPID_Ctrl_Conf gHeater_BTM_PID_Conf;
static sPID_Ctrl_Conf gHeater_TOP_PID_Conf;

// Control loop input,output and setpoint variables
static float btm_input = 0, btm_output = 0, btm_setpoint = 37;
static float top_input = 0, top_output = 0, top_setpoint = 37;

/* Private constants ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/* Private user code ---------------------------------------------------------*/

/**
 * @brief  下加热体 PWM 输出占空比调整
 * @note   范围 0 ~ 100
 * @param  pr 浮点数形式占空比
 * @retval None
 */
void beater_BTM_Output_Ctl(float pr)
{
    uint16_t ccr;

    ccr = ((uint16_t)(pr * HEATER_BTM_ARR)) % (HEATER_BTM_ARR + 1);
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, ccr);
}

/**
 * @brief  下加热体 PWM 输出 启动
 * @param  None
 * @retval None
 */

void beater_BTM_Output_Start(void)
{
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_4);
}

/**
 * @brief  下加热体 PWM 输出 停止
 * @param  None
 * @retval None
 */
void beater_BTM_Output_Stop(void)
{
    HAL_TIM_PWM_Stop(&htim4, TIM_CHANNEL_4);
}

/**
 * @brief  下加热体 PWM 输出 状态获取
 * @param  None
 * @retval PWM 输出 状态
 */
uint8_t beater_BTM_Output_Is_Live(void)
{
    return htim4.Instance->CR1 == TIM_CR1_CEN;
}

/**
 * @brief  下加热体 PWM 输出 初始化
 * @param  None
 * @retval None
 */
void heater_BTM_Output_Init(void)
{
    // Prepare PID controller for operation
    pid_ctrl_init(&gHeater_BTM_PID_Conf, HEATER_BTM_SAMPLE, &btm_input, &btm_output, &btm_setpoint, 6000, 36000, 200);
    // Set controler output limits from 0 to 200
    pid_ctrl_limits(&gHeater_BTM_PID_Conf, 0, 100);
    // Allow PID to compute and change output
    pid_ctrl_auto(&gHeater_BTM_PID_Conf);
}

/**
 * @brief  下加热体 PWM 输出 PID 调整
 * @param  None
 * @retval None
 */
void heater_BTM_Output_Keep_Deal(void)
{
    // Check if need to compute PID
    if (pid_ctrl_need_compute(&gHeater_BTM_PID_Conf)) {
        // Read process feedback
        btm_input = temp_Get_Temp_Data_BTM();
        // Compute new PID output value
        pid_ctrl_compute(&gHeater_BTM_PID_Conf);
        // Change actuator value
        beater_BTM_Output_Ctl(btm_output / gHeater_BTM_PID_Conf.omax);
    }
}

void heater_BTM_Log_PID(void)
{
    pid_ctrl_log_d("BTM", &gHeater_BTM_PID_Conf);
}

/**
 * @brief  上加热体 PWM 输出占空比调整
 * @note   范围 0 ~ 100
 * @param  pr 浮点数形式占空比
 * @retval None
 */
void beater_TOP_Output_Ctl(float pr)
{
    uint16_t ccr;

    ccr = ((uint16_t)(pr * HEATER_TOP_ARR)) % (HEATER_TOP_ARR + 1);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, ccr);
}

/**
 * @brief  上加热体 PWM 输出 启动
 * @param  None
 * @retval None
 */

void beater_TOP_Output_Start(void)
{
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
}

/**
 * @brief  上加热体 PWM 输出 停止
 * @param  None
 * @retval None
 */
void beater_TOP_Output_Stop(void)
{
    HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_3);
}

/**
 * @brief  上加热体 PWM 输出 状态获取
 * @param  None
 * @retval PWM 输出 状态
 */
HAL_TIM_StateTypeDef beater_TOP_Output_Is_Live(void)
{
    return htim3.Instance->CR1 == TIM_CR1_CEN;
}

/**
 * @brief  上加热体 PWM 输出 初始化
 * @param  None
 * @retval None
 */
void heater_TOP_Output_Init(void)
{
    // Prepare PID controller for operation
    pid_ctrl_init(&gHeater_TOP_PID_Conf, HEATER_TOP_SAMPLE, &top_input, &top_output, &top_setpoint, 5000, 40000, 50); /* 37摄氏度 kp 7650 临界波动点 */
    // Set controler output limits from 0 to 200
    pid_ctrl_limits(&gHeater_TOP_PID_Conf, 0, 100);
    // Allow PID to compute and change output
    pid_ctrl_auto(&gHeater_TOP_PID_Conf);
}

/**
 * @brief  上加热体 PWM 输出 PID 调整
 * @param  None
 * @retval None
 */
void heater_TOP_Output_Keep_Deal(void)
{
    // Check if need to compute PID
    if (pid_ctrl_need_compute(&gHeater_TOP_PID_Conf)) {
        // Read process feedback
        top_input = temp_Get_Temp_Data_TOP();
        // Compute new PID output value
        pid_ctrl_compute(&gHeater_TOP_PID_Conf);
        // Change actuator value
        beater_TOP_Output_Ctl(top_output / gHeater_TOP_PID_Conf.omax);
    }
}

void heater_TOP_Log_PID(void)
{
    pid_ctrl_log_d("TOP", &gHeater_TOP_PID_Conf);
}

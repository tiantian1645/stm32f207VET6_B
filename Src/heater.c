/**
 * @file    heater.c
 * @brief   上下加热体控制控制
 *
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "temperature.h"
#include "heater.h"
#include "pid.h"

/* Extern variables ----------------------------------------------------------*/
extern TIM_HandleTypeDef htim4;

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
static struct pid_controller ctrldata;
static spid_t pid;
// Control loop input,output and setpoint variables
static float input = 0, output = 0;
static float setpoint = 37;

/* Private constants ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/* Private user code ---------------------------------------------------------*/

/**
 * @brief  下加热体 PWM 输出占空比调整
 * @note   范围 0 ~ HEATER_BTM_CCR
 * @param  pr 浮点数形式占空比
 * @retval None
 */
void beater_BTM_Output_Ctl(float pr)
{
    uint16_t ccr;
    ccr = pr * HEATER_BTM_ARR;
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, ccr);
}

/**
 * @brief  下加热体 PWM 输出 启动
 * @param  pluse 翻转点距离
 * @retval None
 */

void beater_BTM_Output_Start(void)
{
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_4);
}

/**
 * @brief  下加热体 PWM 输出 停止
 * @param  pluse 翻转点距离
 * @retval None
 */
void beater_BTM_Output_Stop(void)
{
    HAL_TIM_PWM_Stop(&htim4, TIM_CHANNEL_4);
}

/**
 * @brief  下加热体 PWM 输出 初始化
 * @param  None
 * @retval None
 */
void heater_BTM_Output_Init(void)
{
    // Control loop gains
    float kp = 2.5, ki = 1.0, kd = 1.0;

    // Prepare PID controller for operation
    pid = pid_create(&ctrldata, &input, &output, &setpoint, kp, ki, kd);
    // Set controler output limits from 0 to 200
    pid_limits(pid, 5, 100);
    // Allow PID to compute and change output
    pid_auto(pid);
}

/**
 * @brief  下加热体 PWM 输出 PID 调整
 * @param  None
 * @retval None
 */
void heater_BTM_Output_Keep_Deal(void)
{
    // Check if need to compute PID
    if (pid_need_compute(pid)) {
        // Read process feedback
        input = temp_Get_Temp_Data_BTM();
        // Compute new PID output value
        pid_compute(pid);
        // Change actuator value
        beater_BTM_Output_Ctl(output);
    }
}

void heater_BTM_Log_PID(void)
{
    pid_log_d(pid);
}
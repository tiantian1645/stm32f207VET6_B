/**
 * @file    fan.c
 * @brief   风扇控制
 * @note    https://controllerstech.com/how-to-use-input-capture-in-stm32/
 * @note    PWM 最小出力 0.1
 *
 * 定时器8 通道1 PWM输出
 * 定时器10 通道1 输入捕捉
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fan.h"
#include "temperature.h"

/* Extern variables ----------------------------------------------------------*/
extern TIM_HandleTypeDef htim8;
extern TIM_HandleTypeDef htim10;

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/
typedef enum {
    eFan_Work_State_CtrlDeal,
    eFan_Work_State_SelfTest,
} eFan_Work_State;

/* Private define ------------------------------------------------------------*/
#define FAN_PWM_TIM htim8
#define FAN_PWM_TIM_CHANNEL TIM_CHANNEL_1

#define FAN_FB_TIM htim10
#define FAN_FB_TIM_CHANNEL TIM_CHANNEL_1

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
static uint8_t gFan_TIM_IC_Enter_Flag = 0, gFan_IC_Error_Report_Flag = 0;
static uint32_t gFan_IC_Value_Enter = 0, gFan_IC_Value_Leave = 0, gFan_IC_Diff = 0, gFan_IC_Freq = 0;
static float gFan_Setting_Rate = 0;
static eFan_Work_State gFan_Work_state = eFan_Work_State_CtrlDeal;

/* Private constants ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
static void fan_IC_Freq_Clr(void);

static void fan_IC_Start(void);
static void fan_IC_Stop(void);

static void fan_Start(void);
static void fan_Stop(void);
static void fan_Adjust(float rate);

static uint8_t fan_Is_Running(void);

/* Private user code ---------------------------------------------------------*/
/**
 * @brief  风扇 测量频率获取
 * @param  None
 * @retval None
 */
uint32_t fan_IC_Freq_Get(void)
{
    return gFan_IC_Freq;
}

/**
 * @brief  风扇 测量频率清零
 * @param  None
 * @retval None
 */
static void fan_IC_Freq_Clr(void)
{
    gFan_IC_Freq = 0;
}

/**
 * @brief  风扇 告警报告 使能
 * @param  None
 * @retval None
 */
void fan_IC_Error_Report_Enable(void)
{
    gFan_IC_Error_Report_Flag = 1;
    fan_IC_Start(); /* 启动输入捕获定时器 */
}

/**
 * @brief  风扇 告警报告 禁用
 * @param  None
 * @retval None
 */
void fan_IC_Error_Report_Disable(void)
{
    gFan_IC_Error_Report_Flag = 0;
    fan_IC_Stop(); /* 停止输入捕获定时器 */
}

/**
 * @brief  风扇 初始化
 * @param  None
 * @retval None
 */
void fan_Init(void)
{
    fan_Start();                                /* 启动风扇PWM输出 */
    fan_Adjust(0.1);                            /* 调整PWM占空比 */
    fan_IC_Error_Report_Enable();               /* 使能告警 */
    gFan_Work_state = eFan_Work_State_CtrlDeal; /* 初始化工作状态 */
}

/**
 * @brief  风扇 输入捕获定时器 启动
 * @param  None
 * @retval None
 */
static void fan_IC_Start(void)
{
    HAL_TIM_IC_Start_IT(&FAN_FB_TIM, FAN_FB_TIM_CHANNEL); /* 开启输入捕捉中断 */
}

/**
 * @brief  风扇 输入捕获定时器 停止
 * @param  None
 * @retval None
 */
static void fan_IC_Stop(void)
{
    HAL_TIM_IC_Stop_IT(&FAN_FB_TIM, FAN_FB_TIM_CHANNEL); /* 开启输入捕捉中断 */
}

/**
 * @brief  风扇 启动
 * @param  None
 * @retval None
 */
static void fan_Start(void)
{
    HAL_GPIO_WritePin(FAN_EN_GPIO_Port, FAN_EN_Pin, GPIO_PIN_SET); /* 操作使能管脚 */
    HAL_TIM_PWM_Start(&FAN_PWM_TIM, FAN_PWM_TIM_CHANNEL);          /* 操作PWM输出管脚 */
}

/**
 * @brief  风扇 停止
 * @param  None
 * @retval None
 */
static void fan_Stop(void)
{
    gFan_Setting_Rate = 0;
    HAL_GPIO_WritePin(FAN_EN_GPIO_Port, FAN_EN_Pin, GPIO_PIN_RESET); /* 操作使能管脚 */
    HAL_TIM_PWM_Stop(&FAN_PWM_TIM, FAN_PWM_TIM_CHANNEL);             /* 操作PWM输出管脚 */
}

/**
 * @brief  风扇 调速
 * @param  rate PWM 占空比调节电压 范围 0.0 ～ 1.0
 * @retval None
 */
static void fan_Adjust(float rate)
{
    uint16_t ccr;

    if (rate < 0) {
        rate = 0;
    } else if (rate > 1) {
        rate = 1;
    }
    gFan_Setting_Rate = rate;

    ccr = (uint16_t)(rate * (FAN_ARR)) % (FAN_ARR + 1);
    __HAL_TIM_SET_COMPARE(&FAN_PWM_TIM, FAN_PWM_TIM_CHANNEL, ccr);
}

/**
 * @brief  风扇是否处于工作中
 * @note   风扇使能管脚状态判断
 * @param  None
 * @retval 1 工作中 0 停机
 */
static uint8_t fan_Is_Running(void)
{
    return (HAL_GPIO_ReadPin(FAN_EN_GPIO_Port, FAN_EN_Pin) == GPIO_PIN_SET);
}

/**
 * @brief  风扇风速控制处理
 * @param  temp_env 环境温度
 * @note   低于27度保持低速 高于30度全速 27～30之间线性调整转速
 * @note   当前环境温度探头处温度为箱内温度
 * @retval None
 */
void fan_Ctrl_Deal(float temp_env)
{
    uint8_t is_running;

    if (gFan_Work_state == eFan_Work_State_SelfTest) {
        return;
    }

    is_running = fan_Is_Running(); /* 风扇是否工作中 */

    if (temp_env < 27 || temp_env == TEMP_INVALID_DATA) {
        if (is_running) {
            fan_Stop();
        }
    } else if (temp_env > 30) {
        if (is_running == 0) {
            fan_Start();
        }
        fan_Adjust(1.0);
    } else {
        if (is_running == 0) {
            fan_Start();
        }
        fan_Adjust(0.3 * temp_env - 8);
    }
}

/**
 * @brief  进入自检状态
 * @param  None
 * @retval None
 */
void fan_Enter_Self_Test(void)
{
    gFan_Work_state = eFan_Work_State_SelfTest;

    fan_IC_Freq_Clr();
    fan_IC_Start();

    if (fan_Is_Running() == 0) {
        fan_Start();
    }
    fan_Adjust(1.0);
}

/**
 * @brief  退出自检状态
 * @param  None
 * @retval None
 */
void fan_Leave_Self_Test(void)
{
    gFan_Work_state = eFan_Work_State_CtrlDeal;
}

/**
 * @brief  风扇 转速过低处理
 * @note   停机前最低转速 1600 左右
 * @param  None
 * @retval None
 */
void fan_IC_Error_Deal(void)
{
    uint8_t is_running;
    static uint32_t tick = 0;

    if (gFan_IC_Error_Report_Flag == 0) {
        tick = HAL_GetTick();
    }

    if (gFan_Setting_Rate == 0) {
        return;
    }

    is_running = fan_Is_Running(); /* 风扇是否工作中 */
    if (is_running == 0) {         /* 停机时不判断 */
        return;
    }

    if (fan_IC_Freq_Get() < 1200) {
        if (HAL_GetTick() - tick > 300 * 1000) { /* 5分钟上报一次 */
            tick = HAL_GetTick();
            error_Emit(eError_Fan_Lost_Speed);
        }
    } else {
        tick = HAL_GetTick();
    }
    fan_IC_Freq_Clr();
}

/**
 * @brief  风扇 输入捕捉回调
 * @note   中断优先级15 最低 切勿调用系统API
 * @param  htim 定时器句柄
 * @retval None
 */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef * htim)
{
    if (htim->Instance == TIM10) {
        switch (gFan_TIM_IC_Enter_Flag) {
            case 0:
                gFan_IC_Value_Enter = HAL_TIM_ReadCapturedValue(htim, FAN_FB_TIM_CHANNEL);
                gFan_TIM_IC_Enter_Flag = 1;
                break;
            default:
                gFan_IC_Value_Leave = HAL_TIM_ReadCapturedValue(htim, FAN_FB_TIM_CHANNEL);
                if (gFan_IC_Value_Enter > gFan_IC_Value_Leave) {
                    gFan_IC_Diff = gFan_IC_Value_Leave + (FAN_IC_ARR - gFan_IC_Value_Enter) + 1;
                } else {
                    gFan_IC_Diff = gFan_IC_Value_Leave - gFan_IC_Value_Enter;
                }
                gFan_IC_Freq = HAL_RCC_GetPCLK2Freq() * 2 / (FAN_IC_PSC + 1) * (FAN_IC_IF + 1) / gFan_IC_Diff;
                gFan_TIM_IC_Enter_Flag = 0;
                break;
        }
    }
}

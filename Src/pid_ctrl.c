/**
 * @file    pid_ctl.c
 * @brief   上下加热体控制控制
 * @note    https://github.com/alvinyeats/pid-control/tree/master/CHAPTER1
 * @note    https://github.com/alvinyeats/pid-control/blob/master/CHAPTER1/chap1_4.c
 * @note    https://github.com/geekfactory/PID
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "pid_ctrl.h"
#include <stdio.h>

/* Extern variables ----------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/
#define TICK_SECOND configTICK_RATE_HZ /* 系统时钟频率 */
#define tick_get HAL_GetTick     /* 系统时钟获取函数 */

/* Private variables ---------------------------------------------------------*/

/* Private constants ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/* Private user code ---------------------------------------------------------*/
/**
 * @brief  创建 PID 配置记录变量
 * @param  pid PID 变量结构体指针
 * @param  duration PID 计算间隔 影响 倍率系数换算 单位 mS
 * @param  in 输入变量地址
 * @param  out 输出变量地址
 * @param  set 目标点
 * @param  kp 差值倍率 1秒为基准
 * @param  ki 积分倍率 1秒为基准
 * @param  kd 微分倍率 1秒为基准
 * @retval PID 变量结构体指针
 */
void pid_ctrl_init(sPID_Ctrl_Conf * pPID_Info, uint32_t duration, float * in, float * out, float * set, float kp, float ki, float kd)
{
    pPID_Info->input = in;
    pPID_Info->output = out;
    pPID_Info->setpoint = set;
    pPID_Info->automode = false;

    pid_ctrl_limits(pPID_Info, 0, 100);

    // Set default sample time to 100 ms
    pPID_Info->sampletime = duration * (TICK_SECOND / 1000);

    pid_ctrl_direction(pPID_Info, E_PID_DIRECT); /* 方向设置 */
    pid_ctrl_tune(pPID_Info, kp, ki, kd);        /* 倍率换算为以一秒采样间隔倍率 */

    pPID_Info->lasttime = tick_get() - pPID_Info->sampletime;
}

/**
 * @brief  判断是否需要进行 PID 计算
 * @param  pid PID 变量结构体指针
 * @retval 1 需要计算 0 不需要计算
 */
bool pid_ctrl_need_compute(sPID_Ctrl_Conf * pPID_Info)
{
    // Check if the PID period has elapsed
    return (tick_get() - pPID_Info->lasttime >= pPID_Info->sampletime) ? true : false;
}

/**
 * @brief  PID 计算
 * @param  pid PID 变量结构体指针
 * @retval None
 */
void pid_ctrl_compute(sPID_Ctrl_Conf * pPID_Info)
{
    // Check if control is enabled
    if (!pPID_Info->automode)
        return;

    float in = *(pPID_Info->input);
    // Compute error
    float error = (*(pPID_Info->setpoint)) - in;
    // Compute integral
    if (pPID_Info->Ki == 0)
        pPID_Info->iterm = 0;
    else {
        pPID_Info->iterm += (pPID_Info->Ki * error);
        if (pPID_Info->iterm > pPID_Info->omax)
            pPID_Info->iterm = pPID_Info->omax;
        else if (pPID_Info->iterm < pPID_Info->omin)
            pPID_Info->iterm = pPID_Info->omin;
    }
    // Compute differential on input
    float dinput = in - pPID_Info->lastin;
    // Compute PID output
    float out = pPID_Info->Kp * error + pPID_Info->iterm - pPID_Info->Kd * dinput;
    // Apply limit to output value
    if (out > pPID_Info->omax)
        out = pPID_Info->omax;
    else if (out < pPID_Info->omin)
        out = pPID_Info->omin;
    // Output to pointed variable
    (*pPID_Info->output) = out;
    // Keep track of some variables for next execution
    pPID_Info->lastin = in;
    pPID_Info->lasttime = tick_get();
}

/**
 * @brief  PID 倍率变量换算 以一秒为基准
 * @param  pid PID 变量结构体指针
 * @param  kp 差值倍率
 * @param  ki 积分倍率
 * @param  kd 微分倍率
 * @retval None
 */
void pid_ctrl_tune(sPID_Ctrl_Conf * pPID_Info, float kp, float ki, float kd)
{
    // Check for validity
    if (kp < 0 || ki < 0 || kd < 0)
        return;

    // Compute sample time in seconds
    float ssec = ((float)pPID_Info->sampletime) / ((float)TICK_SECOND);

    pPID_Info->Kp = kp;
    pPID_Info->Ki = ki * ssec;
    pPID_Info->Kd = kd / ssec;

    if (pPID_Info->direction == E_PID_REVERSE) {
        pPID_Info->Kp = 0 - pPID_Info->Kp;
        pPID_Info->Ki = 0 - pPID_Info->Ki;
        pPID_Info->Kd = 0 - pPID_Info->Kd;
    }
}

/**
 * @brief  PID 倍率变量换算
 * @param  pid PID 变量结构体指针
 * @param  time 采样间隔
 * @retval None
 */
void pid_ctrl_sample(sPID_Ctrl_Conf * pPID_Info, uint32_t time)
{
    if (time > 0) {
        float ratio = (float)(time * (TICK_SECOND / 1000)) / (float)pPID_Info->sampletime;
        pPID_Info->Ki *= ratio;
        pPID_Info->Kd /= ratio;
        pPID_Info->sampletime = time * (TICK_SECOND / 1000);
    }
}

/**
 * @brief  PID 输出限制
 * @param  pid PID 变量结构体指针
 * @param  min 输出最大值
 * @param  max 输出最小值
 * @param  time
 * @retval None
 */
void pid_ctrl_limits(sPID_Ctrl_Conf * pPID_Info, float min, float max)
{
    if (min >= max) {
        pPID_Info->omin = max;
        pPID_Info->omax = min;
    } else {
        pPID_Info->omin = min;
        pPID_Info->omax = max;
    }
    // Adjust output to new limits
    if (pPID_Info->automode) {
        if (*(pPID_Info->output) > pPID_Info->omax) {
            *(pPID_Info->output) = pPID_Info->omax;
        } else if (*(pPID_Info->output) < pPID_Info->omin) {
            *(pPID_Info->output) = pPID_Info->omin;
        }

        if (pPID_Info->iterm > pPID_Info->omax) {
            pPID_Info->iterm = pPID_Info->omax;
        } else if (pPID_Info->iterm < pPID_Info->omin) {
            pPID_Info->iterm = pPID_Info->omin;
        }
    }
}

/**
 * @brief  PID 使能自动计算标志
 * @param  pid PID 变量结构体指针
 * @retval None
 */
void pid_ctrl_auto(sPID_Ctrl_Conf * pPID_Info)
{
    // If going from manual to auto
    if (!pPID_Info->automode) {
        pPID_Info->iterm = *(pPID_Info->output);
        pPID_Info->lastin = *(pPID_Info->input);
        if (pPID_Info->iterm > pPID_Info->omax) {
            pPID_Info->iterm = pPID_Info->omax;
        } else if (pPID_Info->iterm < pPID_Info->omin) {
            pPID_Info->iterm = pPID_Info->omin;
        }
        pPID_Info->automode = true;
    }
}

/**
 * @brief  PID 失能自动计算标志
 * @param  pid PID 变量结构体指针
 * @retval None
 */
void pid_ctrl_manual(sPID_Ctrl_Conf * pPID_Info)
{
    pPID_Info->automode = false;
}

/**
 * @brief  PID 计算方向设置
 * @param  pid PID 变量结构体指针
 * @param  dir PID 计算方向
 * @retval None
 */
void pid_ctrl_direction(sPID_Ctrl_Conf * pPID_Info, ePID_Ctrl_Dir dir)
{
    if (pPID_Info->automode && pPID_Info->direction != dir) {
        pPID_Info->Kp = (0 - pPID_Info->Kp);
        pPID_Info->Ki = (0 - pPID_Info->Ki);
        pPID_Info->Kd = (0 - pPID_Info->Kd);
    }
    pPID_Info->direction = dir;
}

/**
 * @file    heater.c
 * @brief   上下加热体控制控制
 *
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "pid.h"
#include <stdio.h>

/* Extern variables ----------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/
#define TICK_SECOND configTICK_RATE_HZ /* 系统时钟频率 */
#define tick_get xTaskGetTickCount     /* 系统时钟获取函数 */

/* Private variables ---------------------------------------------------------*/

/* Private constants ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/* Private user code ---------------------------------------------------------*/
/**
 * @brief  创建 PID 配置记录变量
 * @param  pid PID 变量结构体指针
 * @param  in 输入变量地址
 * @param  out 输出变量地址
 * @param  set 目标点
 * @param  kp 差值倍率
 * @param  ki 积分倍率
 * @param  kd 微分倍率
 * @retval PID 变量结构体指针
 */
spid_t pid_create(spid_t pid, float * in, float * out, float * set, float kp, float ki, float kd)
{
    pid->input = in;
    pid->output = out;
    pid->setpoint = set;
    pid->automode = false;

    pid_limits(pid, 0, 255);

    // Set default sample time to 100 ms
    pid->sampletime = 100 * (TICK_SECOND / 1000);

    pid_direction(pid, E_PID_DIRECT);
    spid_tune(pid, kp, ki, kd);

    pid->lasttime = tick_get() - pid->sampletime;

    return pid;
}

/**
 * @brief  判断是否需要进行 PID 计算
 * @param  pid PID 变量结构体指针
 * @retval 1 需要计算 0 不需要计算
 */
bool pid_need_compute(spid_t pid)
{
    // Check if the PID period has elapsed
    return (tick_get() - pid->lasttime >= pid->sampletime) ? true : false;
}

/**
 * @brief  PID 计算
 * @param  pid PID 变量结构体指针
 * @retval None
 */
void pid_compute(spid_t pid)
{
    // Check if control is enabled
    if (!pid->automode)
        return;

    float in = *(pid->input);
    // Compute error
    float error = (*(pid->setpoint)) - in;
    // Compute integral
    pid->iterm += (pid->Ki * error);
    if (pid->iterm > pid->omax)
        pid->iterm = pid->omax;
    else if (pid->iterm < pid->omin)
        pid->iterm = pid->omin;
    // Compute differential on input
    float dinput = in - pid->lastin;
    // Compute PID output
    float out = pid->Kp * error + pid->iterm - pid->Kd * dinput;
    // Apply limit to output value
    if (out > pid->omax)
        out = pid->omax;
    else if (out < pid->omin)
        out = pid->omin;
    // Output to pointed variable
    (*pid->output) = out;
    // Keep track of some variables for next execution
    pid->lastin = in;
    pid->lasttime = tick_get();
}

/**
 * @brief  PID 倍率变量换算
 * @param  pid PID 变量结构体指针
 * @param  kp 差值倍率
 * @param  ki 积分倍率
 * @param  kd 微分倍率
 * @retval None
 */
void spid_tune(spid_t pid, float kp, float ki, float kd)
{
    // Check for validity
    if (kp < 0 || ki < 0 || kd < 0)
        return;

    // Compute sample time in seconds
    float ssec = ((float)pid->sampletime) / ((float)TICK_SECOND);

    pid->Kp = kp;
    pid->Ki = ki * ssec;
    pid->Kd = kd / ssec;

    if (pid->direction == E_PID_REVERSE) {
        pid->Kp = 0 - pid->Kp;
        pid->Ki = 0 - pid->Ki;
        pid->Kd = 0 - pid->Kd;
    }
}

/**
 * @brief  PID 倍率变量换算
 * @param  pid PID 变量结构体指针
 * @param  time
 * @retval None
 */
void pid_sample(spid_t pid, uint32_t time)
{
    if (time > 0) {
        float ratio = (float)(time * (TICK_SECOND / 1000)) / (float)pid->sampletime;
        pid->Ki *= ratio;
        pid->Kd /= ratio;
        pid->sampletime = time * (TICK_SECOND / 1000);
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
void pid_limits(spid_t pid, float min, float max)
{
    if (min >= max)
        return;
    pid->omin = min;
    pid->omax = max;
    // Adjust output to new limits
    if (pid->automode) {
        if (*(pid->output) > pid->omax)
            *(pid->output) = pid->omax;
        else if (*(pid->output) < pid->omin)
            *(pid->output) = pid->omin;

        if (pid->iterm > pid->omax)
            pid->iterm = pid->omax;
        else if (pid->iterm < pid->omin)
            pid->iterm = pid->omin;
    }
}

/**
 * @brief  PID 使能自动计算标志
 * @param  pid PID 变量结构体指针
 * @retval None
 */
void pid_auto(spid_t pid)
{
    // If going from manual to auto
    if (!pid->automode) {
        pid->iterm = *(pid->output);
        pid->lastin = *(pid->input);
        if (pid->iterm > pid->omax)
            pid->iterm = pid->omax;
        else if (pid->iterm < pid->omin)
            pid->iterm = pid->omin;
        pid->automode = true;
    }
}

/**
 * @brief  PID 失能自动计算标志
 * @param  pid PID 变量结构体指针
 * @retval None
 */
void pid_manual(spid_t pid)
{
    pid->automode = false;
}

/**
 * @brief  PID 计算方向设置
 * @param  pid PID 变量结构体指针
 * @param  dir PID 计算方向
 * @retval None
 */
void pid_direction(spid_t pid, enum pid_control_directions dir)
{
    if (pid->automode && pid->direction != dir) {
        pid->Kp = (0 - pid->Kp);
        pid->Ki = (0 - pid->Ki);
        pid->Kd = (0 - pid->Kd);
    }
    pid->direction = dir;
}

void ffprintff(float float_num)
{
    printf("%d.%03d", (uint32_t)float_num, (uint16_t)((float_num * 1000) - (uint32_t)(float_num)*1000));
}

/**
 * @brief  PID 变量打印
 * @param  pid PID 变量结构体指针
 * @retval None
 */
void pid_log_k(spid_t pid)
{
    printf("\n+++  PID  K Conf +++\n");

    printf("Kp | ");
    ffprintff(pid->Kp);

    printf("Ki | ");
    ffprintff(pid->Ki);

    printf("Kd | ");
    ffprintff(pid->Kd);

    printf("\n");
}

/**
 * @brief  PID 变量打印
 * @param  pid PID 变量结构体指针
 * @retval None
 */
void pid_log_d(spid_t pid)
{
    printf("\n+++  PID  K Conf +++\n");

    printf("input | ");
    ffprintff(*(pid->input));

    printf(" output | ");
    ffprintff(*(pid->output));

    printf(" setpoint | ");
    ffprintff(*(pid->setpoint));

    printf("\n");
}

/**
 * @file    beep.c
 * @brief   蜂鸣器控制
 *
 * 定时器9 非高级定时器 单纯PWM输出
 * APB2 总线 120MHz
 *
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "beep.h"

/* Extern variables ----------------------------------------------------------*/
extern TIM_HandleTypeDef htim9;

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/
typedef struct {
    eBeep_Status state;  /* 工作状态 */
    eBeep_Freq freq;     /* 响的频率 */
    uint16_t t_on;       /* 周期内响的时间 单位毫秒 */
    uint16_t t_off;      /* 周期内熄的时间 单位毫秒 */
    uint16_t period_cnt; /* 周期个数 */
    uint32_t start;      /* 开始时间脉搏 */
} sBeep_Conf;

/* Private define ------------------------------------------------------------*/
#define BEEP_TIM htim9             /* 定时器9 */
#define BEEP_TIM_CHN TIM_CHANNEL_2 /* 通道2 */

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
static sBeep_Conf gBeep_Conf;

/* Private constants ---------------------------------------------------------*/
const eBeep_Freq cBeepFreqs[] = {
    eBeep_Freq_do, eBeep_Freq_re, eBeep_Freq_mi, eBeep_Freq_fa, eBeep_Freq_so, eBeep_Freq_la, eBeep_Freq_ti,
};

/* Private function prototypes -----------------------------------------------*/
static void beep_Conf_Init_Start(void);
static void beep_Conf_Set_State(eBeep_Status state);

/* Private user code ---------------------------------------------------------*/

/**
 * @brief  蜂鸣器空闲状态确认
 * @param  None
 * @retval 0 忙 1 空闲
 */
uint8_t beep_Is_Hima(void)
{
    if (gBeep_Conf.state != eBeep_Status_Hima) {
        return 0;
    }
    return 1;
}

/**
 * @brief  蜂鸣器 清空上次PWM输出次数记录
 * @param  None
 * @retval None
 */
static void beep_Conf_Init_Start(void)
{
    gBeep_Conf.start = xTaskGetTickCount();
}

/**
 * @brief  蜂鸣器空闲状态 修改 Sudo
 * @note   无条件修改蜂鸣器工作状态
 * @param  None
 * @retval None
 */
static void beep_Conf_Set_State(eBeep_Status state)
{
    gBeep_Conf.state = state;
}

/**
 * @brief  蜂鸣器 频率配置修改
 * @param  freq 声响频率选择
 * @retval None
 */
void beep_Conf_Set_Freq(eBeep_Freq freq)
{
    if (beep_Is_Hima() == 0) {
        return;
    }
    gBeep_Conf.freq = freq;
    __HAL_TIM_SET_AUTORELOAD(&BEEP_TIM, freq);
    __HAL_TIM_SET_COMPARE(&BEEP_TIM, BEEP_TIM_CHN, freq / 2);
}

/**
 * @brief  蜂鸣器
 * @param  None
 * @retval None
 */
void beep_Conf_Set_T_on(uint16_t t_on)
{
    if (beep_Is_Hima() == 0) {
        return;
    }
    gBeep_Conf.t_on = t_on;
}

/**
 * @brief  蜂鸣器
 * @param  None
 * @retval None
 */
void beep_Conf_Set_T_off(uint16_t t_off)
{
    if (beep_Is_Hima() == 0) {
        return;
    }
    gBeep_Conf.t_off = t_off;
}

/**
 * @brief  蜂鸣器
 * @param  None
 * @retval None
 */
void beep_Conf_Set_Period_Cnt(uint16_t period_cnt)
{
    if (beep_Is_Hima()) {
        gBeep_Conf.period_cnt = period_cnt;
    }
}

/**
 * @brief  蜂鸣器 PWM 开始输出
 * @param  None
 * @retval None
 */
void beep_Start(void)
{
    HAL_TIM_PWM_Start(&BEEP_TIM, BEEP_TIM_CHN);
    beep_Conf_Init_Start();
    beep_Conf_Set_State(eBeep_Status_Isogasi);
}

/**
 * @brief  蜂鸣器 PWM 开始输出 带配置
 * @param  freq         响的频率
 * @param  t_on        周期内响的时间 单位毫秒
 * @param  t_off       周期内熄的时间 单位毫秒
 * @param  period_cnt  周期个数
 * @retval None
 */
void beep_Start_With_Conf(eBeep_Freq freq, uint16_t t_on, uint16_t t_off, uint16_t period_cnt)
{
    beep_Conf_Set_Freq(freq);
    beep_Conf_Set_T_on(t_on);
    beep_Conf_Set_T_off(t_off);
    beep_Conf_Set_Period_Cnt(period_cnt);
    beep_Start();
}

/**
 * @brief  蜂鸣器 PWM 开始输出 循环音调
 * @param  None
 * @retval None
 */
void beep_Start_With_Loop(void)
{
    static uint8_t idx = 0;

    beep_Conf_Set_Freq(cBeepFreqs[idx % ARRAY_LEN(cBeepFreqs)]);
    ++idx;
    beep_Start();
}

/**
 * @brief  蜂鸣器 PWM 停止输出
 * @param  None
 * @retval None
 */
void beep_Stop(void)
{
    HAL_TIM_PWM_Stop(&BEEP_TIM, BEEP_TIM_CHN);    /* 停止PWM输出 */
    __HAL_TIM_CLEAR_IT(&BEEP_TIM, TIM_IT_UPDATE); /* 清除更新事件标志位 */
    __HAL_TIM_SET_COUNTER(&BEEP_TIM, 0);          /* 清零定时器计数寄存器 */
    beep_Conf_Set_State(eBeep_Status_Hima);
}

/**
 * @brief  蜂鸣器 响声PWM 处理
 * @param  res 执行检查周期
 * @retval None
 */
void beep_Deal(uint32_t res)
{
    uint32_t gap;

    if (beep_Is_Hima() && HAL_TIM_PWM_GetState(&BEEP_TIM) == HAL_TIM_STATE_READY) { /* 空闲状态置位 PWM状态空闲 跳过检查 */
        return;
    }

    gap = xTaskGetTickCount() - gBeep_Conf.start;
    if ((gap > (gBeep_Conf.t_off + gBeep_Conf.t_on) * gBeep_Conf.period_cnt)) { /* 输出完成 */
        beep_Stop();                                                            /* 停止PWM输出 */
        return;
    }
    if ((gap % (gBeep_Conf.t_off + gBeep_Conf.t_on) > gBeep_Conf.t_on) || (gBeep_Conf.t_off + gBeep_Conf.t_on) * gBeep_Conf.period_cnt - gap < res / 2 * 3) {
        HAL_TIM_PWM_Stop(&BEEP_TIM, BEEP_TIM_CHN); /* 停止PWM输出 */
    } else {
        HAL_TIM_PWM_Start(&BEEP_TIM, BEEP_TIM_CHN); /* 启动PWM输出 */
    }
}

/**
 * @brief  蜂鸣器 初始化
 * @param  None
 * @retval None
 */
void beep_Init(void)
{
    beep_Stop();
    beep_Conf_Set_Period_Cnt(0);
    beep_Conf_Set_T_on(500);
    beep_Conf_Set_T_off(0);
    beep_Conf_Set_Freq(eBeep_Freq_mi);
    beep_Start();
}

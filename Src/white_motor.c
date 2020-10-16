/**
 * @file    white_motor.c
 * @brief   白板电机控制
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "motor.h"
#include "m_drv8824.h"
#include "white_motor.h"
#include <math.h>

/* Extern variables ----------------------------------------------------------*/
extern TIM_HandleTypeDef htim1;

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/
typedef enum {
    eWhite_Motor_In_Finish,
    eWhite_Motor_Out_Finish,
    eWhite_Motor_Undone,
} eWhite_Motor_Status;

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/
#define WHITE_MOTOR_PD_PCS_UNT 8
#define WHITE_MOTOR_PD_PCS_SUM 322
#define WHITE_MOTOR_PD_PCS_PATCH 6
#define WHITE_MOTOR_PD_PCS_GAP (800)
#define WHITE_MOTOR_PD_FREQ_MAX (7714.286) /* 108000000 / 14000 */
#define WHITE_MOTOR_PD_FREQ_MIN (2000.00) /* 108000000 / 54000 */
#define WHITE_MOTOR_PD_E_K (0.30)
#define WHITE_MOTOR_PD_E_B (6)

#define WHITE_MOTOR_WH_PCS_UNT 8
#define WHITE_MOTOR_WH_PCS_SUM 318
#define WHITE_MOTOR_WH_PCS_GAP (800)
#define WHITE_MOTOR_WH_FREQ_MAX (4200.00) /* 108000000 / 32000 */
#define WHITE_MOTOR_WH_FREQ_MIN (2500.00) /* 108000000 / 43200 */
#define WHITE_MOTOR_WH_E_K (0.20)
#define WHITE_MOTOR_WH_E_B (6)

#define WHITE_MOTOR_WH_FREQ_MIN2 (2000.00)
#define WHITE_MOTOR_WH_E_K2 (0.60)
#define WHITE_MOTOR_WH_E_B2 (80)

/* Private variables ---------------------------------------------------------*/
static eMotorDir gWhite_Motor_Dir = eMotorDir_FWD;
static uint32_t gWhite_Motor_Position = 0xFFFFFFFF;
static uint32_t gWhite_Motor_SRC_Buffer[3] = {0, 0, 0};
static eWhite_Motor_Status gWhite_Motor_Status = eWhite_Motor_Undone;
static uint8_t gWhite_Motor_PD_Failed_Flag = 0; /* PD方向运动失败标志 此标志用于重新清零白板电机位置 白板方向实际距离大于PD方向距离时 PD方向将永远不可达 */

/* Private constants ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
static void gWhite_Motor_Position_Set(uint32_t position);
static void gWhite_Motor_Position_Inc(uint32_t position);
static void gWhite_Motor_Position_Dec(uint32_t position);
static void gWhite_Motor_Position_Clr(void);

/* Private user code ---------------------------------------------------------*/

eWhite_Motor_Status gWhite_Motor_Status_Get(void)
{
    return gWhite_Motor_Status;
}

void gWhite_Motor_Status_Set(eWhite_Motor_Status status)
{
    gWhite_Motor_Status = status;
}

/**
 * @brief  白板电机方向 获取
 * @param  None
 * @retval 白板电机方向
 */
eMotorDir gWhite_Motor_Dir_Get(void)
{
    return gWhite_Motor_Dir;
}

/**
 * @brief  白板电机方向 设置
 * @param  dir 白板电机方向
 * @retval None
 */
void gWhite_Motor_Dir_Set(eMotorDir dir)
{
    gWhite_Motor_Dir = dir;
}

/**
 * @brief  白板电机位置 获取
 * @param  None
 * @retval 白板电机位置
 */
uint32_t gWhite_Motor_Position_Get(void)
{
    return gWhite_Motor_Position;
}

/**
 * @brief  白板电机PD方向运动失败标志 使置位
 * @param  None
 * @retval None
 */
void gWhite_Motor_PD_Failed_Flag_Mark(void)
{
    gWhite_Motor_PD_Failed_Flag = 1;
}

/**
 * @brief  白板电机PD方向运动失败标志 清零
 * @param  None
 * @retval None
 */
void gWhite_Motor_PD_Failed_Flag_Clr(void)
{
    gWhite_Motor_PD_Failed_Flag = 0;
}

/**
 * @brief  白板电机PD方向运动失败标志 获取
 * @param  None
 * @retval gWhite_Motor_PD_Failed_Flag
 */
uint8_t gWhite_Motor_PD_Failed_Flag_Get(void)
{
    return gWhite_Motor_PD_Failed_Flag;
}

/**
 * @brief  白板电机位置 使能
 * @param  None
 * @retval None
 */
void white_Motor_Active(void)
{
    HAL_GPIO_WritePin(STEP_NCS1_GPIO_Port, STEP_NCS1_Pin, GPIO_PIN_RESET);
}

/**
 * @brief  白板电机位置 失能
 * @param  None
 * @retval None
 */
void white_Motor_Deactive(void)
{
    HAL_GPIO_WritePin(STEP_NCS1_GPIO_Port, STEP_NCS1_Pin, GPIO_PIN_SET);
}

/**
 * @brief  白板电机位置 检查是否已经处于收缩状态
 * @note   已运动步数为0
 * @param  None
 * @retval 白板电机位置
 */
uint8_t white_Motor_Position_Is_In(void)
{
    if (motor_OPT_Status_Get_White_In() == eMotor_OPT_Status_OFF && gWhite_Motor_Status_Get() == eWhite_Motor_In_Finish) {
        return 1;
    }
    return 0;
}

/**
 * @brief  白板电机位置 检查是否已经处于伸展状态
 * @note   已运动步数超过极限位置98%
 * @param  None
 * @retval 白板电机位置
 */
uint8_t white_Motor_Position_Is_Out(void)
{
    return (gWhite_Motor_Position_Get() != 0xFFFFFFFF) && (gWhite_Motor_Status_Get() == eWhite_Motor_Out_Finish) &&
           (motor_OPT_Status_Get_White_In() == eMotor_OPT_Status_ON);
}

/**
 * @brief  白板电机位置 设置
 * @param  白板电机位置
 * @retval None
 */
static void gWhite_Motor_Position_Set(uint32_t position)
{
    gWhite_Motor_Position = position;
}

/**
 * @brief  白板电机位置 增量
 * @param  白板电机位置
 * @retval None
 */
static void gWhite_Motor_Position_Inc(uint32_t position)
{
    gWhite_Motor_Position_Set(gWhite_Motor_Position_Get() + position);
}

/**
 * @brief  白板电机位置 增量
 * @param  白板电机位置
 * @retval None
 */
static void gWhite_Motor_Position_Dec(uint32_t position)
{
    gWhite_Motor_Position_Set(gWhite_Motor_Position_Get() - position);
}

/**
 * @brief  白板电机位置 清零
 * @param  None
 * @retval None
 */
static void gWhite_Motor_Position_Clr(void)
{
    gWhite_Motor_Position_Set(0);
}

/**
 * @brief  白板电机位置 重置
 * @param  None
 * @retval None
 */
static void gWhite_Motor_Position_Rst(void)
{
    gWhite_Motor_Position_Set(0xFFFFFFFF);
}

/**
 * @brief 白板电机 停车确认
 * @param  None
 * @retval 0 正常停车 1 停车超时 2 异常
 */
uint8_t white_Motor_Wait_Stop(uint32_t timeout)
{
    TickType_t xTick;

    xTick = xTaskGetTickCount();
    switch (gWhite_Motor_Dir_Get()) {
        case eMotorDir_REV:
            do {
                if (white_Motor_Position_Is_In()) {
                    white_Motor_Deactive();
                    PWM_AW_Stop();
                    gWhite_Motor_Position_Clr();
                    return 0;
                }
                vTaskDelay(1);
            } while (xTaskGetTickCount() - xTick < timeout);
            PWM_AW_Stop();
            return 1;
        case eMotorDir_FWD:
        default:
            do {
                if (white_Motor_Position_Is_Out()) {
                    white_Motor_Deactive();
                    return 0;
                }
                vTaskDelay(1);
            } while (xTaskGetTickCount() - xTick < timeout);
            PWM_AW_Stop();
            return 1;
    }
    PWM_AW_Stop();
    return 2;
}

/**
 * @brief  白板电机 PD位置
 * @param  None
 * @note   此操作为复归
 * @retval 运动结果 0 正常 1 PWM启动失败 2 运动超时 3 位置异常
 */
uint8_t white_Motor_PD()
{
    // TickType_t xTick;

    // xTick = xTaskGetTickCount();
    // error_Emit(eError_Motor_White_Debug);

    if (white_Motor_Position_Is_In()) { /* 光耦被遮挡 处于收起状态 */
        white_Motor_Deactive();
        gWhite_Motor_Position_Clr(); /* 清空位置记录 */
        return 0;
    }

    gWhite_Motor_Position_Rst();                               /* 重置位置记录置非法值 0xFFFFFFFF */
    m_drv8824_Index_Switch(eM_DRV8824_Index_0, portMAX_DELAY); /* 等待PWM资源 */
    m_drv8824_Clear_Flag();                                    /* 清理故障标志 */
    m_drv8824_SetDir(eMotorDir_REV);                           /* 运动方向设置 硬件管脚 */
    gWhite_Motor_Dir_Set(eMotorDir_REV);                       /* 运动方向设置 目标方向 */
    gWhite_Motor_Status_Set(eWhite_Motor_Undone);              /* 状态记录初始化 */
    gPWM_TEST_AW_CNT_Clear();                                  /* PWM数目清零 */
    if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1) != HAL_OK) {  /* 启动PWM输出 */
        m_drv8824_release();
        return 1;
    }

    PWM_AW_IRQ_CallBcak();

    if (white_Motor_Wait_Stop(WHITE_MOTOR_RUN_PD_TIMEOUT) == 0) {
        m_drv8824_release();
        gWhite_Motor_PD_Failed_Flag_Clr();
        // xTick = xTaskGetTickCount() - xTick;
        // if (xTick) {
        //     return 0;
        // }
        return 0;
    }
    m_drv8824_release();
    if (white_Motor_Position_Is_In() == 0) {
        error_Emit(eError_Motor_White_Timeout_PD);
        gWhite_Motor_PD_Failed_Flag_Mark();
        return 3;
    }
    return 2;
}

/**
 * @brief  白板电机 白板位置
 * @param  None
 * @retval 运动结果 0 正常 1 PWM启动失败 2 运动超时 3 位置异常
 */
uint8_t white_Motor_WH()
{
    // TickType_t xTick;
    // xTick = xTaskGetTickCount();
    // error_Emit(eError_Motor_White_Debug);

    if (gWhite_Motor_PD_Failed_Flag_Get()) { /* PD方向运动异常 清零位置 */
        white_Motor_PD();
    } else if (white_Motor_Position_Is_Out()) { /* 已完成白板位置运动 */
        white_Motor_Deactive();
        return 0;
    }

    gWhite_Motor_Position_Rst();                               /* 重置位置记录置非法值 0xFFFFFFFF */
    m_drv8824_Index_Switch(eM_DRV8824_Index_0, portMAX_DELAY); /* 等待PWM资源 */
    m_drv8824_Clear_Flag();                                    /* 清理故障标志 */
    m_drv8824_SetDir(eMotorDir_FWD);                           /* 运动方向设置 硬件管脚 */
    gWhite_Motor_Dir_Set(eMotorDir_FWD);                       /* 运动方向设置 目标方向 */
    gWhite_Motor_Status_Set(eWhite_Motor_Undone);              /* 状态记录初始化 */
    gPWM_TEST_AW_CNT_Clear();                                  /* PWM数目清零 */
    if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1) != HAL_OK) {  /* 启动PWM输出 */
        m_drv8824_release();
        return 1;
    }

    PWM_AW_IRQ_CallBcak();

    if (white_Motor_Wait_Stop(WHITE_MOTOR_RUN_WH_TIMEOUT) == 0) {
        m_drv8824_release();
        // xTick = xTaskGetTickCount() - xTick;
        // if (xTick) {
        //     return 0;
        // }
        return 0;
    }
    m_drv8824_release();
    if (white_Motor_Position_Is_In() || gWhite_Motor_Status_Get() == eWhite_Motor_Undone) {
        error_Emit(eError_Motor_White_Timeout_WH);
        return 3;
    }
    return 2;
}

/**
 * @brief  白板电机位置翻转
 * @param  None
 * @retval 运动结果 0 正常 1 PWM启动失败 2 运动超时 3 位置异常
 */
uint8_t white_Motor_Toggle(void)
{
    if (white_Motor_Position_Is_In()) {
        return white_Motor_WH();
    }
    return white_Motor_PD();
}

/**
 * @brief  白板电机PD方向 PWM输出周期计算
 * @param  idx 输出计数
 * @retval 周期脉冲长度
 */
uint16_t whiteMotor_PWM_Period_In(uint16_t idx)
{
    float freq;

    freq = WHITE_MOTOR_PD_FREQ_MIN + (WHITE_MOTOR_PD_FREQ_MAX - WHITE_MOTOR_PD_FREQ_MIN) / (1 + expf(-WHITE_MOTOR_PD_E_K * idx + WHITE_MOTOR_PD_E_B));

    return 108000000.0 / freq;
}

/**
 * @brief  白板电机PD方向 PWM输出控制
 * @param  None
 * @retval 0 输出完成 1 输出未完成
 */
uint8_t white_Motor_PWM_Gen_In(void)
{
    uint32_t cnt;
    static uint16_t patch = 0;

    if (motor_OPT_Status_Get_White_In() == eMotor_OPT_Status_OFF) {
        ++patch;
    } else {
        patch = 0;
    }

    if (patch >= WHITE_MOTOR_PD_PCS_PATCH) {
        PWM_AW_Stop();
        gWhite_Motor_Status_Set(eWhite_Motor_In_Finish);
        return 0;
    }

    cnt = gPWM_TEST_AW_CNT_Get();                                      /* 获取当前脉冲计数 */
    gWhite_Motor_SRC_Buffer[0] = whiteMotor_PWM_Period_In(cnt);        /* 周期长度 */
    gWhite_Motor_SRC_Buffer[1] = WHITE_MOTOR_PD_PCS_UNT;               /* 重复次数 */
    gWhite_Motor_SRC_Buffer[2] = (gWhite_Motor_SRC_Buffer[0] + 1) / 2; /* 占空比 默认50% */
    /* burst模式修改时基单元 */
    HAL_TIM_DMABurst_WriteStart(&htim1, TIM_DMABASE_ARR, TIM_DMA_UPDATE, (uint32_t *)gWhite_Motor_SRC_Buffer, TIM_DMABURSTLENGTH_3TRANSFERS);
    gWhite_Motor_Position_Dec(gWhite_Motor_SRC_Buffer[1]); /* 自减位置记录 */
    gPWM_TEST_AW_CNT_Inc();                                /* 自增脉冲计数 */
    return 1;
}

/**
 * @brief  白板电机白板方向 PWM输出周期计算
 * @param  idx 输出计数
 * @retval 周期脉冲长度
 */
uint16_t whiteMotor_PWM_Period_Out(uint16_t idx)
{
    float freq;

    if (idx < WHITE_MOTOR_WH_PCS_SUM / 2) {
        freq = WHITE_MOTOR_WH_FREQ_MIN + (WHITE_MOTOR_WH_FREQ_MAX - WHITE_MOTOR_WH_FREQ_MIN) / (1 + expf(-WHITE_MOTOR_WH_E_K * idx + WHITE_MOTOR_WH_E_B));
    } else {
        freq = WHITE_MOTOR_WH_FREQ_MIN2 + (WHITE_MOTOR_WH_FREQ_MAX - WHITE_MOTOR_WH_FREQ_MIN2) /
                                              (1 + expf(WHITE_MOTOR_WH_E_K2 * (idx - WHITE_MOTOR_WH_PCS_SUM / 2) - WHITE_MOTOR_WH_E_B2));
    }

    return 108000000.0 / freq;
}

/**
 * @brief  白板电机白板方向 PWM输出控制
 * @param  None
 * @retval 0 输出完成 1 输出未完成
 */
uint8_t white_Motor_PWM_Gen_Out(void)
{
    uint32_t cnt;
    eMotor_OPT_Status status;

    cnt = gPWM_TEST_AW_CNT_Get(); /* 获取当前脉冲计数 */
    status = motor_OPT_Status_Get_White_In();

    if (cnt > WHITE_MOTOR_WH_PCS_SUM) { /* 停止输出 */
        PWM_AW_Stop();
        gWhite_Motor_Status_Set(eWhite_Motor_Out_Finish);
        return 0;
    } else {
        gWhite_Motor_SRC_Buffer[0] = whiteMotor_PWM_Period_Out(cnt);       /* 周期长度 */
        gWhite_Motor_SRC_Buffer[1] = WHITE_MOTOR_WH_PCS_UNT;               /* 重复次数 */
        gWhite_Motor_SRC_Buffer[2] = (gWhite_Motor_SRC_Buffer[0] + 1) / 2; /* 占空比 默认50% */
        /* burst模式修改时基单元 */
        HAL_TIM_DMABurst_WriteStart(&htim1, TIM_DMABASE_ARR, TIM_DMA_UPDATE, (uint32_t *)gWhite_Motor_SRC_Buffer, TIM_DMABURSTLENGTH_3TRANSFERS);
        if (status == eMotor_OPT_Status_ON) {
            gWhite_Motor_Position_Inc(gWhite_Motor_SRC_Buffer[1]); /* 自增位置记录 */
        } else {
            gWhite_Motor_Position_Clr();
        }
    }
    if (status == eMotor_OPT_Status_ON) {
        gPWM_TEST_AW_CNT_Inc(); /* 自增脉冲计数 */
    }
    return 1;
}

/**
 * @brief  白板电机初始化
 * @note   尽量在入仓位置动作
 * @param  None
 * @retval None
 */
void whilte_Motor_Init(void)
{
    white_Motor_PD();
    if (motor_OPT_Status_Get_White_In() == eMotor_OPT_Status_ON) {
        white_Motor_PD();
    }
    white_Motor_WH();
    if (motor_OPT_Status_Get_White_In() == eMotor_OPT_Status_OFF) {
        white_Motor_WH();
    }
}

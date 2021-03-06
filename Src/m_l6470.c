/**
 * 非菊花链拓扑结构
 */

/**
 ******************************************************************************
 * @file    dspin.c
 * @author  IPC Rennes
 * @version V2.0
 * @date    October 4, 2013
 * @brief   dSPIN (L6470 and L6472) product related routines
 * @note    (C) COPYRIGHT 2013 STMicroelectronics
 ******************************************************************************
 * @copy
 *
 * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
 * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
 * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
 * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
 * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
 * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
 *
 * <h2><center>&copy; COPYRIGHT 2013 STMicroelectronics</center></h2>
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "m_l6470.h"
#include "dspin_config.h"
#include "stdio.h"

/** @addtogroup  dSPIN FW library interface
 * @{
 */

/* Extern variables ----------------------------------------------------------*/
extern SPI_HandleTypeDef hspi2;

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
static eM_L6470_Index gML6470Index = eM_L6470_Index_0;
static SemaphoreHandle_t m_l6470_spi_sem = NULL;

/* Private function prototypes -----------------------------------------------*/
static uint8_t m_l6470_acquire(uint32_t timeout);
static dSPIN_RegsStruct_TypeDef dSPIN_RegsStructs[2];

/* Private functions ---------------------------------------------------------*/
static void m_l6470_NCS_GPIO_Deactive(void);
void dSPIN_Hard_Stop(void);
void dSPIN_Soft_HiZ(void);

/**
 * @brief  获取SPI资源
 * @param  timeout 等待资源超时时间
 * @retval None
 */
static uint8_t m_l6470_acquire(uint32_t timeout)
{
    if (xSemaphoreTake(m_l6470_spi_sem, timeout) == pdPASS) {
        return 0;
    }
    return 1;
}

/**
 * @brief  释放SPI资源
 * @param  None
 * @retval None
 */
uint8_t m_l6470_release(void)
{
    if (xSemaphoreGive(m_l6470_spi_sem) == pdPASS) {
        return 0;
    }
    return 1;
}

/**
 * @brief  索引切换
 * @param  index       索引值
 * @note   index 参考 eM_L6470_Index
 * @retval None
 */
uint8_t m_l6470_Index_Switch(eM_L6470_Index index, uint32_t timeout)
{
    if (m_l6470_acquire(timeout) == 0) {
        gML6470Index = index;
        return 0;
    }
    return 1;
}

/**
 * @brief  Reads dSPIN internal writable registers and compares with the values in the code
 * @param  dSPIN_RegsStruct Configuration structure address (pointer to configuration structure)
 * @retval Bitmap with the bits, corresponding to the unmatched registers, set
 */
uint32_t dSPIN_Registers_Check(dSPIN_RegsStruct_TypeDef * dSPIN_RegsStruct)
{
    uint32_t result = 0;
    uint32_t param;

    param = dSPIN_Get_Param(dSPIN_ABS_POS);
    if (param != dSPIN_RegsStruct->ABS_POS) {
        result |= (1 << dSPIN_ABS_POS);
    }

    param = dSPIN_Get_Param(dSPIN_EL_POS);
    if (param != dSPIN_RegsStruct->EL_POS) {
        result |= (1 << dSPIN_EL_POS);
    }

    param = dSPIN_Get_Param(dSPIN_MARK);
    if (param != dSPIN_RegsStruct->MARK) {
        result |= (1 << dSPIN_MARK);
    }

    param = dSPIN_Get_Param(dSPIN_ACC);
    if (param != dSPIN_RegsStruct->ACC) {
        result |= (1 << dSPIN_ACC);
    }

    param = dSPIN_Get_Param(dSPIN_DEC);
    if (param != dSPIN_RegsStruct->DEC) {
        result |= (1 << dSPIN_DEC);
    }

    param = dSPIN_Get_Param(dSPIN_MAX_SPEED);
    if (param != dSPIN_RegsStruct->MAX_SPEED) {
        result |= (1 << dSPIN_MAX_SPEED);
    }

    param = dSPIN_Get_Param(dSPIN_MIN_SPEED);
    if (param != dSPIN_RegsStruct->MIN_SPEED) {
        result |= (1 << dSPIN_MIN_SPEED);
    }

    param = dSPIN_Get_Param(dSPIN_KVAL_HOLD);
    if (param != dSPIN_RegsStruct->KVAL_HOLD) {
        result |= (1 << dSPIN_KVAL_HOLD);
    }

    param = dSPIN_Get_Param(dSPIN_KVAL_RUN);
    if (param != dSPIN_RegsStruct->KVAL_RUN) {
        result |= (1 << dSPIN_KVAL_RUN);
    }

    param = dSPIN_Get_Param(dSPIN_KVAL_ACC);
    if (param != dSPIN_RegsStruct->KVAL_ACC) {
        result |= (1 << dSPIN_KVAL_ACC);
    }

    param = dSPIN_Get_Param(dSPIN_KVAL_DEC);
    if (param != dSPIN_RegsStruct->KVAL_DEC) {
        result |= (1 << dSPIN_KVAL_DEC);
    }

    param = dSPIN_Get_Param(dSPIN_INT_SPD);
    if (param != dSPIN_RegsStruct->INT_SPD) {
        result |= (1 << dSPIN_INT_SPD);
    }

    param = dSPIN_Get_Param(dSPIN_ST_SLP);
    if (param != dSPIN_RegsStruct->ST_SLP) {
        result |= (1 << dSPIN_ST_SLP);
    }

    param = dSPIN_Get_Param(dSPIN_FN_SLP_ACC);
    if (param != dSPIN_RegsStruct->FN_SLP_ACC) {
        result |= (1 << dSPIN_FN_SLP_ACC);
    }

    param = dSPIN_Get_Param(dSPIN_FN_SLP_DEC);
    if (param != dSPIN_RegsStruct->FN_SLP_DEC) {
        result |= (1 << dSPIN_FN_SLP_DEC);
    }

    param = dSPIN_Get_Param(dSPIN_K_THERM);
    if (param != dSPIN_RegsStruct->K_THERM) {
        result |= (1 << dSPIN_K_THERM);
    }

    param = dSPIN_Get_Param(dSPIN_OCD_TH);
    if (param != dSPIN_RegsStruct->OCD_TH) {
        result |= (1 << dSPIN_OCD_TH);
    }

    param = dSPIN_Get_Param(dSPIN_STALL_TH);
    if (param != dSPIN_RegsStruct->STALL_TH) {
        result |= (1 << dSPIN_STALL_TH);
    }

    param = dSPIN_Get_Param(dSPIN_FS_SPD);
    if (param != dSPIN_RegsStruct->FS_SPD) {
        result |= (1 << dSPIN_FS_SPD);
    }

    param = dSPIN_Get_Param(dSPIN_STEP_MODE);
    if (param != dSPIN_RegsStruct->STEP_MODE) {
        result |= (1 << dSPIN_STEP_MODE);
    }

    param = dSPIN_Get_Param(dSPIN_ALARM_EN);
    if (param != dSPIN_RegsStruct->ALARM_EN) {
        result |= (1 << dSPIN_ALARM_EN);
    }

    param = dSPIN_Get_Param(dSPIN_CONFIG);
    if (param != dSPIN_RegsStruct->CONFIG) {
        result |= (1 << dSPIN_CONFIG);
    }

    param = dSPIN_Get_Param(dSPIN_STATUS);
    if ((param & 0x7e00) < 0x7e00) {
        result |= (1 << dSPIN_STATUS);
    }
    return result;
}

/**
 * @brief  驱动参数初始化
 * @param  None
 * @retval None
 */
void m_l6470_Params_Init(void)
{
    dSPIN_Regs_Struct_Reset(&dSPIN_RegsStructs[gML6470Index]);
    switch (gML6470Index) {
        case 0:
            dSPIN_RegsStructs[gML6470Index].ACC = Index_0_dSPIN_CONF_PARAM_ACC;
            dSPIN_RegsStructs[gML6470Index].DEC = Index_0_dSPIN_CONF_PARAM_DEC;
            dSPIN_RegsStructs[gML6470Index].MAX_SPEED = Index_0_dSPIN_CONF_PARAM_MAX_SPEED;
            dSPIN_RegsStructs[gML6470Index].FS_SPD = Index_0_dSPIN_CONF_PARAM_FS_SPD;
            dSPIN_RegsStructs[gML6470Index].MIN_SPEED = Index_0_dSPIN_CONF_PARAM_MIN_SPEED;
            dSPIN_RegsStructs[gML6470Index].KVAL_ACC = Index_0_dSPIN_CONF_PARAM_KVAL_ACC;
            dSPIN_RegsStructs[gML6470Index].KVAL_DEC = Index_0_dSPIN_CONF_PARAM_KVAL_DEC;
            dSPIN_RegsStructs[gML6470Index].KVAL_RUN = Index_0_dSPIN_CONF_PARAM_KVAL_RUN;
            dSPIN_RegsStructs[gML6470Index].KVAL_HOLD = Index_0_dSPIN_CONF_PARAM_KVAL_HOLD;
            dSPIN_RegsStructs[gML6470Index].K_THERM = Index_0_dSPIN_CONF_PARAM_K_THERM;
            dSPIN_RegsStructs[gML6470Index].INT_SPD = Index_0_dSPIN_CONF_PARAM_INT_SPD;
            dSPIN_RegsStructs[gML6470Index].ST_SLP = Index_0_dSPIN_CONF_PARAM_ST_SLP;
            dSPIN_RegsStructs[gML6470Index].FN_SLP_ACC = Index_0_dSPIN_CONF_PARAM_FN_SLP_ACC;
            dSPIN_RegsStructs[gML6470Index].FN_SLP_DEC = Index_0_dSPIN_CONF_PARAM_FN_SLP_DEC;
            dSPIN_RegsStructs[gML6470Index].STALL_TH = Index_0_dSPIN_CONF_PARAM_STALL_TH;
            dSPIN_RegsStructs[gML6470Index].CONFIG = Index_0_dSPIN_CONF_PARAM_CONFIG;
            dSPIN_RegsStructs[gML6470Index].STEP_MODE = Index_0_dSPIN_CONF_PARAM_STEP_MODE;
            dSPIN_RegsStructs[gML6470Index].ALARM_EN = Index_0_dSPIN_CONF_PARAM_ALARM_EN;
            dSPIN_RegsStructs[gML6470Index].OCD_TH = Index_0_dSPIN_CONF_PARAM_OCD_TH;
            break;
        default:
            dSPIN_RegsStructs[gML6470Index].ACC = Index_1_dSPIN_CONF_PARAM_ACC;
            dSPIN_RegsStructs[gML6470Index].DEC = Index_1_dSPIN_CONF_PARAM_DEC;
            dSPIN_RegsStructs[gML6470Index].MAX_SPEED = Index_1_dSPIN_CONF_PARAM_MAX_SPEED;
            dSPIN_RegsStructs[gML6470Index].FS_SPD = Index_1_dSPIN_CONF_PARAM_FS_SPD;
            dSPIN_RegsStructs[gML6470Index].MIN_SPEED = Index_1_dSPIN_CONF_PARAM_MIN_SPEED;
            dSPIN_RegsStructs[gML6470Index].KVAL_ACC = Index_1_dSPIN_CONF_PARAM_KVAL_ACC;
            dSPIN_RegsStructs[gML6470Index].KVAL_DEC = Index_1_dSPIN_CONF_PARAM_KVAL_DEC;
            dSPIN_RegsStructs[gML6470Index].KVAL_RUN = Index_1_dSPIN_CONF_PARAM_KVAL_RUN;
            dSPIN_RegsStructs[gML6470Index].KVAL_HOLD = Index_1_dSPIN_CONF_PARAM_KVAL_HOLD;
            dSPIN_RegsStructs[gML6470Index].K_THERM = Index_1_dSPIN_CONF_PARAM_K_THERM;
            dSPIN_RegsStructs[gML6470Index].INT_SPD = Index_1_dSPIN_CONF_PARAM_INT_SPD;
            dSPIN_RegsStructs[gML6470Index].ST_SLP = Index_1_dSPIN_CONF_PARAM_ST_SLP;
            dSPIN_RegsStructs[gML6470Index].FN_SLP_ACC = Index_1_dSPIN_CONF_PARAM_FN_SLP_ACC;
            dSPIN_RegsStructs[gML6470Index].FN_SLP_DEC = Index_1_dSPIN_CONF_PARAM_FN_SLP_DEC;
            dSPIN_RegsStructs[gML6470Index].STALL_TH = Index_1_dSPIN_CONF_PARAM_STALL_TH;
            dSPIN_RegsStructs[gML6470Index].CONFIG = Index_1_dSPIN_CONF_PARAM_CONFIG;

            dSPIN_RegsStructs[gML6470Index].STEP_MODE = Index_1_dSPIN_CONF_PARAM_STEP_MODE;
            dSPIN_RegsStructs[gML6470Index].ALARM_EN = Index_1_dSPIN_CONF_PARAM_ALARM_EN;
            dSPIN_RegsStructs[gML6470Index].OCD_TH = Index_1_dSPIN_CONF_PARAM_OCD_TH;
            break;
    }

    /* Program all dSPIN registers */
    dSPIN_Registers_Set(&dSPIN_RegsStructs[gML6470Index]);
}

/**
 * @brief  驱动资源及参数初始化
 * @param  None
 * @retval None
 */
uint8_t m_l6470_Init(void)
{
    uint8_t result = 0;

    m_l6470_spi_sem = xSemaphoreCreateBinary();
    if (m_l6470_spi_sem == NULL || xSemaphoreGive(m_l6470_spi_sem) != pdPASS) {
        Error_Handler();
    }

    m_l6470_NCS_GPIO_Deactive();
    HAL_GPIO_WritePin(MOT_NRST_GPIO_Port, MOT_NRST_Pin, GPIO_PIN_RESET);
    vTaskDelay(pdMS_TO_TICKS(1000));
    HAL_GPIO_WritePin(MOT_NRST_GPIO_Port, MOT_NRST_Pin, GPIO_PIN_SET);

    /* 扫码电机驱动 */
    m_l6470_Index_Switch(eM_L6470_Index_0, portMAX_DELAY);
    m_l6470_Params_Init();
    dSPIN_Get_Status(); /* 读取电机状态 清除低压告警位 */
    if (dSPIN_Registers_Check(&dSPIN_RegsStructs[gML6470Index]) > 0) {
        result |= 0x01;
    }
    m_l6470_release();

    /* 托盘电机驱动 */
    m_l6470_Index_Switch(eM_L6470_Index_1, portMAX_DELAY);
    m_l6470_Params_Init();
    dSPIN_Get_Status(); /* 读取电机状态 清除低压告警位 */
    if (dSPIN_Registers_Check(&dSPIN_RegsStructs[gML6470Index]) > 0) {
        result |= 0x10;
    }
    m_l6470_release();
    return result;
}

/**
 * @brief  CS 激活
 * @note   拉低其中一个NCS脚以激活 拉高另一个灭活
 * @retval None
 */
void m_l6470_NCS_GPIO_Active(void)
{
    switch (gML6470Index) {
        case eM_L6470_Index_0:
            HAL_GPIO_WritePin(MOT_NCS1_GPIO_Port, MOT_NCS1_Pin, GPIO_PIN_RESET); /* 激活扫码电机 */
            HAL_GPIO_WritePin(MOT_NCS2_GPIO_Port, MOT_NCS2_Pin, GPIO_PIN_SET);   /* 灭活托盘电机 */
            break;
        case eM_L6470_Index_1:
        default:
            HAL_GPIO_WritePin(MOT_NCS1_GPIO_Port, MOT_NCS1_Pin, GPIO_PIN_SET);   /* 灭活扫码电机 */
            HAL_GPIO_WritePin(MOT_NCS2_GPIO_Port, MOT_NCS2_Pin, GPIO_PIN_RESET); /* 激活托盘电机 */
            break;
    }
}

/**
 * @brief  CS 灭活
 * @note   同时拉高 两个NCS脚
 * @param  None
 * @retval None
 */
void m_l6470_NCS_GPIO_Deactive(void)
{
    HAL_GPIO_WritePin(MOT_NCS1_GPIO_Port, MOT_NCS1_Pin, GPIO_PIN_SET); /* 灭活扫码电机 */
    HAL_GPIO_WritePin(MOT_NCS2_GPIO_Port, MOT_NCS2_Pin, GPIO_PIN_SET); /* 灭活托盘电机 */
}

/**
 * @brief  重置驱动
 * @param  None
 * @retval None
 */
void m_l6470_Reset_HW(void)
{
    m_l6470_NCS_GPIO_Active();
    HAL_GPIO_WritePin(MOT_NRST_GPIO_Port, MOT_NRST_Pin, GPIO_PIN_RESET); /* 重置托盘电机 */
    vTaskDelay(2);                                                       /* t-STBY-min 10uS t-lobicwu 38~45 usS t-cpwu 650 uS */
    HAL_GPIO_WritePin(MOT_NRST_GPIO_Port, MOT_NRST_Pin, GPIO_PIN_SET);
}

/**
 * @brief  BUSY 管脚配置
 * @param  pGPIOx       port
 * @param  pGPIO_Pin    pin
 * @retval None
 */
GPIO_PinState m_l6470_BUSY_GPIO_Get(void)
{
    switch (gML6470Index) {
        case eM_L6470_Index_0:
            return HAL_GPIO_ReadPin(MOT_NBUSY1_GPIO_Port, MOT_NBUSY1_Pin);

        case eM_L6470_Index_1:
        default:
            return HAL_GPIO_ReadPin(MOT_NBUSY2_GPIO_Port, MOT_NBUSY2_Pin);
    }
}

/**
 * @brief  FLAG 管脚配置
 * @param  pGPIOx       port
 * @param  pGPIO_Pin    pin
 * @retval None
 */
GPIO_PinState m_l6470_FLAG_GPIO_Get(void)
{
    switch (gML6470Index) {
        case eM_L6470_Index_0:
            return HAL_GPIO_ReadPin(MOT_NFLG1_GPIO_Port, MOT_NFLG1_Pin);

        case eM_L6470_Index_1:
        default:
            return HAL_GPIO_ReadPin(MOT_NFLG2_GPIO_Port, MOT_NFLG2_Pin);
    }
}

/**
 * @brief  Fills-in dSPIN configuration structure with default values.
 * @param  dSPIN_RegsStruct structure address (pointer to struct)
 * @retval None
 */
void dSPIN_Regs_Struct_Reset(dSPIN_RegsStruct_TypeDef * dSPIN_RegsStruct)
{
    dSPIN_RegsStruct->ABS_POS = 0;
    dSPIN_RegsStruct->EL_POS = 0;
    dSPIN_RegsStruct->MARK = 0;
    dSPIN_RegsStruct->ACC = 0x08A;
    dSPIN_RegsStruct->DEC = 0x08A;
    dSPIN_RegsStruct->MAX_SPEED = 0x041;
    dSPIN_RegsStruct->MIN_SPEED = 0;
    dSPIN_RegsStruct->FS_SPD = 0x027;

    dSPIN_RegsStruct->KVAL_HOLD = 0x29;
    dSPIN_RegsStruct->KVAL_RUN = 0x29;
    dSPIN_RegsStruct->KVAL_ACC = 0x29;
    dSPIN_RegsStruct->KVAL_DEC = 0x29;
    dSPIN_RegsStruct->INT_SPD = 0x0408;
    dSPIN_RegsStruct->ST_SLP = 0x19;
    dSPIN_RegsStruct->FN_SLP_ACC = 0x29;
    dSPIN_RegsStruct->FN_SLP_DEC = 0x29;
    dSPIN_RegsStruct->K_THERM = 0;
    dSPIN_RegsStruct->STALL_TH = 0x40;

    dSPIN_RegsStruct->OCD_TH = 0x8;
    dSPIN_RegsStruct->STEP_MODE = 0x7;
    dSPIN_RegsStruct->ALARM_EN = 0;
    dSPIN_RegsStruct->CONFIG = 0x2E88;
}

/**
 * @brief  Configures dSPIN internal registers with values in the config structure.
 * @param  dSPIN_RegsStruct Configuration structure address (pointer to configuration structure)
 * @retval None
 */
void dSPIN_Registers_Set(dSPIN_RegsStruct_TypeDef * dSPIN_RegsStruct)
{
    dSPIN_Set_Param(dSPIN_ABS_POS, dSPIN_RegsStruct->ABS_POS);
    dSPIN_Set_Param(dSPIN_EL_POS, dSPIN_RegsStruct->EL_POS);
    dSPIN_Set_Param(dSPIN_MARK, dSPIN_RegsStruct->MARK);
    dSPIN_Set_Param(dSPIN_ACC, dSPIN_RegsStruct->ACC);
    dSPIN_Set_Param(dSPIN_DEC, dSPIN_RegsStruct->DEC);

    dSPIN_Set_Param(dSPIN_MAX_SPEED, dSPIN_RegsStruct->MAX_SPEED); /* WTF 配置此项后需要断开电桥 但软停车不行 */
    dSPIN_Soft_HiZ();
    dSPIN_Set_Param(dSPIN_MIN_SPEED, dSPIN_RegsStruct->MIN_SPEED);
    dSPIN_Set_Param(dSPIN_FS_SPD, dSPIN_RegsStruct->FS_SPD);

    dSPIN_Set_Param(dSPIN_KVAL_HOLD, dSPIN_RegsStruct->KVAL_HOLD);
    dSPIN_Set_Param(dSPIN_KVAL_RUN, dSPIN_RegsStruct->KVAL_RUN);
    dSPIN_Set_Param(dSPIN_KVAL_ACC, dSPIN_RegsStruct->KVAL_ACC);
    dSPIN_Set_Param(dSPIN_KVAL_DEC, dSPIN_RegsStruct->KVAL_DEC);
    dSPIN_Set_Param(dSPIN_INT_SPD, dSPIN_RegsStruct->INT_SPD);
    dSPIN_Set_Param(dSPIN_ST_SLP, dSPIN_RegsStruct->ST_SLP);
    dSPIN_Set_Param(dSPIN_FN_SLP_ACC, dSPIN_RegsStruct->FN_SLP_ACC);
    dSPIN_Set_Param(dSPIN_FN_SLP_DEC, dSPIN_RegsStruct->FN_SLP_DEC);
    dSPIN_Set_Param(dSPIN_K_THERM, dSPIN_RegsStruct->K_THERM);
    dSPIN_Set_Param(dSPIN_STALL_TH, dSPIN_RegsStruct->STALL_TH);

    dSPIN_Set_Param(dSPIN_OCD_TH, dSPIN_RegsStruct->OCD_TH);
    dSPIN_Set_Param(dSPIN_STEP_MODE, dSPIN_RegsStruct->STEP_MODE);
    dSPIN_Set_Param(dSPIN_ALARM_EN, dSPIN_RegsStruct->ALARM_EN);
    dSPIN_Set_Param(dSPIN_CONFIG, dSPIN_RegsStruct->CONFIG);
    //    // ws
    //    dSPIN_Set_Param(dSPIN_ABS_POS, dSPIN_RegsStruct->ABS_POS);
    //    dSPIN_Set_Param(dSPIN_EL_POS, dSPIN_RegsStruct->EL_POS);
    //    // wr
    //    dSPIN_Set_Param(dSPIN_MARK, dSPIN_RegsStruct->MARK);
    //    // r
    //    dSPIN_Set_Param(dSPIN_SPEED, dSPIN_RegsStruct->SPEED);
    //    // ws
    //    dSPIN_Soft_Stop();
    //    //    while(dSPIN_Busy_SW(ctrl));
    //    dSPIN_Set_Param(dSPIN_ACC, dSPIN_RegsStruct->ACC);
    //    dSPIN_Set_Param(dSPIN_DEC, dSPIN_RegsStruct->DEC);
    //    // wr
    //    dSPIN_Set_Param(dSPIN_MAX_SPEED, dSPIN_RegsStruct->MAX_SPEED); /* WTF 配置此项后电机会自启动 */
    //    // ws
    //    dSPIN_Soft_Stop();
    //    //    while(dSPIN_Busy_SW(ctrl));
    //    dSPIN_Set_Param(dSPIN_MIN_SPEED, dSPIN_RegsStruct->MIN_SPEED);
    //    // wr
    //    dSPIN_Set_Param(dSPIN_FS_SPD, dSPIN_RegsStruct->FS_SPD);
    //    dSPIN_Set_Param(dSPIN_KVAL_HOLD, dSPIN_RegsStruct->KVAL_HOLD);
    //    dSPIN_Set_Param(dSPIN_KVAL_RUN, dSPIN_RegsStruct->KVAL_RUN);
    //    dSPIN_Set_Param(dSPIN_KVAL_ACC, dSPIN_RegsStruct->KVAL_ACC);
    //    dSPIN_Set_Param(dSPIN_KVAL_DEC, dSPIN_RegsStruct->KVAL_DEC);
    //    // wh
    //    dSPIN_Soft_HiZ();
    //    //    while(dSPIN_Busy_SW(ctrl));
    //    dSPIN_Set_Param(dSPIN_INT_SPD, dSPIN_RegsStruct->INT_SPD);
    //    dSPIN_Set_Param(dSPIN_ST_SLP, dSPIN_RegsStruct->ST_SLP);
    //    dSPIN_Set_Param(dSPIN_FN_SLP_ACC, dSPIN_RegsStruct->FN_SLP_ACC);
    //    dSPIN_Set_Param(dSPIN_FN_SLP_DEC, dSPIN_RegsStruct->FN_SLP_DEC);
    //    // wr
    //    dSPIN_Set_Param(dSPIN_K_THERM, dSPIN_RegsStruct->K_THERM);
    //    // wr
    //    dSPIN_Set_Param(dSPIN_OCD_TH, dSPIN_RegsStruct->OCD_TH);
    //    dSPIN_Set_Param(dSPIN_STALL_TH, dSPIN_RegsStruct->STALL_TH);
    //    // wh
    //    dSPIN_Soft_HiZ();
    //    //    while(dSPIN_Busy_SW(ctrl));
    //    dSPIN_Set_Param(dSPIN_STEP_MODE, dSPIN_RegsStruct->STEP_MODE);
    //    // ws
    //    dSPIN_Soft_Stop();
    //    //    while(dSPIN_Busy_SW(ctrl));
    //    dSPIN_Set_Param(dSPIN_ALARM_EN, dSPIN_RegsStruct->ALARM_EN);
    //    // wh
    //    dSPIN_Soft_HiZ();
    //    //    while(dSPIN_Busy_SW(ctrl));
    //    dSPIN_Set_Param(dSPIN_CONFIG, dSPIN_RegsStruct->CONFIG);
    //    dSPIN_Soft_Stop();
    //    //    while(dSPIN_Busy_SW(ctrl));
}

/**
 * @brief Issues dSPIN NOP command.
 * @param None
 * @retval None
 */
void dSPIN_Nop(void)
{
    /* Send NOP operation code to dSPIN */
    dSPIN_Write_Byte(dSPIN_NOP);
}

/**
 * @brief  Issues dSPIN Set Param command.
 * @param  param dSPIN register address
 * @param  value to be set
 * @retval None
 */
void dSPIN_Set_Param(dSPIN_Registers_TypeDef param, uint32_t value)
{
    /* Send SetParam operation code to dSPIN */
    dSPIN_Write_Byte((uint8_t)dSPIN_SET_PARAM | (uint8_t)param);
    switch (param) {
        case dSPIN_ABS_POS:;
        case dSPIN_MARK:;
            /* Send parameter - byte 2 to dSPIN */
            dSPIN_Write_Byte((uint8_t)(value >> 16));
        case dSPIN_EL_POS:;
        case dSPIN_ACC:;
        case dSPIN_DEC:;
        case dSPIN_MAX_SPEED:;
        case dSPIN_MIN_SPEED:;
        case dSPIN_FS_SPD:;

        case dSPIN_INT_SPD:;

        case dSPIN_CONFIG:;
        case dSPIN_STATUS:
            /* Send parameter - byte 1 to dSPIN */
            dSPIN_Write_Byte((uint8_t)(value >> 8));
        default:
            /* Send parameter - byte 0 to dSPIN */
            dSPIN_Write_Byte((uint8_t)(value));
    }
}

/**
 * @brief  Issues dSPIN Get Param command.
 * @param  param dSPIN register address
 * @retval Register value - 1 to 3 bytes (depends on register)
 */
uint32_t dSPIN_Get_Param(dSPIN_Registers_TypeDef param)
{
    uint32_t temp = 0;
    uint32_t rx = 0;

    /* Send GetParam operation code to dSPIN */
    temp = dSPIN_Write_Byte((uint8_t)dSPIN_GET_PARAM | (uint8_t)param);
    /* MSB which should be 0 */
    temp = temp << 24;
    rx |= temp;
    switch (param) {
        case dSPIN_ABS_POS:;
        case dSPIN_MARK:;
        case dSPIN_SPEED:
            temp = dSPIN_Write_Byte((uint8_t)(0x00));
            temp = temp << 16;
            rx |= temp;
        case dSPIN_EL_POS:;
        case dSPIN_ACC:;
        case dSPIN_DEC:;
        case dSPIN_MAX_SPEED:;
        case dSPIN_MIN_SPEED:;
        case dSPIN_FS_SPD:;

        case dSPIN_INT_SPD:;

        case dSPIN_CONFIG:;
        case dSPIN_STATUS:
            temp = dSPIN_Write_Byte((uint8_t)(0x00));
            temp = temp << 8;
            rx |= temp;
        default:
            temp = dSPIN_Write_Byte((uint8_t)(0x00));
            rx |= temp;
    }
    return rx;
}

/**
 * @brief  Issues dSPIN Run command.
 * @param  direction Movement direction (FWD, REV)
 * @param  speed over 3 bytes
 * @retval None
 */
void dSPIN_Run(dSPIN_Direction_TypeDef direction, uint32_t speed)
{
    /* Send RUN operation code to dSPIN */
    dSPIN_Write_Byte((uint8_t)dSPIN_RUN | (uint8_t)direction);
    /* Send speed - byte 2 data dSPIN */
    dSPIN_Write_Byte((uint8_t)(speed >> 16));
    /* Send speed - byte 1 data dSPIN */
    dSPIN_Write_Byte((uint8_t)(speed >> 8));
    /* Send speed - byte 0 data dSPIN */
    dSPIN_Write_Byte((uint8_t)(speed));
}

/**
 * @brief  Issues dSPIN Step Clock command.
 * @param  direction Movement direction (FWD, REV)
 * @retval None
 */
void dSPIN_Step_Clock(dSPIN_Direction_TypeDef direction)
{
    /* Send StepClock operation code to dSPIN */
    dSPIN_Write_Byte((uint8_t)dSPIN_STEP_CLOCK | (uint8_t)direction);
}

/**
 * @brief  Issues dSPIN Move command.
 * @param  direction mMovement direction
 * @param  n_step number of steps
 * @retval None
 */
void dSPIN_Move(dSPIN_Direction_TypeDef direction, uint32_t n_step)
{
    /* Send Move operation code to dSPIN */
    dSPIN_Write_Byte((uint8_t)dSPIN_MOVE | (uint8_t)direction);
    /* Send n_step - byte 2 data dSPIN */
    dSPIN_Write_Byte((uint8_t)(n_step >> 16));
    /* Send n_step - byte 1 data dSPIN */
    dSPIN_Write_Byte((uint8_t)(n_step >> 8));
    /* Send n_step - byte 0 data dSPIN */
    dSPIN_Write_Byte((uint8_t)(n_step));
}

/**
 * @brief  Issues dSPIN Go To command.
 * @param  abs_pos absolute position where requested to move
 * @retval None
 */
void dSPIN_Go_To(uint32_t abs_pos)
{
    /* Send GoTo operation code to dSPIN */
    dSPIN_Write_Byte(dSPIN_GO_TO);
    /* Send absolute position parameter - byte 2 data to dSPIN */
    dSPIN_Write_Byte((uint8_t)(abs_pos >> 16));
    /* Send absolute position parameter - byte 1 data to dSPIN */
    dSPIN_Write_Byte((uint8_t)(abs_pos >> 8));
    /* Send absolute position parameter - byte 0 data to dSPIN */
    dSPIN_Write_Byte((uint8_t)(abs_pos));
}

/**
 * @brief  Issues dSPIN Go To Dir command.
 * @param  direction movement direction
 * @param  abs_pos absolute position where requested to move
 * @retval None
 */
void dSPIN_Go_To_Dir(dSPIN_Direction_TypeDef direction, uint32_t abs_pos)
{
    /* Send GoTo_DIR operation code to dSPIN */
    dSPIN_Write_Byte((uint8_t)dSPIN_GO_TO_DIR | (uint8_t)direction);
    /* Send absolute position parameter - byte 2 data to dSPIN */
    dSPIN_Write_Byte((uint8_t)(abs_pos >> 16));
    /* Send absolute position parameter - byte 1 data to dSPIN */
    dSPIN_Write_Byte((uint8_t)(abs_pos >> 8));
    /* Send absolute position parameter - byte 0 data to dSPIN */
    dSPIN_Write_Byte((uint8_t)(abs_pos));
}

/**
 * @brief  Issues dSPIN Go Until command.
 * @param  action
 * @param  direction movement direction
 * @param  speed
 * @retval None
 */
void dSPIN_Go_Until(dSPIN_Action_TypeDef action, dSPIN_Direction_TypeDef direction, uint32_t speed)
{
    /* Send GoUntil operation code to dSPIN */
    dSPIN_Write_Byte((uint8_t)dSPIN_GO_UNTIL | (uint8_t)action | (uint8_t)direction);
    /* Send speed parameter - byte 2 data to dSPIN */
    dSPIN_Write_Byte((uint8_t)(speed >> 16));
    /* Send speed parameter - byte 1 data to dSPIN */
    dSPIN_Write_Byte((uint8_t)(speed >> 8));
    /* Send speed parameter - byte 0 data to dSPIN */
    dSPIN_Write_Byte((uint8_t)(speed));
}

/**
 * @brief  Issues dSPIN Release SW command.
 * @param  action
 * @param  direction movement direction
 * @retval None
 */
void dSPIN_Release_SW(dSPIN_Action_TypeDef action, dSPIN_Direction_TypeDef direction)
{
    /* Send ReleaseSW operation code to dSPIN */
    dSPIN_Write_Byte((uint8_t)dSPIN_RELEASE_SW | (uint8_t)action | (uint8_t)direction);
}

/**
 * @brief  Issues dSPIN Go Home command. (Shorted path to zero position)
 * @param  None
 * @retval None
 */
void dSPIN_Go_Home(void)
{
    /* Send GoHome operation code to dSPIN */
    dSPIN_Write_Byte(dSPIN_GO_HOME);
}

/**
 * @brief  Issues dSPIN Go Mark command.
 * @param  None
 * @retval None
 */
void dSPIN_Go_Mark(void)
{
    /* Send GoMark operation code to dSPIN */
    dSPIN_Write_Byte(dSPIN_GO_MARK);
}

/**
 * @brief  Issues dSPIN Reset Pos command.
 * @param  None
 * @retval None
 */
void dSPIN_Reset_Pos(void)
{
    /* Send ResetPos operation code to dSPIN */
    dSPIN_Write_Byte(dSPIN_RESET_POS);
}

/**
 * @brief  Issues dSPIN Reset Device command.
 * @param  None
 * @retval None
 */
void dSPIN_Reset_Device(void)
{
    /* Send ResetDevice operation code to dSPIN */
    dSPIN_Write_Byte(dSPIN_RESET_DEVICE);
}

/**
 * @brief  Issues dSPIN Soft Stop command.
 * @param  None
 * @retval None
 */
void dSPIN_Soft_Stop(void)
{
    /* Send SoftStop operation code to dSPIN */
    dSPIN_Write_Byte(dSPIN_SOFT_STOP);
}

/**
 * @brief  Issues dSPIN Hard Stop command.
 * @param  None
 * @retval None
 */
void dSPIN_Hard_Stop(void)
{
    /* Send HardStop operation code to dSPIN */
    dSPIN_Write_Byte(dSPIN_HARD_STOP);
}

/**
 * @brief  Issues dSPIN Soft HiZ command.
 * @param  None
 * @retval None
 */
void dSPIN_Soft_HiZ(void)
{
    /* Send SoftHiZ operation code to dSPIN */
    dSPIN_Write_Byte(dSPIN_SOFT_HIZ);
}

/**
 * @brief  Issues dSPIN Hard HiZ command.
 * @param  None
 * @retval None
 */
void dSPIN_Hard_HiZ(void)
{
    /* Send HardHiZ operation code to dSPIN */
    dSPIN_Write_Byte(dSPIN_HARD_HIZ);
}

/**
 * @brief  Issues dSPIN Get Status command.
 * @param  None
 * @retval Status Register content
 */
uint16_t dSPIN_Get_Status(void)
{
    uint16_t temp = 0;
    uint16_t rx = 0;

    /* Send GetStatus operation code to dSPIN */
    dSPIN_Write_Byte(dSPIN_GET_STATUS);
    /* Send zero byte / receive MSByte from dSPIN */
    temp = dSPIN_Write_Byte((uint8_t)(0x00));
    temp = temp << 8;
    rx |= temp;
    /* Send zero byte / receive LSByte from dSPIN */
    temp = dSPIN_Write_Byte((uint8_t)(0x00));
    rx |= temp;
    return rx;
}

/**
 * @brief  Checks if the dSPIN is Busy by hardware - active Busy signal.
 * @param  None
 * @retval one if chip is busy, otherwise zero
 */
uint8_t dSPIN_Busy_HW(void)
{
    if (!(m_l6470_BUSY_GPIO_Get()))
        return 0x01;
    else
        return 0x00;
}

/**
 * @brief  Checks if the dSPIN is Busy by SPI - Busy flag bit in Status Register.
 * @param  None
 * @retval one if chip is busy, otherwise zero
 */
uint8_t dSPIN_Busy_SW(void)
{
    if (!(dSPIN_Get_Status() & dSPIN_STATUS_BUSY))
        return 0x01;
    else
        return 0x00;
}

/**
 * @brief  Checks dSPIN Flag signal.
 * @param  None
 * @retval one if Flag signal is active, otherwise zero
 */
uint8_t dSPIN_Flag(void)
{
    if (!(m_l6470_FLAG_GPIO_Get()))
        return 0x01;
    else
        return 0x00;
}

/**
 * @brief  Transmits/Receives one byte to/from dSPIN over SPI.
 * @param  byte Transmited byte
 * @retval Received byte
 */
uint8_t dSPIN_Write_Byte(uint8_t byte)
{
    uint8_t result = 0xA5;

    /* nSS signal activation - low */
    m_l6470_NCS_GPIO_Active();

    /* SPI byte send and receive */

    if (HAL_SPI_TransmitReceive(&hspi2, &byte, &result, 1, 30) != HAL_OK) {
        result = 0xA5;
    }

    /* nSS signal deactivation - high */
    m_l6470_NCS_GPIO_Deactive();
    return result;
}

/** @} */
/******************* (C) COPYRIGHT 2013 STMicroelectronics *****END OF FILE****/

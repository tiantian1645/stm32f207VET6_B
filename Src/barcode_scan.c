/**
 * @file    barcode_scan.c
 * @brief   扫码电机控制
 *
 * 只有初始位置有一个光耦
 * 扫码电机运动控制流程
 *  1 初始光耦位置作为参照起点
 *      1.1 起始位置处于光耦位置 --> 移动固定步数
 *      1.2 起始位置不处于光耦位置 --> 先移动到光耦位置 --> 再移动到固定步数
 *  2 电机当前运动绝对步数作为参照起点
 *      2.1 起始位置处于光耦位置 --> 设置当前电机运动步数 --> 移动固定步数
 *      2.2 起始位置不处于光耦位置 --> 检查在光耦位置时是否设置过电机运动步数
 *          2.2.1 光耦位置时没记录过电机运动步数 --> 1
 *          2.2.2 根据电机当前运动步数移动步数量
 *
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "motor.h"
#include "serial.h"
#include "comm_out.h"
#include "comm_main.h"
#include "comm_data.h"
#include "protocol.h"
#include "barcode_scan.h"
#include "m_l6470.h"
#include "m_drv8824.h"
#include "tray_run.h"
#include "heat_motor.h"
#include "se2707.h"
#include "beep.h"
#include <string.h>

/* Extern variables ----------------------------------------------------------*/
extern UART_HandleTypeDef huart3;

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/
typedef struct {
    uint32_t tatasi;
    uint32_t qigau;
    uint32_t total;
} sBarcodeStatistic;

/* Private define ------------------------------------------------------------*/
#define BAR_SAM_0 BAR_SAM_LDH_
#define BAR_SAM_1 BAR_SAM_LDH_
#define BAR_SAM_2 BAR_SAM_LDH_
#define BAR_SAM_3 BAR_SAM_LDH_
#define BAR_SAM_4 BAR_SAM_LDH_
#define BAR_SAM_5 BAR_SAM_LDH_
#define BAR_SAM_6 BAR_SAM_LDH_

/* Private macro -------------------------------------------------------------*/
#define BARCODE_MOTOR_IS_OPT (HAL_GPIO_ReadPin(OPTSW_OUT0_GPIO_Port, OPTSW_OUT0_Pin) == GPIO_PIN_RESET) /* 光耦输入 */
#define BARCODE_MOTOR_IS_BUSY (dSPIN_Busy_HW())                                                         /* 扫码电机忙碌位读取 */
#define BARCODE_MOTOR_IS_FLAG (dSPIN_Flag())                                                            /* 扫码电机标志脚读取 */
#define BARCODE_MOTOR_MAX_DISP 16000                                                                    /* 扫码电机运动最大步数 物理限制步数 */
#define BARCODE_UART huart3                                                                             /* 扫码串口 */
#define BARCODE_MOTOR_MAX_GO_UNTIL_SPEED 600000                                                         /* 扫码电机归零最大速度 */

/* Private variables ---------------------------------------------------------*/
static sMotorRunStatus gBarcodeMotorRunStatus;
static sMoptorRunCmdInfo gBarcodeMotorRunCmdInfo;

sBarcoderesult gBarcodeDecodeResult[7];
uint8_t gBarcodeDecodeData_0[BARCODE_QR_LENGTH];
uint8_t gBarcodeDecodeData_1[BARCODE_BA_LENGTH];
uint8_t gBarcodeDecodeData_2[BARCODE_BA_LENGTH];
uint8_t gBarcodeDecodeData_3[BARCODE_BA_LENGTH];
uint8_t gBarcodeDecodeData_4[BARCODE_BA_LENGTH];
uint8_t gBarcodeDecodeData_5[BARCODE_BA_LENGTH];
uint8_t gBarcodeDecodeData_6[BARCODE_BA_LENGTH];

sBarcodeStatistic gBarcodeStatistics[7];

TaskHandle_t barcodeTaskHandle = NULL;

uint8_t gBarcodeIsContinuousScan = 0;

/* Private constants ---------------------------------------------------------*/
const char BAR_SAM_LDH_[] = "1419190801";
const char BAR_SAM_HB__[] = "1415190701";
const char BAR_SAM_AMY_[] = "1411190601";
const char BAR_SAM_UA__[] = "1413190703";
const char BAR_SAM_CREA[] = "1418190602";
const char BAR_SAM_QR__[] = "6882190918202303039503500200020004500560020000000400001170301020000000008";

const uint8_t BAR_CODE_PROTOCOL_CLOSE_SCAN[] = {0x08, 0xC6, 0x04, 0x00, 0xFF, 0xF0, 0x32, 0x00, 0xFD, 0x0D}; //关闭瞄准器
const uint8_t BAR_CODE_PROTOCOL_ACK[] = {0x04, 0xD0, 0x00, 0x00, 0xFF, 0x2C};                                //正常回应
const uint8_t BAR_CODE_PROTOCOL_NAK[] = {0x05, 0xD1, 0x00, 0x00, 0x01, 0xFF, 0x29};                          //异常回应

const eBarcodeIndex cBarCodeIndex[] = {
    eBarcodeIndex_5, eBarcodeIndex_4, eBarcodeIndex_3, eBarcodeIndex_2, eBarcodeIndex_1, eBarcodeIndex_0,
};

/* Private function prototypes -----------------------------------------------*/

/* Private user code ---------------------------------------------------------*/

/**
 * @brief  初始化扫码结果缓存
 * @param  None
 * @retval None
 */
void barcode_Result_Init(void)
{
    gBarcodeDecodeResult[0].pData = gBarcodeDecodeData_0;
    gBarcodeDecodeResult[0].state = eBarcodeState_Error;
    memset(gBarcodeDecodeData_0, 0xA5, ARRAY_LEN(gBarcodeDecodeData_0));

    gBarcodeDecodeResult[1].pData = gBarcodeDecodeData_1;
    gBarcodeDecodeResult[1].state = eBarcodeState_Error;
    memset(gBarcodeDecodeData_1, 0xA5, ARRAY_LEN(gBarcodeDecodeData_1));

    gBarcodeDecodeResult[2].pData = gBarcodeDecodeData_2;
    gBarcodeDecodeResult[2].state = eBarcodeState_Error;
    memset(gBarcodeDecodeData_2, 0xA5, ARRAY_LEN(gBarcodeDecodeData_2));

    gBarcodeDecodeResult[3].pData = gBarcodeDecodeData_3;
    gBarcodeDecodeResult[3].state = eBarcodeState_Error;
    memset(gBarcodeDecodeData_3, 0xA5, ARRAY_LEN(gBarcodeDecodeData_3));

    gBarcodeDecodeResult[4].pData = gBarcodeDecodeData_4;
    gBarcodeDecodeResult[4].state = eBarcodeState_Error;
    memset(gBarcodeDecodeData_4, 0xA5, ARRAY_LEN(gBarcodeDecodeData_4));

    gBarcodeDecodeResult[5].pData = gBarcodeDecodeData_5;
    gBarcodeDecodeResult[5].state = eBarcodeState_Error;
    memset(gBarcodeDecodeData_5, 0xA5, ARRAY_LEN(gBarcodeDecodeData_5));

    gBarcodeDecodeResult[6].pData = gBarcodeDecodeData_6;
    gBarcodeDecodeResult[6].state = eBarcodeState_Error;
    memset(gBarcodeDecodeData_6, 0xA5, ARRAY_LEN(gBarcodeDecodeData_6));
}

/**
 * @brief  扫码电机 读取驱动中记录的步数
 * @param  None
 * @retval 驱动中记录的步数
 */
int32_t barcode_Motor_Read_Position(void)
{
    int32_t position;
    position = (((int32_t)((dSPIN_Get_Param(dSPIN_ABS_POS)) << 10)) >> 10);
    return position * -1;
}

/**
 * @brief  扫码电机停车
 * @param  None
 * @retval 运动结果 0 正常 1 异常
 */
uint8_t barcode_Motor_Brake(void)
{
    dSPIN_Hard_Stop();
    return 0;
}

/**
 * @brief  根据电机驱动状态设置电机状态
 * @note
 * @param  status 电机驱动状态值 dSPIN_STATUS_Masks_TypeDef
 * @retval None
 */
void barcode_Motor_Deal_Status()
{
    uint16_t status;

    status = dSPIN_Get_Status(); /* 读取电机状态 */

    if (((status & dSPIN_STATUS_STEP_LOSS_A) == 0) || ((status & dSPIN_STATUS_STEP_LOSS_B) == 0)) { /* 发生失步 */
        // m_l6470_Reset_HW();                                                                         /* 硬件重置 */
        // m_l6470_Params_Init();                                                                      /* 初始化参数 */
        return;
    }
    if (((status & dSPIN_STATUS_UVLO)) == 0) { /* 低压 */
                                               // m_l6470_Reset_HW();                    /* 硬件重置 */
        // m_l6470_Params_Init();                 /* 初始化参数 */
        return;
    }
    if (((status & dSPIN_STATUS_TH_WRN)) == 0 || ((status & dSPIN_STATUS_TH_SD)) == 0 || ((status & dSPIN_STATUS_OCD)) == 0) { /* 高温 超温 过流 */
        // m_l6470_Reset_HW();                                                                                                    /* 硬件重置 */
        // m_l6470_Params_Init();                                                                                                 /* 初始化参数 */
        return;
    }
    return;
}

/**
 * @brief  电机运动前回调
 * @param  None
 * @retval 运动结果 0 正常 1 异常
 */
eBarcodeState barcode_Motor_Enter(void)
{
    if (m_l6470_Index_Switch(eM_L6470_Index_0, 500) != 0) { /* 获取SPI总线资源 */
        return eBarcodeState_Tiemout;
    }

    if (dSPIN_Flag()) {
        barcode_Motor_Deal_Status();
    }

    if (dSPIN_Busy_HW()) { /* 软件检测忙碌状态 */
        return eBarcodeState_Busy;
    } else {
        return eBarcodeState_OK;
    }
}

/**
 * @brief  电机运动后回调 光耦位置硬件检测停车
 * @note   光耦位置处重置电机状态记录中步数值 重置电机驱动中步数记录值
 * @param  None
 * @retval 运动结果 0 正常 1 运动超时
 */
eBarcodeState barcode_Motor_Leave_On_OPT(void)
{
    while (xTaskGetTickCount() - motor_Status_Get_TickInit(&gBarcodeMotorRunStatus) < motor_CMD_Info_Get_Tiemout(&gBarcodeMotorRunCmdInfo)) { /* 超时等待 */
        if (BARCODE_MOTOR_IS_BUSY == 0 || BARCODE_MOTOR_IS_OPT) { /* 已配置硬件检测停车 电机驱动空闲状态 */
            barcode_Motor_Brake();                                /* 刹车 */
            dSPIN_Reset_Pos();                                    /* 重置电机驱动步数记录 */
            barcode_Motor_Deal_Status();
            motor_Status_Set_Position(&gBarcodeMotorRunStatus, 0); /* 重置电机状态步数记录 */
            return eBarcodeState_OK;
        }
        vTaskDelay(1); /* 延时 */
    }
    barcode_Motor_Brake(); /* 刹车 */
    barcode_Motor_Deal_Status();
    return eBarcodeState_Error; /* 超时返回错误 */
}

/**
 * @brief  电机运动后回调 被动检查停车状态
 * @param  None
 * @retval 运动结果 0 正常 1 异常
 */
eBarcodeState barcode_Motor_Leave_On_Busy_Bit(void)
{
    while (xTaskGetTickCount() - motor_Status_Get_TickInit(&gBarcodeMotorRunStatus) < motor_CMD_Info_Get_Tiemout(&gBarcodeMotorRunCmdInfo)) { /* 超时等待 */
        if (BARCODE_MOTOR_IS_BUSY == 0) {                                                      /* 电机驱动空闲状态 */
            barcode_Motor_Brake();                                                             /* 刹车 */
            motor_Status_Set_Position(&gBarcodeMotorRunStatus, barcode_Motor_Read_Position()); /* 更新电机状态步数记录 */
            return eBarcodeState_OK;
        }
        vTaskDelay(1); /* 延时 */
    }
    motor_Status_Set_Position(&gBarcodeMotorRunStatus, barcode_Motor_Read_Position()); /* 更新电机状态步数记录 */
    return eBarcodeState_Error;                                                        /* 超时返回错误 */
}

/**
 * @brief  扫码电机运动
 * @param  runInfo 运动信息
 * @retval 运动结果 0 正常 1 异常
 */
eBarcodeState barcode_Motor_Run(void)
{
    eBarcodeState result;

    error_Emit(eError_Peripheral_Motor_Scan, ERROR_TYPE_DEBUG);
    if (gBarcodeMotorRunCmdInfo.step == 0) {
        return eBarcodeState_OK;
    }

    result = gBarcodeMotorRunCmdInfo.pfEnter();
    if (result != eBarcodeState_OK) { /* 入口回调 */
        if (result != eBarcodeState_Tiemout) {
            m_l6470_release(); /* 释放SPI总线资源*/
        }
        if (result == eBarcodeState_Busy) {
            error_Emit(eError_Peripheral_Motor_Scan, eError_Motor_Busy);
        } else if (result == eBarcodeState_Tiemout) {
            error_Emit(eError_Peripheral_Motor_Scan, eError_Motor_Timeout);
        }
        return result;
    }

    motor_Status_Set_TickInit(&gBarcodeMotorRunStatus, xTaskGetTickCount()); /* 记录开始时钟脉搏数 */
    if (gBarcodeMotorRunCmdInfo.step < 0xFFFFFF) {
        switch (gBarcodeMotorRunCmdInfo.dir) {
            case eMotorDir_REV:
                dSPIN_Move(FWD, gBarcodeMotorRunCmdInfo.step); /* 向驱动发送指令 */
                break;
            case eMotorDir_FWD:
            default:
                dSPIN_Move(REV, gBarcodeMotorRunCmdInfo.step); /* 向驱动发送指令 */
                break;
        }
    } else {
        dSPIN_Go_Until(ACTION_RESET, FWD, BARCODE_MOTOR_MAX_GO_UNTIL_SPEED);
    }

    result = gBarcodeMotorRunCmdInfo.pfLeave(); /* 出口回调 */
    barcode_Motor_Deal_Status();                /* 读取电机驱动状态清除标志 */
    m_l6470_release();                          /* 释放SPI总线资源*/
    return result;
}

/**
 * @brief  重置电机状态位置
 * @param  timeout 停车等待超时
 * @retval 0 成功 1 失败
 */

uint8_t barcode_Motor_Reset_Pos(void)
{
    eBarcodeState result;
    TickType_t xTick;

    xTick = xTaskGetTickCount();

    while ((!BARCODE_MOTOR_IS_OPT) && xTaskGetTickCount() - xTick < 1000) { /* 检测光耦是否被遮挡 */
        vTaskDelay(1);
    }

    result = barcode_Motor_Enter();
    if (result == eBarcodeState_Tiemout) { /* 获取SPI资源失败 */
        error_Emit(eError_Peripheral_Motor_Scan, eError_Motor_Timeout);
        return 2;
    }
    if (result == eBarcodeState_Busy) { /* 电机忙 */
        barcode_Motor_Brake();          /* 无条件刹车 */
        m_l6470_release();              /* 释放SPI总线资源*/
        error_Emit(eError_Peripheral_Motor_Scan, eError_Motor_Busy);
        return 3;
    }
    barcode_Motor_Brake();                                 /* 刹车 */
    dSPIN_Reset_Pos();                                     /* 重置电机驱动步数记录 */
    m_l6470_release();                                     /* 释放SPI总线资源*/
    motor_Status_Set_Position(&gBarcodeMotorRunStatus, 0); /* 重置电机状态步数记录 */
    return 0;
}

/**
 * @brief  初始化电机位置
 * @param  None
 * @retval 0 成功 1 失败
 */
eBarcodeState barcode_Motor_Init(void)
{
    eBarcodeState result;
    uint32_t cnt = 0;

    error_Emit(eError_Peripheral_Motor_Scan, ERROR_TYPE_DEBUG);
    result = barcode_Motor_Enter();
    if (result != eBarcodeState_OK) { /* 入口回调 */
        if (result != eBarcodeState_Tiemout) {
            m_l6470_release(); /* 释放SPI总线资源*/
        }
        if (result == eBarcodeState_Busy) {
            error_Emit(eError_Peripheral_Motor_Scan, eError_Motor_Busy);
        } else if (result == eBarcodeState_Tiemout) {
            error_Emit(eError_Peripheral_Motor_Scan, eError_Motor_Timeout);
        }
        return result;
    }

    if (BARCODE_MOTOR_IS_OPT) {
        dSPIN_Move(REV, 300);
        while (BARCODE_MOTOR_IS_OPT && ++cnt < 6000000)
            ;
        cnt = 0;
        barcode_Motor_Brake(); /* 刹车 */
    }
    if (BARCODE_MOTOR_IS_FLAG) {
        barcode_Motor_Deal_Status();
    }

    dSPIN_Go_Until(ACTION_RESET, FWD, BARCODE_MOTOR_MAX_GO_UNTIL_SPEED);
    m_l6470_release(); /* 释放SPI总线资源*/
    return eBarcodeState_OK;
}

/**
 * @brief  计算需要移动的方向和步数
 * @param  target_step 目标位置距离初始位置运动的步数
 * @retval 若移动目标位置超过物理限制 返回 1 否则 0
 */
void barcode_Motor_Calculate(uint32_t target_step)
{
    int32_t current_position;

    current_position = motor_Status_Get_Position(&gBarcodeMotorRunStatus);

    if (target_step >= current_position) {
        motor_CMD_Info_Set_Step(&gBarcodeMotorRunCmdInfo, (target_step - current_position));
        motor_CMD_Info_Set_Dir(&gBarcodeMotorRunCmdInfo, eMotorDir_FWD);
    } else {
        motor_CMD_Info_Set_Step(&gBarcodeMotorRunCmdInfo, (current_position - target_step));
        motor_CMD_Info_Set_Dir(&gBarcodeMotorRunCmdInfo, eMotorDir_REV);
    }
}

/**
 * @brief  扫码模块硬件初始化
 * @param  None
 * @retval None
 */
void barcode_sn2707_Init(void)
{
    sSE2707_Image_Capture_Param icParam;
    uint8_t result = 0, error_flag = 0;

    HAL_GPIO_WritePin(BC_AIM_WK_N_GPIO_Port, BC_AIM_WK_N_Pin, GPIO_PIN_RESET);
    HAL_Delay(5);
    HAL_GPIO_WritePin(BC_AIM_WK_N_GPIO_Port, BC_AIM_WK_N_Pin, GPIO_PIN_SET);

    icParam.param = Decode_Aiming_Pattern;
    icParam.data = 0;
    result = se2707_check_param(&BARCODE_UART, &icParam, 2000, 2);    /* 检查参数项 */
    if (result != 0) {                                                /* 参数项不匹配 */
        result = se2707_conf_param(&BARCODE_UART, &icParam, 2000, 2); /* 重新配置参数项 */
    }
    if (result != 0) {
        error_flag = 1;
    }

    icParam.param = Illumination_Brightness;
    icParam.data = 1;
    result = se2707_check_param(&BARCODE_UART, &icParam, 2000, 2);    /* 检查参数项 */
    if (result != 0) {                                                /* 参数项不匹配 */
        result = se2707_conf_param(&BARCODE_UART, &icParam, 2000, 2); /* 重新配置参数项 */
    }
    if (result != 0) {
        error_flag = 1;
    }

    if (error_flag != 0) {                                                 /* 存在错误 */
        error_Emit(eError_Peripheral_Scanner, eError_Scanner_Conf_Failed); /* 报错 */
        beep_Start_With_Conf(eBeep_Freq_re, 100, 100, 1);                  /* RE 一声 */
    } else {                                                               /* 参数项匹配 或 设置成功 */
        beep_Start_With_Conf(eBeep_Freq_mi, 100, 100, 2);                  /* MI 两声 */
    }
}

/**
 * @brief  扫码枪初始化
 * @param  None
 * @retval None
 */
void barcode_Init(void)
{
    barcode_Result_Init(); /* 扫码结果初始化 */
    barcode_sn2707_Init(); /* 扫码模块硬件初始化 */
}

/**
 * @brief  扫码
 * @param  pOut_length 读取到的数据长度
 * @param  pdata   结果存放指针
 * @param  timeout 等待时间
 * @note   串口统一按最大长度24读取
 * @retval 扫码结果
 */
eBarcodeState barcode_Read_From_Serial(uint8_t * pOut_length, uint8_t * pData, uint8_t max_read_length, uint32_t timeout)
{
    HAL_StatusTypeDef status;
    eBarcodeState result;

    HAL_GPIO_WritePin(BC_TRIG_N_GPIO_Port, BC_TRIG_N_Pin, GPIO_PIN_RESET);
    status = HAL_UART_Receive(&BARCODE_UART, pData, max_read_length, pdMS_TO_TICKS(timeout));
    *pOut_length = max_read_length - BARCODE_UART.RxXferCount;
    switch (status) {
        case HAL_TIMEOUT: /* 接收超时 */
            *pOut_length = max_read_length - BARCODE_UART.RxXferCount - 1;
            result = eBarcodeState_Tiemout;
            break;
        case HAL_OK: /* 接收到足够字符串 */
            *pOut_length = max_read_length;
            result = eBarcodeState_OK;
            break;
        default:
            *pOut_length = 0; /* 故障 HAL_ERROR HAL_BUSY */
            result = eBarcodeState_Error;
            break;
    }
    HAL_GPIO_WritePin(BC_TRIG_N_GPIO_Port, BC_TRIG_N_Pin, GPIO_PIN_SET);
    return result;
}

/**
 * @brief  扫码执行 按索引 移动电机
 * @param  index   条码位置索引
 * @retval 扫码结果数据长度
 */
eBarcodeState barcode_Motor_Run_By_Index(eBarcodeIndex index)
{
    motor_CMD_Info_Set_PF_Enter(&gBarcodeMotorRunCmdInfo, barcode_Motor_Enter); /* 配置启动前回调 */
    motor_CMD_Info_Set_Tiemout(&gBarcodeMotorRunCmdInfo, 1500);                 /* 运动超时时间 1500mS */
    if (index != eBarcodeIndex_0) {
        barcode_Motor_Calculate((index >> 5) << 3);                                             /* 计算运动距离 及方向 32细分转8细分 */
        motor_CMD_Info_Set_PF_Leave(&gBarcodeMotorRunCmdInfo, barcode_Motor_Leave_On_Busy_Bit); /* 等待驱动状态位空闲 */
    } else {
        motor_CMD_Info_Set_PF_Leave(&gBarcodeMotorRunCmdInfo, barcode_Motor_Leave_On_OPT); /* 等待驱动状态位空闲 */
        motor_CMD_Info_Set_Step(&gBarcodeMotorRunCmdInfo, 0xFFFFFF);
    }
    return barcode_Motor_Run(); /* 执行电机运动 */
}

/**
 * @brief  扫码执行 按索引操作
 * @param  index   条码位置索引
 * @retval 扫码结果数据长度
 */
eBarcodeState barcode_Scan_By_Index(eBarcodeIndex index)
{
    eBarcodeState result;
    sBarcoderesult * pResult;
    uint8_t max_read_length, idx;

    switch (index) {
        case eBarcodeIndex_0:
            pResult = &(gBarcodeDecodeResult[6]);
            max_read_length = BARCODE_BA_LENGTH;
            idx = 1;
            break;
        case eBarcodeIndex_1:
            pResult = &(gBarcodeDecodeResult[5]);
            max_read_length = BARCODE_BA_LENGTH;
            idx = 2;
            break;
        case eBarcodeIndex_2:
            pResult = &(gBarcodeDecodeResult[4]);
            max_read_length = BARCODE_BA_LENGTH;
            idx = 3;
            break;
        case eBarcodeIndex_3:
            pResult = &(gBarcodeDecodeResult[3]);
            max_read_length = BARCODE_BA_LENGTH;
            idx = 4;
            break;
        case eBarcodeIndex_4:
            pResult = &(gBarcodeDecodeResult[2]);
            max_read_length = BARCODE_BA_LENGTH;
            idx = 5;
            break;
        case eBarcodeIndex_5:
            pResult = &(gBarcodeDecodeResult[1]);
            max_read_length = BARCODE_BA_LENGTH;
            idx = 6;
            break;
        case eBarcodeIndex_6:
            pResult = &(gBarcodeDecodeResult[0]);
            max_read_length = BARCODE_QR_LENGTH;
            idx = 7;
            break;
        default:
            return eBarcodeState_Error;
    }
    result = barcode_Motor_Run_By_Index(index); /* 执行电机运动 */
    if (result != eBarcodeState_OK) {
        pResult->length = 0;
        pResult->state = eBarcodeState_Error;
    } else {
        pResult->state = barcode_Read_From_Serial(&(pResult->length), pResult->pData + 2, max_read_length, 500);
        if (pResult->state != eBarcodeState_Error) { /* 存在有效QR Code */
            pResult->pData[0] = idx;
            pResult->pData[1] = pResult->length;
            if (index != eBarcodeIndex_6 || pResult->length > 0) {
                comm_Main_SendTask_QueueEmitWithBuildCover(eProtocoleRespPack_Client_BARCODE, pResult->pData, pResult->length + 2);
                comm_Out_SendTask_QueueEmitWithModify(pResult->pData, pResult->length + 2 + 7, 0);
            }
        }
    }
    return pResult->state;
}

/**
 * @brief  扫码执行 电机任务调用
 * @param  None
 * @note   Bug 进入此函数前 若电机已经发生位置偏移 则所有结果均不可信
 * @retval 扫码结果
 */
eBarcodeState barcode_Scan_Whole(void)
{
    uint8_t i;
    eBarcodeState result;

    if (barcode_Scan_By_Index(eBarcodeIndex_6) != eBarcodeState_Error) { /* 先扫二维码 */
        if (gBarcodeDecodeResult[0].length > 0) {                        /* 扫码结果非空 */
            return eBarcodeState_OK;                                     /* 提前返回 */
        }
    }

    for (i = 0; i < ARRAY_LEN(cBarCodeIndex); ++i) {      /* 不存在有效QR Code */
        result = barcode_Scan_By_Index(cBarCodeIndex[i]); /* 扫码位置索引倒序 */
        if (result == eBarcodeState_Error) {              /* 扫码电机故障 */
            return eBarcodeState_Error;                   /* 提前返回 */
        }
    }
    return eBarcodeState_OK;
}

/**
 * @brief  扫码枪通讯测试
 * @param  None
 * @retval 0 能通信 1 通信故障
 */
uint8_t barcode_serial_Test(void)
{
    uint8_t buffer[15];
    HAL_StatusTypeDef status;

    //扫描模组通信报文
    const unsigned char Close_Scan[] = {
        //  0x08,0xC6,0x04,0x00,0xFF,0xF0,0x2A,0x00,0xFD,0x15   //关闭大灯
        0x08, 0xC6, 0x04, 0x00, 0xFF, 0xF0, 0x32, 0x00, 0xFD, 0x0D //关闭瞄准器
    };
    //    const unsigned char Open_Scan[] = {
    //        //  0x08,0xC6,0x04,0x00,0xFF,0xF0,0x2A,0x01,0xFD,0x14   //打开大灯
    //        0x08, 0xC6, 0x04, 0x00, 0xFF, 0xF0, 0x32, 0x02, 0xFD, 0x0B //打开瞄准器
    //    };

    if (HAL_UART_Transmit(&BARCODE_UART, (uint8_t *)Close_Scan, ARRAY_LEN(Close_Scan), pdMS_TO_TICKS(10)) != HAL_OK) {
        return 1;
    }
    status = HAL_UART_Receive(&BARCODE_UART, buffer, ARRAY_LEN(buffer), pdMS_TO_TICKS(100));
    switch (status) {
        case HAL_TIMEOUT: /* 接收超时 */
            if (ARRAY_LEN(buffer) - BARCODE_UART.RxXferCount > 1) {
                return 0;
            }
            return 3;
        case HAL_OK: /* 接收到足够字符串 */
            return 0;
        default: /* 故障 HAL_ERROR HAL_BUSY */
            return 2;
    }
    return 2;
}

/**
 * @brief  扫码结果比较
 * @param  pData 输入指针
 * @param  length 输入长度
 * @param  pTarget 期望
 * @retval 0 一致 1 长度不匹配 2 数据不一致
 */
uint8_t barcode_Comp_Result(uint8_t * pData, uint8_t length, const char * pTarget)
{
    if (length != strlen(pTarget)) {
        return 1;
    }
    if (memcmp(pData, pTarget, length) != 0) {
        return 2;
    }
    return 0;
}

/**
 * @brief  扫码测试
 * @param  cnt 测试次数
 * @retval 扫码结果数据长度
 */
void barcode_Test(uint32_t cnt)
{
    uint32_t i;

    for (i = 0; i < ARRAY_LEN(gBarcodeStatistics); ++i) {
        gBarcodeStatistics[i].qigau = 0;
        gBarcodeStatistics[i].tatasi = 0;
        gBarcodeStatistics[i].total = 0;
    }

    heat_Motor_Run(eMotorDir_FWD, 3000);
    while (!heat_Motor_Position_Is_Up() && ++i < 100) {
        vTaskDelay(10);
    }
    if (!heat_Motor_Position_Is_Up()) {
        return;
    }

    if (tray_Move_By_Index(eTrayIndex_1, 3000) != eTrayState_OK) {
        return;
    }

    do {
        if ((barcode_Scan_By_Index(eBarcodeIndex_0) != eBarcodeState_Error) &&
            (barcode_Comp_Result(gBarcodeDecodeResult[6].pData, gBarcodeDecodeResult[6].length, BAR_SAM_6) == 0)) {
            ++gBarcodeStatistics[0].tatasi;
        } else {
            ++gBarcodeStatistics[0].qigau;
        }
        ++gBarcodeStatistics[0].total;

        if ((barcode_Scan_By_Index(eBarcodeIndex_1) != eBarcodeState_Error) &&
            (barcode_Comp_Result(gBarcodeDecodeResult[5].pData, gBarcodeDecodeResult[5].length, BAR_SAM_5) == 0)) {
            ++gBarcodeStatistics[1].tatasi;
        } else {
            ++gBarcodeStatistics[1].qigau;
        }
        ++gBarcodeStatistics[1].total;

        if ((barcode_Scan_By_Index(eBarcodeIndex_2) != eBarcodeState_Error) &&
            (barcode_Comp_Result(gBarcodeDecodeResult[4].pData, gBarcodeDecodeResult[4].length, BAR_SAM_4) == 0)) {
            ++gBarcodeStatistics[2].tatasi;
        } else {
            ++gBarcodeStatistics[2].qigau;
        }
        ++gBarcodeStatistics[2].total;

        if ((barcode_Scan_By_Index(eBarcodeIndex_3) != eBarcodeState_Error) &&
            (barcode_Comp_Result(gBarcodeDecodeResult[3].pData, gBarcodeDecodeResult[3].length, BAR_SAM_3) == 0)) {
            ++gBarcodeStatistics[3].tatasi;
        } else {
            ++gBarcodeStatistics[3].qigau;
        }
        ++gBarcodeStatistics[3].total;

        if ((barcode_Scan_By_Index(eBarcodeIndex_4) != eBarcodeState_Error) &&
            (barcode_Comp_Result(gBarcodeDecodeResult[2].pData, gBarcodeDecodeResult[2].length, BAR_SAM_2) == 0)) {
            ++gBarcodeStatistics[4].tatasi;
        } else {
            ++gBarcodeStatistics[4].qigau;
        }
        ++gBarcodeStatistics[4].total;

        if ((barcode_Scan_By_Index(eBarcodeIndex_5) != eBarcodeState_Error) &&
            (barcode_Comp_Result(gBarcodeDecodeResult[1].pData, gBarcodeDecodeResult[1].length, BAR_SAM_1) == 0)) {
            ++gBarcodeStatistics[5].tatasi;
        } else {
            ++gBarcodeStatistics[5].qigau;
        }
        ++gBarcodeStatistics[5].total;

        if ((barcode_Scan_By_Index(eBarcodeIndex_6) != eBarcodeState_Error) &&
            (barcode_Comp_Result(gBarcodeDecodeResult[0].pData, gBarcodeDecodeResult[0].length, BAR_SAM_0) == 0)) {
            ++gBarcodeStatistics[6].tatasi;
        } else {
            ++gBarcodeStatistics[6].qigau;
        }
        ++gBarcodeStatistics[6].total;
    } while (--cnt > 0);
}

/**
 * @brief  通知扫码
 * @param  mark 扫码掩码
 * @retval 通知结果
 */
uint8_t barcode_Task_Notify(uint32_t mark)
{
    if (barcodeTaskHandle != NULL) {
        if (xTaskNotify(barcodeTaskHandle, mark, eSetValueWithoutOverwrite) == pdPASS) {
            return 0;
        }
    }
    return 1;
}

/**
 * @brief  批次执行扫码
 * @param  mark 扫码掩码
 * @retval None
 */
void barcode_Scan_Bantch(uint32_t mark)
{
    if (mark & (1 << 0)) {
        barcode_Scan_By_Index(eBarcodeIndex_6);
    }
    if (mark & (1 << 1)) {
        barcode_Scan_By_Index(eBarcodeIndex_5);
    }
    if (mark & (1 << 2)) {
        barcode_Scan_By_Index(eBarcodeIndex_4);
    }
    if (mark & (1 << 3)) {
        barcode_Scan_By_Index(eBarcodeIndex_3);
    }
    if (mark & (1 << 4)) {
        barcode_Scan_By_Index(eBarcodeIndex_2);
    }
    if (mark & (1 << 5)) {
        barcode_Scan_By_Index(eBarcodeIndex_1);
    }
    if (mark & (1 << 6)) {
        barcode_Scan_By_Index(eBarcodeIndex_0);
    }
}

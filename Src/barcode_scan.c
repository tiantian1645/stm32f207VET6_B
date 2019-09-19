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
#include "comm_data.h"
#include "barcode_scan.h"
#include "stdio.h"
#include "m_l6470.h"

/* Extern variables ----------------------------------------------------------*/
extern UART_HandleTypeDef huart3;

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/
#define BARCODE_MOTOR_IS_OPT (HAL_GPIO_ReadPin(OPTSW_OUT0_GPIO_Port, OPTSW_OUT0_Pin) == GPIO_PIN_RESET) /* 光耦输入 */
#define BARCODE_MOTOR_IS_BUSY (dSPIN_Busy_HW())                                                         /* 扫码电机忙碌位读取 */
#define BARCODE_MOTOR_IS_FLAG (dSPIN_Flag())                                                            /* 扫码电机标志脚读取 */
#define BARCODE_MOTOR_MAX_DISP 16000                                                                    /* 扫码电机运动最大步数 物理限制步数 */
#define BARCODE_UART huart3

/* Private variables ---------------------------------------------------------*/
static sMotorRunStatus gBarcodeMotorRunStatus;
static sMoptorRunCmdInfo gBarcodeMotorRunCmdInfo;

/* Private function prototypes -----------------------------------------------*/

/* Private user code ---------------------------------------------------------*/

/**
 * @brief  扫码电机 读取驱动中记录的步数
 * @param  None
 * @retval 驱动中记录的步数
 */
int32_t barcode_Motor_Read_Position(void)
{
    int32_t position;
    position = (((int32_t)((dSPIN_Get_Param(dSPIN_ABS_POS)) << 10)) >> 10);
    return position;
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
 * @brief  电机运动前回调
 * @param  None
 * @retval 运动结果 0 正常 1 异常
 */
eBarcodeState barcode_Motor_Enter(void)
{
    if (m_l6470_Index_Switch(eM_L6470_Index_0, 500) != 0) { /* 获取SPI总线资源 */
        return eBarcodeState_Tiemout;
    }

    if (dSPIN_Busy_HW()) { /* 软件检测忙碌状态 */
        return eBarcodeState_Busy;
    } else {
        return eBarcodeState_OK;
    }
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

    result = gBarcodeMotorRunCmdInfo.pfEnter();
    if (result != eBarcodeState_OK) { /* 入口回调 */
        if (result != eBarcodeState_Tiemout) {
            m_l6470_release(); /* 释放SPI总线资源*/
        }
        return result;
    }

    motor_Status_Set_TickInit(&gBarcodeMotorRunStatus, xTaskGetTickCount()); /* 记录开始时钟脉搏数 */

    switch (gBarcodeMotorRunCmdInfo.dir) {
        case eMotorDir_REV:
            dSPIN_Move(REV, gBarcodeMotorRunCmdInfo.step); /* 向驱动发送指令 */
            break;
        case eMotorDir_FWD:
        default:
            dSPIN_Move(FWD, gBarcodeMotorRunCmdInfo.step); /* 向驱动发送指令 */
            break;
    }

    result = gBarcodeMotorRunCmdInfo.pfLeave(); /* 出口回调 */
    barcode_Motor_Deal_Status();                /* 读取电机驱动状态清除标志 */
    m_l6470_release();                          /* 释放SPI总线资源*/
    return result;
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

    result = barcode_Motor_Enter();
    if (result != eBarcodeState_OK) { /* 入口回调 */
        if (result != eBarcodeState_Tiemout) {
            m_l6470_release(); /* 释放SPI总线资源*/
        }
        return result;
    }

    if (BARCODE_MOTOR_IS_OPT) {
        dSPIN_Move(FWD, 300);
        while (BARCODE_MOTOR_IS_OPT && ++cnt < 6000000)
            ;
        cnt = 0;
        barcode_Motor_Brake(); /* 刹车 */
    }
    if (BARCODE_MOTOR_IS_FLAG) {
        barcode_Motor_Deal_Status();
    }

    dSPIN_Go_Until(ACTION_RESET, REV, 7500);
    while (BARCODE_MOTOR_IS_BUSY && ++cnt < 60000000)
        ;
    barcode_Motor_Brake(); /* 刹车 */
    m_l6470_release();     /* 释放SPI总线资源*/
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
        motor_CMD_Info_Set_Step(&gBarcodeMotorRunCmdInfo, (target_step - current_position) % BARCODE_MOTOR_MAX_DISP);
        motor_CMD_Info_Set_Dir(&gBarcodeMotorRunCmdInfo, eMotorDir_FWD);
    } else {
        motor_CMD_Info_Set_Step(&gBarcodeMotorRunCmdInfo, (current_position - target_step) % BARCODE_MOTOR_MAX_DISP);
        motor_CMD_Info_Set_Dir(&gBarcodeMotorRunCmdInfo, eMotorDir_REV);
    }
}

/**
 * @brief  扫码枪初始化
 * @param  None
 * @retval None
 */
void barcode_Init(void)
{
    m_l6470_Init();                                        /* 驱动资源及参数初始化 */
    barcode_Motor_Init();                                  /* 扫码电机位置初始化 */
    dSPIN_Reset_Pos();                                     /* 重置电机驱动步数记录 */
    motor_Status_Set_Position(&gBarcodeMotorRunStatus, 0); /* 重置电机状态步数记录 */
}

/**
 * @brief  扫码
 * @param  pOut_length 读取到的数据长度
 * @param  pdata   结果存放指针
 * @param  timeout 等待时间
 * @note   串口统一按最大长度24读取
 * @retval 扫码结果
 */
eBarcodeState barcode_Read_From_Serial(uint8_t * pOut_length, uint8_t * pData, uint32_t timeout)
{
    HAL_StatusTypeDef status;
    eBarcodeState result;

    HAL_GPIO_WritePin(BC_AIM_WK_N_GPIO_Port, BC_AIM_WK_N_Pin, GPIO_PIN_RESET);
    vTaskDelay(1);
    HAL_GPIO_WritePin(BC_TRIG_N_GPIO_Port, BC_TRIG_N_Pin, GPIO_PIN_RESET);
    status = HAL_UART_Receive(&BARCODE_UART, pData, BARCODE_MAX_LENGTH, pdMS_TO_TICKS(timeout));
    *pOut_length = BARCODE_MAX_LENGTH - BARCODE_UART.RxXferCount;
    switch (status) {
        case HAL_TIMEOUT: /* 接收超时 */
            *pOut_length = BARCODE_MAX_LENGTH - BARCODE_UART.RxXferCount - 1;
            result = eBarcodeState_Tiemout;
            break;
        case HAL_OK: /* 接收到足够字符串 */
            *pOut_length = BARCODE_MAX_LENGTH;
            result = eBarcodeState_OK;
            break;
        default:
            *pOut_length = 0; /* 故障 HAL_ERROR HAL_BUSY */
            result = eBarcodeState_Error;
            break;
    }
    HAL_GPIO_WritePin(BC_AIM_WK_N_GPIO_Port, BC_AIM_WK_N_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(BC_TRIG_N_GPIO_Port, BC_TRIG_N_Pin, GPIO_PIN_SET);
    return result;
}

/**
 * @brief  扫码执行
 * @param  index   条码位置索引
 * @param  pdata   结果存放指针
 * @param  length  最大结果长度
 * @param  timeout 串口接收等待时间
 * @retval 扫码结果数据长度
 */
eBarcodeState barcode_Scan_By_Index(eBarcodeIndex index, uint8_t * pOut_length, uint8_t * pData, uint32_t timeout)
{
    eBarcodeState result;

    motor_CMD_Info_Set_PF_Enter(&gBarcodeMotorRunCmdInfo, barcode_Motor_Enter);             /* 配置启动前回调 */
    barcode_Motor_Calculate((index >> 5) << 3);                                             /* 计算运动距离 及方向 */
    motor_CMD_Info_Set_Tiemout(&gBarcodeMotorRunCmdInfo, timeout);                          /* 运动超时时间 1000mS */
    motor_CMD_Info_Set_PF_Leave(&gBarcodeMotorRunCmdInfo, barcode_Motor_Leave_On_Busy_Bit); /* 等待驱动状态位空闲 */

    result = barcode_Motor_Run();                                 /* 执行电机运动 */
                                                                  //    if (result != eBarcodeState_OK) {
                                                                  //        return eBarcodeState_Error;
                                                                  //    }
    return barcode_Read_From_Serial(pOut_length, pData, timeout); /* 开始扫码 */
}

void barcode_serial_Test(void)
{
    uint8_t buffer[24];
    //扫描模组通信报文
    const unsigned char Close_Scan[] = {
        //  0x08,0xC6,0x04,0x00,0xFF,0xF0,0x2A,0x00,0xFD,0x15   //关闭大灯
        0x08, 0xC6, 0x04, 0x00, 0xFF, 0xF0, 0x32, 0x00, 0xFD, 0x0D //关闭瞄准器
    };
    const unsigned char Open_Scan[] = {
        //  0x08,0xC6,0x04,0x00,0xFF,0xF0,0x2A,0x01,0xFD,0x14   //打开大灯
        0x08, 0xC6, 0x04, 0x00, 0xFF, 0xF0, 0x32, 0x02, 0xFD, 0x0B //打开瞄准器
    };

    HAL_UART_Transmit(&BARCODE_UART, Close_Scan, ARRAY_LEN(Close_Scan), pdMS_TO_TICKS(10));
    HAL_UART_Receive(&BARCODE_UART, buffer, ARRAY_LEN(buffer), pdMS_TO_TICKS(1000));
}

/**
 * @brief  扫码测试
 * @param  cnt     测试次数
 * @retval None
 */
void barcde_Test(uint32_t cnt)
{
    uint8_t test_buff[BARCODE_MAX_LENGTH];
    uint8_t length;

    while (cnt > 0) {
        test_buff[0] = 0;                                                     /* 发送包头设置为索引值 */
        barcode_Scan_By_Index(eBarcodeIndex_0, &length, test_buff + 1, 2000); /* 条码位置 1 */
        serialSendStartDMA(eSerialIndex_5, test_buff, length + 1, 10);        /* 发送结果 */
        test_buff[0] = 1;                                                     /* 发送包头设置为索引值 */
        barcode_Scan_By_Index(eBarcodeIndex_1, &length, test_buff + 1, 2000); /* 条码位置 2 */
        serialSendStartDMA(eSerialIndex_5, test_buff, length + 1, 10);        /* 发送结果 */
        test_buff[0] = 2;                                                     /* 发送包头设置为索引值 */
        barcode_Scan_By_Index(eBarcodeIndex_2, &length, test_buff + 1, 2000); /* 条码位置 3 */
        serialSendStartDMA(eSerialIndex_5, test_buff, length + 1, 10);        /* 发送结果 */
        test_buff[0] = 3;                                                     /* 发送包头设置为索引值 */
        barcode_Scan_By_Index(eBarcodeIndex_3, &length, test_buff + 1, 2000); /* 条码位置 4 */
        serialSendStartDMA(eSerialIndex_5, test_buff, length + 1, 10);        /* 发送结果 */
        test_buff[0] = 4;                                                     /* 发送包头设置为索引值 */
        barcode_Scan_By_Index(eBarcodeIndex_4, &length, test_buff + 1, 2000); /* 条码位置 5 */
        serialSendStartDMA(eSerialIndex_5, test_buff, length + 1, 10);        /* 发送结果 */
        test_buff[0] = 5;                                                     /* 发送包头设置为索引值 */
        barcode_Scan_By_Index(eBarcodeIndex_5, &length, test_buff + 1, 2000); /* 条码位置 6 */
        serialSendStartDMA(eSerialIndex_5, test_buff, length + 1, 10);        /* 发送结果 */
        --cnt;
    }
    if (comm_Data_DMA_TX_Enter(200) == pdPASS) { /* 等待最后一包发出去 避免系统回收 test_buff */
        comm_Data_DMA_TX_Error();
    }
}

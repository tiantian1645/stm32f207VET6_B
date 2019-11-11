/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "comm_out.h"
#include "comm_main.h"
#include "i2c_eeprom.h"
#include "spi_flash.h"
#include "protocol.h"
#include "soft_timer.h"

/* Private includes ----------------------------------------------------------*/
#include "storge_task.h"

/* Private define ------------------------------------------------------------*/
#define STORGE_FLASH_PART_NUM 224  /* Flash 最大单次操作数据长度 */
#define STORGE_EEPROM_PART_NUM 224 /* EEPROM 最大单次操作数据长度 */

#if STORGE_FLASH_PART_NUM > STORGE_EEPROM_PART_NUM
#define STORGE_BUFF_LEN STORGE_FLASH_PART_NUM
#else
#define STORGE_BUFF_LEN STORGE_EEPROM_PART_NUM
#endif

#define STORGE_DEAL_MASK                                                                                                                                       \
    (eStorgeNotifyConf_Read_Falsh + eStorgeNotifyConf_Write_Falsh + eStorgeNotifyConf_Load_Parmas + eStorgeNotifyConf_Dump_Params +                            \
     eStorgeNotifyConf_Read_ID_Card + eStorgeNotifyConf_Write_ID_Card)

#define STORGE_APP_PARAMS_ADDR (0x1000) /* Sector 1 */
/* Private typedef -----------------------------------------------------------*/
typedef struct {
    uint32_t addr;                   /* 操作地址 */
    uint32_t num;                    /* 操作数量 */
    uint8_t buffer[STORGE_BUFF_LEN]; /* 准备写入的内容 */
} sStorgeTaskQueueInfo;

typedef union {
    float f32;
    uint32_t u32;
    uint16_t u16s[2];
    uint8_t u8s[4];
} uStorgeParamItem;

/* Private macro -------------------------------------------------------------*/

/* Private constants ---------------------------------------------------------*/
const sStorgeParamLimitUnit cStorgeParamLimits[eStorgeParamIndex_Num] = {
    {.default_ = 0, .min = -5, .max = 5}, {.default_ = 0, .min = -5, .max = 5}, {.default_ = 0, .min = -5, .max = 5},
    {.default_ = 0, .min = -5, .max = 5}, {.default_ = 0, .min = -5, .max = 5}, {.default_ = 0, .min = -5, .max = 5},
    {.default_ = 0, .min = -5, .max = 5}, {.default_ = 0, .min = -5, .max = 5}, {.default_ = 0, .min = -5, .max = 5},
};

/* Private variables ---------------------------------------------------------*/
static sStorgeTaskQueueInfo gStorgeTaskInfo;
static TaskHandle_t storgeTaskHandle = NULL;
static uint8_t gStorgeTaskInfoLock = 0;
static sStorgeParamInfo gStorgeParamInfo;

/* Private function prototypes -----------------------------------------------*/
static void storgeTask(void * argument);
static void storge_ParamInit(void);
static uint8_t storge_ParamDump(void);
static uint8_t storge_ParamCheck(uint8_t * pBuff, uint16_t length);
static uint8_t storge_ParamLoad(uint8_t * pBuff);

/* Private user code ---------------------------------------------------------*/

/**
 * @brief  存储任务运行参数 修改锁 释放
 * @param  None
 * @retval None
 */
void gStorgeTaskInfoLockRelease(void)
{
    gStorgeTaskInfoLock = 0; /* 解锁 */
}

/**
 * @brief  存储任务运行参数 修改锁 获取
 * @param  None
 * @retval 0 成功 1 失败
 */
uint8_t gStorgeTaskInfoLockAcquire(void)
{
    if (gStorgeTaskInfoLock == 0) { /* 未上锁 */
        gStorgeTaskInfoLock = 1;    /* 上锁 */
        return 0;
    }
    return 1; /* 未解锁 */
}

/**
 * @brief  存储任务运行参数
 * @param  addr        操作地址
 * @param  pIn         写操作时数据源指针
 * @param  num         操作长度
 * @param  timeout     信号量等待时间
 * @retval None
 */
uint8_t storgeReadConfInfo(uint32_t addr, uint32_t num, uint32_t timeout)
{
    do {
        if (gStorgeTaskInfoLockAcquire() == 0) {
            gStorgeTaskInfo.addr = addr;
            gStorgeTaskInfo.num = num;
            return 0;
        }
        vTaskDelay(1);
    } while (--timeout);

    return 2; /* 错误码2 超时 */
}

/**
 * @brief  存储任务运行参数
 * @param  addr        操作地址
 * @param  pIn         写操作时数据源指针
 * @param  num         操作长度
 * @param  timeout     信号量等待时间
 * @retval None
 */

uint8_t storgeWriteConfInfo(uint32_t addr, uint8_t * pIn, uint32_t num, uint32_t timeout)
{
    if (pIn == NULL ||                             /* 目标指针为空 或者 */
        num > ARRAY_LEN(gStorgeTaskInfo.buffer)) { /* 长度超过缓冲容量 */
        return 1;                                  /* 错误码1 */
    }

    do {
        if (gStorgeTaskInfoLockAcquire() == 0) {
            gStorgeTaskInfo.addr = addr;
            gStorgeTaskInfo.num = num;
            memcpy(gStorgeTaskInfo.buffer, pIn, num);
            return 0;
        }
        vTaskDelay(1);
    } while (--timeout);

    return 2; /* 错误码2 超时 */
}

/**
 * @brief  存储任务启动通知
 * @param  hw          存储类型
 * @param  rw          操作类型
 * @retval 任务通知结果
 */
BaseType_t storgeTaskNotification(eStorgeNotifyConf type, eProtocol_COMM_Index index)
{
    uint32_t notifyValue = 0;

    notifyValue = type;

    if (index == eComm_Out) {
        notifyValue |= eStorgeNotifyConf_COMM_Out;
    }
    if (index == eComm_Main) {
        notifyValue |= eStorgeNotifyConf_COMM_Main;
    }
    return xTaskNotify(storgeTaskHandle, notifyValue, eSetValueWithoutOverwrite);
}

/**
 * @brief  存储数据发送任务初始化
 * @param  argument    None
 * @retval None
 */
void storgeTaskInit(void)
{
    if (xTaskCreate(storgeTask, "StorgeTask", 192, NULL, TASK_PRIORITY_STORGE, &storgeTaskHandle) != pdPASS) {
        FL_Error_Handler(__FILE__, __LINE__);
    }
}

/**
 * @brief  存储数据发送任务
 * @param  argument    None
 * @retval None
 */
static void storgeTask(void * argument)
{
    BaseType_t xResult;
    uint32_t i, readCnt;
    uint32_t wroteCnt;
    uint32_t ulNotifyValue;
    uint16_t length;
    uint8_t buff[256], result;

    bsp_spi_FlashInit();
    storge_ParamInit();
    result = storge_ParamLoad(buff);
    if (result == 1) {
        error_Emit(eError_Peripheral_Storge_Flash, eError_Storge_Param_Overload); /* 参数越限 */
    } else if (result == 2) {
        error_Emit(eError_Peripheral_Storge_Flash, eError_Storge_Read_Error); /* 读取失败 */
    }

    for (;;) {
        xResult = xTaskNotifyWait(0x00, 0xFFFFFFFF, &ulNotifyValue, portMAX_DELAY);
        if (xResult != pdPASS) {
            continue;
        }
        protocol_Temp_Upload_Pause(); /* 暂停温度上送 */
        switch (ulNotifyValue & STORGE_DEAL_MASK) {
            case eStorgeNotifyConf_Read_Falsh:
                for (i = 0; i < ((gStorgeTaskInfo.num + STORGE_FLASH_PART_NUM - 1) / STORGE_FLASH_PART_NUM); ++i) {
                    readCnt =
                        (gStorgeTaskInfo.num >= STORGE_FLASH_PART_NUM * (i + 1)) ? (STORGE_FLASH_PART_NUM) : (gStorgeTaskInfo.num % STORGE_FLASH_PART_NUM);
                    length = spi_FlashReadBuffer(gStorgeTaskInfo.addr + STORGE_FLASH_PART_NUM * i, buff + 7, readCnt);
                    if (length == 0) {
                        buff[0] = 0; /* 信息总长度 小端模式 */
                        buff[1] = 0; /* 信息总长度 小端模式 */
                        buff[2] = 0; /* 信息总长度 小端模式 */
                    } else {
                        buff[0] = gStorgeTaskInfo.num & 0xFF; /* 信息总长度 小端模式 */
                        buff[1] = gStorgeTaskInfo.num >> 8;   /* 信息总长度 小端模式 */
                        buff[2] = gStorgeTaskInfo.num >> 16;  /* 信息总长度 小端模式 */
                    }
                    buff[3] = (gStorgeTaskInfo.addr + STORGE_EEPROM_PART_NUM * i) & 0xFF; /* 地址信息 小端模式 */
                    buff[4] = (gStorgeTaskInfo.addr + STORGE_EEPROM_PART_NUM * i) >> 8;   /* 地址信息 小端模式 */
                    buff[5] = (gStorgeTaskInfo.addr + STORGE_EEPROM_PART_NUM * i) >> 16;  /* 地址信息 小端模式 */
                    buff[6] = length;                                                     /* 数据长度 小端模式 */
                    if (ulNotifyValue & eStorgeNotifyConf_COMM_Out) {
                        xResult = comm_Out_SendTask_QueueEmitWithBuildCover(0xD1, buff, length + 7);
                        if (xResult != pdPASS) {
                            break;
                        }
                    }
                    if (ulNotifyValue & eStorgeNotifyConf_COMM_Main) {
                        xResult = comm_Main_SendTask_QueueEmitWithBuildCover(0xD1, buff, length + 7);
                        if (xResult != pdPASS) {
                            break;
                        }
                    }
                }
                break;
            case eStorgeNotifyConf_Write_Falsh:
                wroteCnt = spi_FlashWriteBuffer(gStorgeTaskInfo.addr, gStorgeTaskInfo.buffer, gStorgeTaskInfo.num);
                if (wroteCnt != gStorgeTaskInfo.num) {
                    buff[0] = 1;
                } else {
                    buff[0] = 0;
                }
                if (ulNotifyValue & eStorgeNotifyConf_COMM_Out) {
                    xResult = comm_Out_SendTask_QueueEmitWithBuildCover(0xD2, buff, 1);
                    if (xResult != pdPASS) {
                        break;
                    }
                }
                if (ulNotifyValue & eStorgeNotifyConf_COMM_Main) {
                    xResult = comm_Main_SendTask_QueueEmitWithBuildCover(0xD2, buff, 1);
                    if (xResult != pdPASS) {
                        break;
                    }
                }
                break;
            case eStorgeNotifyConf_Read_ID_Card:
                for (i = 0; i < ((gStorgeTaskInfo.num + STORGE_EEPROM_PART_NUM - 1) / STORGE_EEPROM_PART_NUM); ++i) {
                    readCnt =
                        (gStorgeTaskInfo.num >= STORGE_EEPROM_PART_NUM * (i + 1)) ? (STORGE_EEPROM_PART_NUM) : (gStorgeTaskInfo.num % STORGE_EEPROM_PART_NUM);
                    length = I2C_EEPROM_Read(gStorgeTaskInfo.addr + STORGE_EEPROM_PART_NUM * i, buff + 5, readCnt, 1000);
                    if (length == 0) {
                        buff[0] = 0; /* 信息总长度 小端模式 */
                        buff[1] = 0; /* 信息总长度 小端模式 */
                    } else {
                        buff[0] = gStorgeTaskInfo.num & 0xFF; /* 信息总长度 小端模式 */
                        buff[1] = gStorgeTaskInfo.num >> 8;   /* 信息总长度 小端模式 */
                    }
                    buff[2] = (gStorgeTaskInfo.addr + STORGE_EEPROM_PART_NUM * i) & 0xFF; /* 地址信息 小端模式 */
                    buff[3] = (gStorgeTaskInfo.addr + STORGE_EEPROM_PART_NUM * i) >> 8;   /* 地址信息 小端模式 */
                    buff[4] = length;                                                     /* 数据长度 小端模式 */
                    if (ulNotifyValue & eStorgeNotifyConf_COMM_Out) {
                        xResult = comm_Out_SendTask_QueueEmitWithBuildCover(eProtocoleRespPack_Client_ID_CARD, buff, length + 5);
                        if (xResult != pdPASS) {
                            break;
                        }
                    }
                    if (ulNotifyValue & eStorgeNotifyConf_COMM_Main) {
                        xResult = comm_Main_SendTask_QueueEmitWithBuildCover(eProtocoleRespPack_Client_ID_CARD, buff, length + 5);
                        if (xResult != pdPASS) {
                            break;
                        }
                    }
                    if (length == 0) {
                        break;
                    }
                }
                break;
            case eStorgeNotifyConf_Write_ID_Card:
                wroteCnt = I2C_EEPROM_Write(gStorgeTaskInfo.addr, gStorgeTaskInfo.buffer, gStorgeTaskInfo.num, 30);
                if (wroteCnt != gStorgeTaskInfo.num) {
                    buff[0] = 1;
                } else {
                    buff[0] = 0;
                }
                if (ulNotifyValue & eStorgeNotifyConf_COMM_Out) {
                    xResult = comm_Out_SendTask_QueueEmitWithBuildCover(0xD4, buff, 1);
                    if (xResult != pdPASS) {
                        break;
                    }
                }
                if (ulNotifyValue & eStorgeNotifyConf_COMM_Main) {
                    xResult = comm_Main_SendTask_QueueEmitWithBuildCover(0xD4, buff, 1);
                    if (xResult != pdPASS) {
                        break;
                    }
                }
                break;
            case eStorgeNotifyConf_Load_Parmas:
                result = storge_ParamLoad(buff);
                if (result == 1) {
                    error_Emit(eError_Peripheral_Storge_Flash, eError_Storge_Param_Overload); /* 参数越限 */
                } else if (result == 2) {
                    error_Emit(eError_Peripheral_Storge_Flash, eError_Storge_Read_Error); /* 读取失败 */
                }
                break;
            case eStorgeNotifyConf_Dump_Params:
                if (storge_ParamDump() > 0) {
                    error_Emit(eError_Peripheral_Storge_Flash, eError_Storge_Write_Error); /* 写入失败 */
                }
                break;
            default:
                break;
        }
        gStorgeTaskInfoLockRelease();  /* 解锁 */
        protocol_Temp_Upload_Resume(); /* 恢复温度上送 */
    }
}

/**
 * @brief  参数初始化
 * @param  None
 * @retval None
 */
static void storge_ParamInit(void)
{
    gStorgeParamInfo.temperature_cc_top_1 = cStorgeParamLimits[eStorgeParamIndex_Temp_CC_top_1].default_;
    gStorgeParamInfo.temperature_cc_top_2 = cStorgeParamLimits[eStorgeParamIndex_Temp_CC_top_2].default_;
    gStorgeParamInfo.temperature_cc_top_3 = cStorgeParamLimits[eStorgeParamIndex_Temp_CC_top_3].default_;
    gStorgeParamInfo.temperature_cc_top_4 = cStorgeParamLimits[eStorgeParamIndex_Temp_CC_top_4].default_;
    gStorgeParamInfo.temperature_cc_top_5 = cStorgeParamLimits[eStorgeParamIndex_Temp_CC_top_5].default_;
    gStorgeParamInfo.temperature_cc_top_6 = cStorgeParamLimits[eStorgeParamIndex_Temp_CC_top_6].default_;
    gStorgeParamInfo.temperature_cc_btm_1 = cStorgeParamLimits[eStorgeParamIndex_Temp_CC_btm_1].default_;
    gStorgeParamInfo.temperature_cc_btm_2 = cStorgeParamLimits[eStorgeParamIndex_Temp_CC_btm_2].default_;
    gStorgeParamInfo.temperature_cc_env = cStorgeParamLimits[eStorgeParamIndex_Temp_CC_env].default_;
}

/**
 * @brief  参数写入Flash
 * @note   无回读
 * @param  None
 * @retval 0 写入成功 1 写入失败
 */
static uint8_t storge_ParamDump(void)
{
    uint16_t wroteCnt;
    wroteCnt = spi_FlashWriteBuffer(STORGE_APP_PARAMS_ADDR, (uint8_t *)(&gStorgeParamInfo), sizeof(gStorgeParamInfo));
    return (wroteCnt == sizeof(gStorgeParamInfo)) ? (0) : (1);
}

/**
 * @brief  参数写入Flash
 * @param  pBuff 数据指针
 * @param  length 数据长度
 * @retval 0 不需要回写 1 参数越限 2 数据异常
 */
static uint8_t storge_ParamCheck(uint8_t * pBuff, uint16_t length)
{
    sStorgeParamInfo * pParam;
    uint8_t error = 0;

    if (length != sizeof(gStorgeParamInfo) || pBuff == NULL) {
        return 2;
    }
    pParam = (sStorgeParamInfo *)(pBuff);

    if (pParam->temperature_cc_top_1 <= cStorgeParamLimits[eStorgeParamIndex_Temp_CC_top_1].max && /* 低于最大值 */
        pParam->temperature_cc_top_1 >= cStorgeParamLimits[eStorgeParamIndex_Temp_CC_top_1].min) { /* 高于最小值 */
        if (gStorgeParamInfo.temperature_cc_top_1 != pParam->temperature_cc_top_1) {               /* 若于原值不相等 */
            gStorgeParamInfo.temperature_cc_top_1 = pParam->temperature_cc_top_1;                  /* 修改原值 */
        }
    } else {
        error |= 1; /* 置位修改标志 存在错误 回写现存值 */
    }
    if (pParam->temperature_cc_top_2 <= cStorgeParamLimits[eStorgeParamIndex_Temp_CC_top_2].max && /* 低于最大值 */
        pParam->temperature_cc_top_2 >= cStorgeParamLimits[eStorgeParamIndex_Temp_CC_top_2].min) { /* 高于最小值 */
        if (gStorgeParamInfo.temperature_cc_top_2 != pParam->temperature_cc_top_2) {               /* 若于原值不相等 */
            gStorgeParamInfo.temperature_cc_top_2 = pParam->temperature_cc_top_2;                  /* 修改原值 */
        }
    } else {
        error |= 1; /* 置位修改标志 存在错误 回写现存值 */
    }
    if (pParam->temperature_cc_top_3 <= cStorgeParamLimits[eStorgeParamIndex_Temp_CC_top_3].max && /* 低于最大值 */
        pParam->temperature_cc_top_3 >= cStorgeParamLimits[eStorgeParamIndex_Temp_CC_top_3].min) { /* 高于最小值 */
        if (gStorgeParamInfo.temperature_cc_top_3 != pParam->temperature_cc_top_3) {               /* 若于原值不相等 */
            gStorgeParamInfo.temperature_cc_top_3 = pParam->temperature_cc_top_3;                  /* 修改原值 */
        }
    } else {
        error |= 1; /* 置位修改标志 存在错误 回写现存值 */
    }
    if (pParam->temperature_cc_top_4 <= cStorgeParamLimits[eStorgeParamIndex_Temp_CC_top_4].max && /* 低于最大值 */
        pParam->temperature_cc_top_4 >= cStorgeParamLimits[eStorgeParamIndex_Temp_CC_top_4].min) { /* 高于最小值 */
        if (gStorgeParamInfo.temperature_cc_top_4 != pParam->temperature_cc_top_4) {               /* 若于原值不相等 */
            gStorgeParamInfo.temperature_cc_top_4 = pParam->temperature_cc_top_4;                  /* 修改原值 */
        }
    } else {
        error |= 1; /* 置位修改标志 存在错误 回写现存值 */
    }
    if (pParam->temperature_cc_top_5 <= cStorgeParamLimits[eStorgeParamIndex_Temp_CC_top_5].max && /* 低于最大值 */
        pParam->temperature_cc_top_5 >= cStorgeParamLimits[eStorgeParamIndex_Temp_CC_top_5].min) { /* 高于最小值 */
        if (gStorgeParamInfo.temperature_cc_top_5 != pParam->temperature_cc_top_5) {               /* 若于原值不相等 */
            gStorgeParamInfo.temperature_cc_top_5 = pParam->temperature_cc_top_5;                  /* 修改原值 */
        }
    } else {
        error |= 1; /* 置位修改标志 存在错误 回写现存值 */
    }
    if (pParam->temperature_cc_top_6 <= cStorgeParamLimits[eStorgeParamIndex_Temp_CC_top_6].max && /* 低于最大值 */
        pParam->temperature_cc_top_6 >= cStorgeParamLimits[eStorgeParamIndex_Temp_CC_top_6].min) { /* 高于最小值 */
        if (gStorgeParamInfo.temperature_cc_top_6 != pParam->temperature_cc_top_6) {               /* 若于原值不相等 */
            gStorgeParamInfo.temperature_cc_top_6 = pParam->temperature_cc_top_6;                  /* 修改原值 */
        }
    } else {
        error |= 1; /* 置位修改标志 存在错误 回写现存值 */
    }
    if (pParam->temperature_cc_btm_1 <= cStorgeParamLimits[eStorgeParamIndex_Temp_CC_btm_1].max && /* 低于最大值 */
        pParam->temperature_cc_btm_1 >= cStorgeParamLimits[eStorgeParamIndex_Temp_CC_btm_1].min) { /* 高于最小值 */
        if (gStorgeParamInfo.temperature_cc_btm_1 != pParam->temperature_cc_btm_1) {               /* 若于原值不相等 */
            gStorgeParamInfo.temperature_cc_btm_1 = pParam->temperature_cc_btm_1;                  /* 修改原值 */
        }
    } else {
        error |= 1; /* 置位修改标志 存在错误 回写现存值 */
    }
    if (pParam->temperature_cc_btm_2 <= cStorgeParamLimits[eStorgeParamIndex_Temp_CC_btm_2].max && /* 低于最大值 */
        pParam->temperature_cc_btm_2 >= cStorgeParamLimits[eStorgeParamIndex_Temp_CC_btm_2].min) { /* 高于最小值 */
        if (gStorgeParamInfo.temperature_cc_btm_2 != pParam->temperature_cc_btm_2) {               /* 若于原值不相等 */
            gStorgeParamInfo.temperature_cc_btm_2 = pParam->temperature_cc_btm_2;                  /* 修改原值 */
        }
    } else {
        error |= 1; /* 置位修改标志 存在错误 回写现存值 */
    }
    if (pParam->temperature_cc_env <= cStorgeParamLimits[eStorgeParamIndex_Temp_CC_env].max && /* 低于最大值 */
        pParam->temperature_cc_env >= cStorgeParamLimits[eStorgeParamIndex_Temp_CC_env].min) { /* 高于最小值 */
        if (gStorgeParamInfo.temperature_cc_env != pParam->temperature_cc_env) {               /* 若于原值不相等 */
            gStorgeParamInfo.temperature_cc_env = pParam->temperature_cc_env;                  /* 修改原值 */
        }
    } else {
        error |= 1; /* 置位修改标志 存在错误 回写现存值 */
    }
    return error;
}

/**
 * @brief  参数从Flash读出
 * @param  pBuff 数据缓冲
 * @retval 0 成功 1 参数越限 2 读取失败
 */
static uint8_t storge_ParamLoad(uint8_t * pBuff)
{
    uint16_t readCnt, length;

    readCnt = sizeof(gStorgeParamInfo);
    length = spi_FlashReadBuffer(STORGE_APP_PARAMS_ADDR, pBuff, readCnt);
    if (length == readCnt) {
        if (storge_ParamCheck(pBuff, length) == 0) {
            return 0;
        }
        return 1;
    }
    return 2;
}

/**
 * @brief  参数修改
 * @param  idx 参数类型
 * @param  pBuff 数据
 * @param  length 数据长度
 * @retval 0 成功 1 参数越限 2 索引越限 3 数据描述异常 4 兜底异常
 */
uint8_t storge_ParamSet(eStorgeParamIndex idx, uint8_t * pBuff, uint8_t length)
{
    const sStorgeParamLimitUnit * pLimit;
    uStorgeParamItem read_data;
    float * pData_f;
    uint8_t data_type = 0;

    if (pBuff == NULL) {
        return 3;
    }

    switch (idx) {
        case eStorgeParamIndex_Temp_CC_top_1:
        case eStorgeParamIndex_Temp_CC_top_2:
        case eStorgeParamIndex_Temp_CC_top_3:
        case eStorgeParamIndex_Temp_CC_top_4:
        case eStorgeParamIndex_Temp_CC_top_5:
        case eStorgeParamIndex_Temp_CC_top_6:
        case eStorgeParamIndex_Temp_CC_btm_1:
        case eStorgeParamIndex_Temp_CC_btm_2:
        case eStorgeParamIndex_Temp_CC_env:
            if (length != 4) {
                return 3;
            }
            data_type = 0;
            break;
        default:
            return 2;
    }
    switch (idx) {
        case eStorgeParamIndex_Temp_CC_top_1:
            pData_f = &(gStorgeParamInfo.temperature_cc_top_1);
            break;
        case eStorgeParamIndex_Temp_CC_top_2:
            pData_f = &(gStorgeParamInfo.temperature_cc_top_2);
            break;
        case eStorgeParamIndex_Temp_CC_top_3:
            pData_f = &(gStorgeParamInfo.temperature_cc_top_3);
            break;
        case eStorgeParamIndex_Temp_CC_top_4:
            pData_f = &(gStorgeParamInfo.temperature_cc_top_4);
            break;
        case eStorgeParamIndex_Temp_CC_top_5:
            pData_f = &(gStorgeParamInfo.temperature_cc_top_5);
            break;
        case eStorgeParamIndex_Temp_CC_top_6:
            pData_f = &(gStorgeParamInfo.temperature_cc_top_6);
            break;
        case eStorgeParamIndex_Temp_CC_btm_1:
            pData_f = &(gStorgeParamInfo.temperature_cc_btm_1);
            break;
        case eStorgeParamIndex_Temp_CC_btm_2:
            pData_f = &(gStorgeParamInfo.temperature_cc_btm_2);
            break;
        case eStorgeParamIndex_Temp_CC_env:
            pData_f = &(gStorgeParamInfo.temperature_cc_env);
            break;
        default:
            return 2;
    }
    memcpy(read_data.u8s, pBuff, length);
    pLimit = cStorgeParamLimits + idx;
    switch (data_type) {
        case 0:
            if (read_data.f32 <= pLimit->max && read_data.f32 >= pLimit->min) {
                *pData_f = read_data.f32;
            } else {
                return 1;
            }
            break;
        default:
            return 4;
    }
    return 0;
}

/**
 * @brief  参数修改
 * @param  idx 参数类型
 * @param  pBuff 输出数据
 * @retval 输出数据长度
 */
uint8_t storge_ParamGet(eStorgeParamIndex idx, uint8_t * pBuff)
{
    if (pBuff == NULL) {
        return 0;
    }

    switch (idx) {
        case eStorgeParamIndex_Temp_CC_top_1:
            memcpy(pBuff, (uint8_t *)(&(gStorgeParamInfo.temperature_cc_top_1)), 4);
            return 4;
        case eStorgeParamIndex_Temp_CC_top_2:
            memcpy(pBuff, (uint8_t *)(&(gStorgeParamInfo.temperature_cc_top_2)), 4);
            return 4;
        case eStorgeParamIndex_Temp_CC_top_3:
            memcpy(pBuff, (uint8_t *)(&(gStorgeParamInfo.temperature_cc_top_3)), 4);
            return 4;
        case eStorgeParamIndex_Temp_CC_top_4:
            memcpy(pBuff, (uint8_t *)(&(gStorgeParamInfo.temperature_cc_top_4)), 4);
            return 4;
        case eStorgeParamIndex_Temp_CC_top_5:
            memcpy(pBuff, (uint8_t *)(&(gStorgeParamInfo.temperature_cc_top_5)), 4);
            return 4;
        case eStorgeParamIndex_Temp_CC_top_6:
            memcpy(pBuff, (uint8_t *)(&(gStorgeParamInfo.temperature_cc_top_6)), 4);
            return 4;
        case eStorgeParamIndex_Temp_CC_btm_1:
            memcpy(pBuff, (uint8_t *)(&(gStorgeParamInfo.temperature_cc_btm_1)), 4);
            return 4;
        case eStorgeParamIndex_Temp_CC_btm_2:
            memcpy(pBuff, (uint8_t *)(&(gStorgeParamInfo.temperature_cc_btm_2)), 4);
            return 4;
        case eStorgeParamIndex_Temp_CC_env:
            memcpy(pBuff, (uint8_t *)(&(gStorgeParamInfo.temperature_cc_env)), 4);
            return 4;
        default:
            return 0;
    }
    return 0;
}

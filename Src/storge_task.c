/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "comm_out.h"
#include "comm_main.h"
#include "comm_data.h"
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
    (eStorgeNotifyConf_Read_Flash + eStorgeNotifyConf_Write_Flash + eStorgeNotifyConf_Read_Parmas + eStorgeNotifyConf_Write_Parmas +                           \
     eStorgeNotifyConf_Load_Parmas + eStorgeNotifyConf_Dump_Params + eStorgeNotifyConf_Read_ID_Card + eStorgeNotifyConf_Write_ID_Card +                        \
     eStorgeNotifyConf_Test_Flash + eStorgeNotifyConf_Test_ID_Card + eStorgeNotifyConf_Test_All)

#define STORGE_APP_PARAMS_ADDR (0x1000) /* Sector 1 */
#define STORGE_APP_APRAM_PART_NUM (56)  /* 单次操作最大数目 */
/* Private typedef -----------------------------------------------------------*/
typedef struct {
    uint32_t addr;                   /* 操作地址 */
    uint32_t num;                    /* 操作数量 */
    uint8_t buffer[STORGE_BUFF_LEN]; /* 准备写入的内容 */
} sStorgeTaskQueueInfo;

/* Private macro -------------------------------------------------------------*/

/* Private constants ---------------------------------------------------------*/
const uint32_t cStorge_Test_Flash_Addrs[3] = {0x200000, 0x400000, 0x7ff000};
const uint16_t cStorge_Test_EEPROM_Addrs[3] = {0, 2048, 4080};

/* Private variables ---------------------------------------------------------*/
static sStorgeTaskQueueInfo gStorgeTaskInfo;
static TaskHandle_t storgeTaskHandle = NULL;
static uint8_t gStorgeTaskInfoLock = 0;
static sStorgeParamInfo gStorgeParamInfo;
static uint8_t gStorgeIllumineCnt = 0;
/* Private function prototypes -----------------------------------------------*/
static void storgeTask(void * argument);
static void storge_ParamInit(void);
static uint8_t storge_ParamDump(void);
static uint8_t storge_ParamLoadAll(void);

static void storge_Test_Flash(uint8_t * pBuffer);
static void storge_Test_EEPROM(uint8_t * pBuffer);
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
 * @brief  存储任务运行参数 修改锁 等待释放
 * @param  None
 * @retval 0 等待释放成功 1 失败
 */
uint8_t gStorgeTaskInfoLockWait(uint32_t timeout)
{
    uint32_t cnt;

    cnt = timeout / 100;
    do {
        vTaskDelay(100);
        if (gStorgeTaskInfoLock == 0) {
            return 1;
        }
    } while (--cnt > 0);
    return 1;
}

/**
 * @brief  外部Flash读取失败处理
 * @param  None
 * @retval None
 */
void storgeOutFlashReadErrorHandle(void)
{
    if (spi_FlashReadInfo() > 250) {
        error_Emit(eError_Out_Flash_Unknow);
    } else {
        error_Emit(eError_Out_Flash_Read_Failed);
    }
}

/**
 * @brief  外部Flash写入失败处理
 * @param  None
 * @retval None
 */
void storgeOutFlashWriteErrorHandle(void)
{
    if (spi_FlashReadInfo() > 250) {
        error_Emit(eError_Out_Flash_Unknow);
    } else {
        error_Emit(eError_Out_Flash_Write_Failed);
    }
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
    error_Emit(eError_Storge_Task_Busy);
    return 2; /* 错误码2 超时 */
}

/**
 * @brief  存储任务运行参数 中断版本
 * @param  addr        操作地址
 * @param  pIn         写操作时数据源指针
 * @param  num         操作长度
 * @retval None
 */
uint8_t storgeReadConfInfo_FromISR(uint32_t addr, uint32_t num)
{
    if (gStorgeTaskInfoLockAcquire() == 0) {
        gStorgeTaskInfo.addr = addr;
        gStorgeTaskInfo.num = num;
        return 0;
    }
    error_Emit_FromISR(eError_Storge_Task_Busy);
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
 * @brief  存储任务运行参数
 * @param  addr        操作地址
 * @param  pIn         写操作时数据源指针
 * @param  num         操作长度
 * @param  timeout     信号量等待时间
 * @retval None
 */

uint8_t storgeWriteConfInfo_FromISR(uint32_t addr, uint8_t * pIn, uint32_t num)
{
    if (pIn == NULL ||                             /* 目标指针为空 或者 */
        num > ARRAY_LEN(gStorgeTaskInfo.buffer)) { /* 长度超过缓冲容量 */
        return 1;                                  /* 错误码1 */
    }

    if (gStorgeTaskInfoLockAcquire() == 0) {
        gStorgeTaskInfo.addr = addr;
        gStorgeTaskInfo.num = num;
        memcpy(gStorgeTaskInfo.buffer, pIn, num);
        return 0;
    }

    return 2; /* 错误码2 超时 */
}

/**
 * @brief  存储任务启动通知 中断版本
 * @param  hw          存储类型
 * @param  rw          操作类型
 * @retval 任务通知结果
 */
void storgeTaskNotification(eStorgeNotifyConf type, eProtocol_COMM_Index index)
{
    uint32_t notifyValue = 0;

    notifyValue = type;

    if (type == eStorgeNotifyConf_Read_Flash || type == eStorgeNotifyConf_Write_Flash) {
        if (spi_FlashIsInRange(gStorgeTaskInfo.addr, gStorgeTaskInfo.num) == 0) {
            error_Emit(eError_Out_Flash_Deal_Param);
            return;
        }
    }

    if (index == eComm_Out) {
        notifyValue |= eStorgeNotifyConf_COMM_Out;
    }
    if (index == eComm_Main) {
        notifyValue |= eStorgeNotifyConf_COMM_Main;
    }
    if (xTaskNotify(storgeTaskHandle, notifyValue, eSetValueWithoutOverwrite) != pdPASS) {
        error_Emit(eError_Storge_Task_Busy);
    }
}

/**
 * @brief  存储任务启动通知 中断版本
 * @param  hw          存储类型
 * @param  rw          操作类型
 * @retval 任务通知结果
 */
void storgeTaskNotification_FromISR(eStorgeNotifyConf type, eProtocol_COMM_Index index)
{
    uint32_t notifyValue = 0;
    notifyValue = type;

    if (type == eStorgeNotifyConf_Read_Flash || type == eStorgeNotifyConf_Write_Flash) {
        if (spi_FlashIsInRange(gStorgeTaskInfo.addr, gStorgeTaskInfo.num) == 0) {
            error_Emit_FromISR(eError_Out_Flash_Deal_Param);
            return;
        }
    }

    if (index == eComm_Out) {
        notifyValue |= eStorgeNotifyConf_COMM_Out;
    }
    if (index == eComm_Main) {
        notifyValue |= eStorgeNotifyConf_COMM_Main;
    }
    if (xTaskNotifyFromISR(storgeTaskHandle, notifyValue, eSetValueWithoutOverwrite, NULL) != pdPASS) {
        error_Emit_FromISR(eError_Storge_Task_Busy);
    }
}

/**
 * @brief  存储数据发送任务初始化
 * @param  argument    None
 * @retval None
 */
void storgeTaskInit(void)
{
    if (xTaskCreate(storgeTask, "StorgeTask", 320, NULL, TASK_PRIORITY_STORGE, &storgeTaskHandle) != pdPASS) {
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
    uint32_t i, readCnt, wroteCnt;
    uint32_t ulNotifyValue;
    uint16_t length;
    uint8_t buff[256], result;

    bsp_spi_FlashInit();
    storge_ParamInit();
    result = storge_ParamLoadAll();
    if (result == 1) {
        error_Emit(eError_Out_Flash_Storge_Param_Out_Of_Range); /* 参数越限 */
    } else if (result == 2) {
        error_Emit(eError_Out_Flash_Read_Failed); /* 读取失败 */
    }

    for (;;) {
        xResult = xTaskNotifyWait(0x00, 0xFFFFFFFF, &ulNotifyValue, portMAX_DELAY);
        if (xResult != pdPASS) {
            continue;
        }
        gStorgeTaskInfoLockAcquire(); /* 上锁 */
        protocol_Temp_Upload_Pause(); /* 暂停温度上送 */
        switch (ulNotifyValue & STORGE_DEAL_MASK) {
            case eStorgeNotifyConf_Read_Flash:
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
                    if (length == 0) {
                        storgeOutFlashReadErrorHandle();
                        break;
                    }
                }
                break;
            case eStorgeNotifyConf_Write_Flash:
                wroteCnt = spi_FlashWriteBuffer(gStorgeTaskInfo.addr, gStorgeTaskInfo.buffer, gStorgeTaskInfo.num);
                if (wroteCnt != gStorgeTaskInfo.num) {
                    buff[0] = 1;
                    storgeOutFlashWriteErrorHandle();
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
                        xResult = comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolRespPack_Client_ID_CARD, buff, length + 5);
                        if (xResult != pdPASS) {
                            break;
                        }
                    }
                    if (ulNotifyValue & eStorgeNotifyConf_COMM_Main) {
                        xResult = comm_Main_SendTask_QueueEmitWithBuildCover(eProtocolRespPack_Client_ID_CARD, buff, length + 5);
                        if (xResult != pdPASS) {
                            break;
                        }
                    }
                    if (length == 0) {
                        error_Emit(eError_ID_Card_Read_Failed);
                        break;
                    }
                }
                break;
            case eStorgeNotifyConf_Write_ID_Card:
                wroteCnt = I2C_EEPROM_Write(gStorgeTaskInfo.addr, gStorgeTaskInfo.buffer, gStorgeTaskInfo.num, 30);
                if (wroteCnt != gStorgeTaskInfo.num) {
                    error_Emit(eError_ID_Card_Write_Failed);
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
            case eStorgeNotifyConf_Read_Parmas:
                for (i = 0; i < (gStorgeTaskInfo.num + STORGE_APP_APRAM_PART_NUM - 1) / STORGE_APP_APRAM_PART_NUM; ++i) {
                    readCnt = (gStorgeTaskInfo.num >= STORGE_APP_APRAM_PART_NUM * (i + 1)) ? (STORGE_APP_APRAM_PART_NUM)
                                                                                           : (gStorgeTaskInfo.num % STORGE_APP_APRAM_PART_NUM);
                    length = storge_ParamRead(gStorgeTaskInfo.addr + STORGE_APP_APRAM_PART_NUM * i, readCnt, buff + 4);
                    buff[0] = (gStorgeTaskInfo.addr + STORGE_APP_APRAM_PART_NUM * i) >> 0;
                    buff[1] = (gStorgeTaskInfo.addr + STORGE_APP_APRAM_PART_NUM * i) >> 8;
                    buff[2] = readCnt >> 0;
                    buff[3] = readCnt >> 8;
                    if (ulNotifyValue & eStorgeNotifyConf_COMM_Out) {
                        xResult = comm_Out_SendTask_QueueEmitWithBuildCover(0xDD, buff, length + 4);
                        if (xResult != pdPASS) {
                            break;
                        }
                    }
                    if (ulNotifyValue & eStorgeNotifyConf_COMM_Main) {
                        xResult = comm_Main_SendTask_QueueEmitWithBuildCover(0xDD, buff, length + 4);
                        if (xResult != pdPASS) {
                            break;
                        }
                    }
                    if (length == 0) {
                        storgeOutFlashReadErrorHandle();
                        break;
                    }
                }
                break;
            case eStorgeNotifyConf_Write_Parmas:
                wroteCnt = storge_ParamWrite(gStorgeTaskInfo.addr, gStorgeTaskInfo.num, gStorgeTaskInfo.buffer);
                if (wroteCnt != gStorgeTaskInfo.num) {
                    storgeOutFlashWriteErrorHandle();
                }
                break;
            case eStorgeNotifyConf_Load_Parmas:
                result = storge_ParamLoadAll();
                if (result == 1) {
                    error_Emit(eError_Out_Flash_Storge_Param_Out_Of_Range); /* 参数越限 */
                } else if (result == 2) {
                    storgeOutFlashReadErrorHandle();
                }
                break;
            case eStorgeNotifyConf_Dump_Params:
                if (storge_ParamDump() > 0) {
                    storgeOutFlashWriteErrorHandle(); /* 写入失败 */
                }
                break;
            case eStorgeNotifyConf_Test_Flash:
                storge_Test_Flash(buff);
                break;
            case eStorgeNotifyConf_Test_ID_Card:
                storge_Test_EEPROM(buff);
                break;
            case eStorgeNotifyConf_Test_All:
                storge_Test_Flash(buff);
                storge_Test_EEPROM(buff);
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
    memset(&gStorgeParamInfo, 0, sizeof(gStorgeParamInfo));

    gStorgeParamInfo.temperature_cc_top = 0;
    gStorgeParamInfo.temperature_cc_btm = 0;
    gStorgeParamInfo.temperature_cc_env = 0;
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
 * @brief  参数从Flash读出 到 全局变量 gStorgeParamInfo
 * @param  None
 * @retval 0 成功 1 参数越限 2 读取失败
 */
static uint8_t storge_ParamLoadAll(void)
{
    uint8_t temp, error = 0;
    uint16_t length, i;
    uStorgeParamItem read_data;

    for (i = 0; i < eStorgeParamIndex_Num; ++i) {
        length = spi_FlashReadBuffer(STORGE_APP_PARAMS_ADDR + 4 * i, read_data.u8s, 4);
        if (length != 4) {
            return 2;
        }
        temp = storge_ParamWriteSingle(i, read_data.u8s, 4);
        if (temp != 0) {
            error = temp;
        }
    }
    return error;
}

/**
 * @brief  参数修改
 * @param  idx 参数类型
 * @param  pBuff 数据
 * @param  length 数据长度
 * @retval 0 成功 1 参数越限 2 索引越限 3 数据描述异常
 */
uint8_t storge_ParamWriteSingle(eStorgeParamIndex idx, uint8_t * pBuff, uint8_t length)
{
    uStorgeParamItem read_data;
    float * pData_f;
    uint32_t * pData_u32;

    if (pBuff == NULL) {
        return 3;
    }

    if (idx >= eStorgeParamIndex_Temp_CC_top && idx <= eStorgeParamIndex_Temp_CC_env) {
        if (length != 4) {
            return 3;
        }
        memcpy(read_data.u8s, pBuff, length);
        pData_f = &(gStorgeParamInfo.temperature_cc_top) + (idx - eStorgeParamIndex_Temp_CC_top);
        if (read_data.f32 <= 5 && read_data.f32 >= -5) { /* 温度校正范围限制在±5℃ */
            *pData_f = read_data.f32;
            return 0;
        } else {
            return 1;
        }
    } else if (idx >= eStorgeParamIndex_Illumine_CC_t1_610_i0 && idx <= eStorgeParamIndex_Illumine_CC_t6_550_o5) {
        if (length != 4) {
            return 3;
        }
        memcpy(read_data.u8s, pBuff, length);
        pData_u32 = &(gStorgeParamInfo.illumine_CC_t1_610_i0) + (idx - eStorgeParamIndex_Illumine_CC_t1_610_i0);
        if (read_data.u32 != 0xFFFFFFFF) {
            *pData_u32 = read_data.u32;
            return 0;
        } else {
            return 1;
        }
    }
    return 2;
}

/**
 * @brief  定标参数读取
 * @param  idx 参数类型
 * @retval 定标参数
 */
uint32_t storge_Param_Illumine_CC_Get_Single(eStorgeParamIndex idx)
{
    uint32_t * p;

    if (idx >= eStorgeParamIndex_Num || idx < eStorgeParamIndex_Illumine_CC_t1_610_i0) {
        return 0;
    }
    p = &gStorgeParamInfo.illumine_CC_t1_610_i0;
    p += idx - eStorgeParamIndex_Illumine_CC_t1_610_i0;
    return *p;
}

/**
 * @brief  定标参数设置
 * @param  idx 参数类型
 * @param  data 数据
 * @retval None
 */
void storge_Param_Illumine_CC_Set_Single(eStorgeParamIndex idx, uint32_t data)
{
    uint32_t * p;

    if (idx >= eStorgeParamIndex_Num || idx < eStorgeParamIndex_Illumine_CC_t1_610_i0) {
        return;
    }
    p = &gStorgeParamInfo.illumine_CC_t1_610_i0;
    p += idx - eStorgeParamIndex_Illumine_CC_t1_610_i0;
    *p = data;
    return;
}

/**
 * @brief  参数读取
 * @note   全局变量 gStorgeParamInfo -> 缓存
 * @param  idx 参数类型
 * @param  pBuff 输出数据
 * @retval 输出数据长度
 */
uint8_t storge_ParamReadSingle(eStorgeParamIndex idx, uint8_t * pBuff)
{
    uint8_t * p;
    if (idx >= eStorgeParamIndex_Num) {
        return 0;
    }
    p = (uint8_t *)&gStorgeParamInfo;
    p += idx * 4;
    memcpy(pBuff, p, 4);
    return 4;
}

/**
 * @brief  参数写入
 * @note   缓存 -> 全局变量 gStorgeParamInfo
 * @param  idx 参数类型
 * @param  num 读取个数
 * @param  pBuff 输出数据
 * @retval 输出数据长度
 */
uint16_t storge_ParamWrite(eStorgeParamIndex idx, uint16_t num, uint8_t * pBuff)
{
    uint8_t * p;
    if (pBuff == NULL || num == 0 || num % 4 != 0) {
        return 0;
    }
    if (num / 4 > eStorgeParamIndex_Num - idx) {
        num = (eStorgeParamIndex_Num - idx) * 4;
    }
    p = (uint8_t *)&gStorgeParamInfo;
    p += idx * 4;
    memcpy(p, pBuff, num);
    return num;
}

/**
 * @brief  参数读取
 * @note   全局变量 gStorgeParamInfo -> 缓存
 * @param  idx 参数类型
 * @param  num 读取个数
 * @param  pBuff 输出数据
 * @retval 输出数据长度
 */
uint16_t storge_ParamRead(eStorgeParamIndex idx, uint16_t num, uint8_t * pBuff)
{
    uint8_t * p;
    if (pBuff == NULL || num == 0) {
        return 0;
    }
    if (num > eStorgeParamIndex_Num - idx) {
        num = eStorgeParamIndex_Num - idx;
    }

    p = (uint8_t *)&gStorgeParamInfo;
    p += idx * 4;
    memcpy(pBuff, p, num * 4);
    return num * 4;
}

/**
 * @brief  存储测试 Flash
 * @param  pBuffer 缓存指针
 * @retval None
 */
static void storge_Test_Flash(uint8_t * pBuffer)
{
    uint8_t i, j, readCnt, wroteCnt;

    pBuffer[0] = 4;
    for (i = 0; i < ARRAY_LEN(cStorge_Test_Flash_Addrs); ++i) {
        readCnt = spi_FlashReadBuffer(cStorge_Test_Flash_Addrs[i], pBuffer + 2, 16);
        if (readCnt != 16) {
            pBuffer[1] = 1;
            comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 2);
            break;
        }
        for (j = 0; j < 16; ++j) {
            pBuffer[2 + j] = 0xFF - pBuffer[2 + j];
        }
        wroteCnt = spi_FlashWriteBuffer(cStorge_Test_Flash_Addrs[i], pBuffer + 2, 16);
        if (wroteCnt != 16) {
            pBuffer[1] = 1;
            comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 2);
            break;
        }
        for (j = 0; j < 16; ++j) {
            pBuffer[2 + j] = 0xFF - pBuffer[2 + j];
        }
        wroteCnt = spi_FlashWriteBuffer(cStorge_Test_Flash_Addrs[i], pBuffer + 2, 16);
    }
    pBuffer[1] = 0;
    comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 2);
}

/**
 * @brief  存储测试 EEPROM
 * @param  pBuffer 缓存指针
 * @retval None
 */
static void storge_Test_EEPROM(uint8_t * pBuffer)
{
    uint8_t i, j, readCnt, wroteCnt;

    pBuffer[0] = 5;
    for (i = 0; i < ARRAY_LEN(cStorge_Test_EEPROM_Addrs); ++i) {
        readCnt = I2C_EEPROM_Read(cStorge_Test_EEPROM_Addrs[i], pBuffer + 2, 16, 1000); /* 首次读出数据 */
        if (readCnt != 16) {
            pBuffer[1] = 1;
            comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 2);
            break;
        }
        for (j = 0; j < 16; ++j) { /*  取反作测试数据 */
            pBuffer[2 + j] = 0xFF - pBuffer[2 + j];
        }
        wroteCnt = I2C_EEPROM_Write(cStorge_Test_EEPROM_Addrs[i], pBuffer + 2, 16, 1000); /* 写入数据 */
        if (wroteCnt != 16) {
            pBuffer[1] = 2;
            comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 2);
            break;
        }
        readCnt = I2C_EEPROM_Read(cStorge_Test_EEPROM_Addrs[i], pBuffer + 2 + 16, 16, 1000); /* 回读数据 */
        if (readCnt != 16) {
            pBuffer[1] = 3;
            comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 2);
            break;
        }
        for (j = 0; j < 16; ++j) { /* 二次取反数据 */
            pBuffer[2 + j] = 0xFF - pBuffer[2 + j];
            pBuffer[2 + j + 16] = 0xFF - pBuffer[2 + j + 16];
        }
        wroteCnt = I2C_EEPROM_Write(cStorge_Test_EEPROM_Addrs[i], pBuffer + 2, 16, 1000); /* 写回原始数据 */
        if (memcmp(pBuffer + 2, pBuffer + 2 + 16, 16) != 0) {                             /* 对比结果 */
            pBuffer[1] = 4;
            comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 2);
            break;
        }
    }
    pBuffer[1] = 0;
    comm_Out_SendTask_QueueEmitWithBuildCover(eProtocolEmitPack_Client_CMD_Debug_Self_Check, pBuffer, 2);
}

uint8_t stroge_Conf_CC_O_Data(uint8_t * pBuffer)
{
    uint8_t i;
    uint32_t *pData_u32, *pData_u32_start;
    uint16_t data16;

    if (pBuffer[0] < 1 || pBuffer[0] > 6) { /* 定标段索引不正确 */
        return 2;
    }

    switch (pBuffer[0]) { /* 定标段 */
        case 1:
            pData_u32_start = &(gStorgeParamInfo.illumine_CC_t1_610_o0);
            break;
        case 2:
            pData_u32_start = &(gStorgeParamInfo.illumine_CC_t1_610_o1);
            break;
        case 3:
            pData_u32_start = &(gStorgeParamInfo.illumine_CC_t1_610_o2);
            break;
        case 4:
            pData_u32_start = &(gStorgeParamInfo.illumine_CC_t1_610_o3);
            break;
        case 5:
            pData_u32_start = &(gStorgeParamInfo.illumine_CC_t1_610_o4);
            break;
        case 6:
            pData_u32_start = &(gStorgeParamInfo.illumine_CC_t1_610_o5);
            break;
        default:
            return 2;
    }

    for (i = 0; i < 6; ++i) {
        if (i == 0) { /* 第一通道 三盏灯 */
            pData_u32 = pData_u32_start + (pBuffer[1 + 3 * i] - 1) * 12;
        } else { /* 其余通道两盏灯 */
            pData_u32 = pData_u32_start + 36 + (24 * (i - 1)) + (pBuffer[1 + 3 * i] - 1) * 12;
        }
        memcpy(&data16, pBuffer + 2 + 3 * i, 2); /* 数据 */
        *pData_u32 = data16;
    }

    return storge_ParamDump();
}

void storge_Param_CC_Illumine_CC_Sort(uint32_t * pBuffer, uint8_t length)
{
    uint32_t temp;
    uint8_t i, j;

    for (i = 0; i < length; ++i) {
        for (j = 0; j < length - 1 - i; ++j) {
            if (pBuffer[j] > pBuffer[j + 1]) {
                temp = pBuffer[j + 1];
                pBuffer[j + 1] = pBuffer[j];
                pBuffer[j] = temp;
            }
        }
    }
}

/**
 * @brief  测量数据过滤
 * @param  pBuffer 缓存指针
 * @note   数据量最少要求12个 取后7个中去除2个极值后平均值
 * @retval 浮点值
 */
float storge_Param_CC_Illumine_CC_Filter(uint8_t * pBuffer)
{
    uint8_t i;
    uint16_t temp;
    uint32_t sum = 0, max = 0, min = 0xFFFFFFFF;

    if (pBuffer[0] < 7) { /* 数据不足 */
        return 0;         /* 返回默认值 */
    }

    for (i = pBuffer[0] - 7; i < pBuffer[0]; ++i) {
        memcpy(&temp, pBuffer + 2 * i + 2, 2); /* 采样点数据 */
        if (temp > max) {
            max = temp;
        }
        if (temp < min) {
            min = temp;
        }
        sum += temp;
    }
    return (sum - max - min) / (float)(7 - 2) + 0.5;
}

/**
 * @brief  计算校正值索引
 * @param  channel 通道
 * @param  wave 波长
 * @retval 校正值索引
 */
eStorgeParamIndex storge_Param_Illumine_CC_Get_Index(uint8_t channel, eComm_Data_Sample_Radiant wave)
{
    switch (channel) {
        case 1:
        default:
            return eStorgeParamIndex_Illumine_CC_t1_610_o0 + (wave - eComm_Data_Sample_Radiant_610) * 12;
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
            return eStorgeParamIndex_Illumine_CC_t2_610_o0 + (channel - 2) * 24 + (wave - eComm_Data_Sample_Radiant_610) * 12;
    }
}

/**
 * @brief  校正通道数计数自增
 * @retval None
 */
void gStorgeIllumineCnt_Inc(void)
{
    ++gStorgeIllumineCnt;
}

/**
 * @brief  校正通道数计数清零
 * @retval None
 */
void gStorgeIllumineCnt_Clr(void)
{
    gStorgeIllumineCnt = 0;
}

/**
 * @brief  校正通道数计数 是否达到6
 * @note   计数达到6 即认为该通道校正完成
 * @retval 校正值索引
 */
uint8_t gStorgeIllumineCnt_Check(uint8_t target)
{
    return (gStorgeIllumineCnt == target) ? (1) : (0);
}

/**
 * @brief  校正实际值设置
 * @param  channel 通道索引
 * @param  wave  波长
 * @param  stage_index 段索引
 * @param  avg_data 设置值
 * @retval 校正值索引
 */
uint8_t storge_Conf_CC_Insert(uint8_t channel, eComm_Data_Sample_Radiant wave, uint8_t stage_index, uint32_t avg_data)
{
    eStorgeParamIndex idx;

    if (wave == eComm_Data_Sample_Radiant_405) { /* 剔除405 不进行校正 */
        return 0;
    }

    idx = storge_Param_Illumine_CC_Get_Index(channel, wave);          /* 根据通道和波长 得出校正参数实际值索引 */
    storge_Param_Illumine_CC_Set_Single(idx + stage_index, avg_data); /* 设置校正实际值 */
    gStorgeIllumineCnt_Inc();                                         /* 校正通道数计数自增 */
    return 1;
}

/**
 * @brief  报文解析处理
 * @param  pBuffer 报文 起始指针指向数据区
 * @retval 校正实际值设置 结果
 * @note   当前定标第1段 则依次写入 C1-S1 C2-S2 C3-S3 C4-S4 C5-S5 C6-S6
 * @note   当前定标第2段 则依次写入 C2-S1 C3-S2 C4-S3 C5-S4 C6-S5 C1-S6
 * @note   当前定标第3段 则依次写入 C3-S1 C4-S2 C5-S3 C6-S4 C1-S5 C2-S6
 * @note   当前定标第4段 则依次写入 C4-S1 C5-S2 C6-S3 C1-S4 C2-S5 C3-S6
 * @note   当前定标第5段 则依次写入 C5-S1 C6-S2 C1-S3 C2-S4 C3-S5 C4-S6
 * @note   当前定标第6段 则依次写入 C6-S1 C1-S2 C2-S3 C3-S4 C4-S5 C5-S6
 */
uint8_t stroge_Conf_CC_O_Data_From_B3(uint8_t * pBuffer)
{
    return storge_Conf_CC_Insert(                                                      /* 写入校正实际值 */
                                 pBuffer[1],                                           /* 数据区第一字节 通道号 1～6 */
                                 comm_Data_Get_Correct_Wave(),                         /* 当前测试的波长 */
                                 comm_Data_Get_Corretc_Stage(pBuffer[1]),              /* 当前通道的校正段索引 */
                                 (uint32_t)storge_Param_CC_Illumine_CC_Filter(pBuffer) /* 滤波后的平均值 */
    );
}

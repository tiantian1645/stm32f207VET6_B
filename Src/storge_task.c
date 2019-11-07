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

/* Private typedef -----------------------------------------------------------*/
typedef struct {
    uint32_t addr;                   /* 操作地址 */
    uint32_t num;                    /* 操作数量 */
    uint8_t buffer[STORGE_BUFF_LEN]; /* 准备写入的内容 */
} sStorgeTaskQueueInfo;

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
static sStorgeTaskQueueInfo gStorgeTaskInfo;
static TaskHandle_t storgeTaskHandle = NULL;
static uint8_t gStorgeTaskInfoLock = 0;
static sStorgeParamInfo gStorgeParamInfo;

/* Private function prototypes -----------------------------------------------*/
static void storgeTask(void * argument);

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

    switch (type) {
        case eStorgeNotifyConf_Read_Falsh:
            notifyValue = eStorgeNotifyConf_Read_Falsh;
            break;
        case eStorgeNotifyConf_Write_Falsh:
            notifyValue = eStorgeNotifyConf_Write_Falsh;
            break;
        case eStorgeNotifyConf_Read_ID_Card:
            notifyValue = eStorgeNotifyConf_Read_ID_Card;
            break;
        case eStorgeNotifyConf_Write_ID_Card:
            notifyValue = eStorgeNotifyConf_Write_ID_Card;
            break;
        default:
            return pdFALSE;
    }

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
    uint8_t buff[256];

    bsp_spi_FlashInit();

    for (;;) {
        xResult = xTaskNotifyWait(0x00, 0xFFFFFFFF, &ulNotifyValue, portMAX_DELAY);
        if (xResult != pdPASS) {
            continue;
        }
        temp_Upload_Pause(); /* 暂停温度上送 */
        switch (ulNotifyValue & STORGE_DEAL_MASK) {
            case eStorgeNotifyConf_Read_Falsh:
                for (i = 0; i < ((gStorgeTaskInfo.num + STORGE_FLASH_PART_NUM - 1) / STORGE_FLASH_PART_NUM); ++i) {
                    readCnt =
                        (gStorgeTaskInfo.num >= STORGE_FLASH_PART_NUM * (i + 1)) ? (STORGE_FLASH_PART_NUM) : (gStorgeTaskInfo.num % STORGE_FLASH_PART_NUM);
                    length = spi_FlashReadBuffer(gStorgeTaskInfo.addr + STORGE_FLASH_PART_NUM * i, buff, readCnt);
                    if (ulNotifyValue & eStorgeNotifyConf_COMM_Out) {
                        xResult = comm_Out_SendTask_QueueEmitWithBuildCover(0xD1, buff, length);
                        if (xResult != pdPASS) {
                            break;
                        }
                    }
                    if (ulNotifyValue & eStorgeNotifyConf_COMM_Main) {
                        xResult = comm_Main_SendTask_QueueEmitWithBuildCover(0xD1, buff, length);
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
            default:
                break;
        }
        gStorgeTaskInfoLockRelease(); /* 解锁 */
        temp_Upload_Resume();         /* 恢复温度上送 */
    }
}

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

#define STORGE_TASK_HW_FLASH (1 << 0)
#define STORGE_TASK_HW_EEPROM (0 << 0)

#define STORGE_TASK_TYPE_READ (1 << 1)
#define STORGE_TASK_TYPE_WRITE (0 << 1)

#define STORGE_TASK_NOTIFY_HW (1 << 0)
#define STORGE_TASK_NOTIFY_TYPE (1 << 1)
#define STORGE_TASK_NOTIFY_COMM_OUT (1 << 2)
#define STORGE_TASK_NOTIFY_COMM_MAIN (1 << 3)

#define STORGE_TASK_NOTIFY_IN (STORGE_TASK_NOTIFY_HW | STORGE_TASK_NOTIFY_TYPE)
#define STORGE_TASK_NOTIFY_OUT (STORGE_TASK_NOTIFY_COMM_OUT | STORGE_TASK_NOTIFY_COMM_MAIN)
#define STORGE_TASK_NOTIFY_ALL (STORGE_TASK_NOTIFY_IN | STORGE_TASK_NOTIFY_OUT)

#define STORGE_TASK_NOTIFY_IN_FLASH_READ (STORGE_TASK_HW_FLASH | STORGE_TASK_TYPE_READ)
#define STORGE_TASK_NOTIFY_IN_FLASH_WRITE (STORGE_TASK_HW_FLASH | STORGE_TASK_TYPE_WRITE)
#define STORGE_TASK_NOTIFY_IN_EEPROM_READ (STORGE_TASK_HW_EEPROM | STORGE_TASK_TYPE_READ)
#define STORGE_TASK_NOTIFY_IN_EEPROM_WRITE (STORGE_TASK_HW_EEPROM | STORGE_TASK_TYPE_WRITE)

/* Private typedef -----------------------------------------------------------*/
typedef struct {
    eStorgeHardwareType hw;          /* 存储类型 */
    eStorgeRWType rw;                /* 操作类型 */
    uint32_t addr;                   /* 操作地址 */
    uint32_t num;                    /* 操作数量 */
    uint8_t buffer[STORGE_BUFF_LEN]; /* 准备写入的内容 */
} sStorgeTaskQueueInfo;

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
static sStorgeTaskQueueInfo gStorgeTaskInfo;
static TaskHandle_t storgeTaskHandle = NULL;
static uint8_t gStorgeTaskInfoLock = 0;

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
BaseType_t storgeTaskNotification(eStorgeHardwareType hw, eStorgeRWType rw, eProtocol_COMM_Index index)
{
    uint32_t notifyValue = 0;

    switch (hw) {
        case eStorgeHardwareType_Flash: /* Flash */
            notifyValue |= STORGE_TASK_HW_FLASH;
            break;
        case eStorgeHardwareType_EEPROM: /* EEPROM */
            notifyValue |= STORGE_TASK_HW_EEPROM;
            break;
        default:
            return pdFALSE;
    }
    switch (rw) {
        case eStorgeRWType_Read: /* 读 */
            notifyValue |= STORGE_TASK_TYPE_READ;
            break;
        case eStorgeRWType_Write: /* 写 */
            notifyValue |= STORGE_TASK_TYPE_WRITE;
            break;
        default:
            return pdFALSE;
    }
    if (index == eComm_Out) {
        notifyValue |= STORGE_TASK_NOTIFY_COMM_OUT;
    }
    if (index == eComm_Main) {
        notifyValue |= STORGE_TASK_NOTIFY_COMM_MAIN;
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
    if (xTaskCreate(storgeTask, "StorgeTask", 160, NULL, TASK_PRIORITY_STORGE, &storgeTaskHandle) != pdPASS) {
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
        soft_timer_Temp_Pause(); /* 暂停温度上送 */
        switch (ulNotifyValue & STORGE_TASK_NOTIFY_IN) {
            case STORGE_TASK_NOTIFY_IN_FLASH_READ:
                for (i = 0; i < ((gStorgeTaskInfo.num + STORGE_FLASH_PART_NUM - 1) / STORGE_FLASH_PART_NUM); ++i) {
                    readCnt =
                        (gStorgeTaskInfo.num >= STORGE_FLASH_PART_NUM * (i + 1)) ? (STORGE_FLASH_PART_NUM) : (gStorgeTaskInfo.num % STORGE_FLASH_PART_NUM);
                    length = spi_FlashReadBuffer(gStorgeTaskInfo.addr + STORGE_FLASH_PART_NUM * i, buff, readCnt);
                    if (ulNotifyValue & STORGE_TASK_NOTIFY_COMM_OUT) {
                        xResult = comm_Out_SendTask_QueueEmitWithBuildCover(0xD1, buff, length);
                        if (xResult != pdPASS) {
                            break;
                        }
                    }
                    if (ulNotifyValue & STORGE_TASK_NOTIFY_COMM_MAIN) {
                        xResult = comm_Main_SendTask_QueueEmitWithBuildCover(0xD1, buff, length);
                        if (xResult != pdPASS) {
                            break;
                        }
                    }
                }
                break;
            case STORGE_TASK_NOTIFY_IN_FLASH_WRITE:
                wroteCnt = spi_FlashWriteBuffer(gStorgeTaskInfo.addr, gStorgeTaskInfo.buffer, gStorgeTaskInfo.num);
                if (wroteCnt != gStorgeTaskInfo.num) {
                    buff[0] = 1;
                } else {
                    buff[0] = 0;
                }
                if (ulNotifyValue & STORGE_TASK_NOTIFY_COMM_OUT) {
                    xResult = comm_Out_SendTask_QueueEmitWithBuildCover(0xD2, buff, 1);
                    if (xResult != pdPASS) {
                        break;
                    }
                }
                if (ulNotifyValue & STORGE_TASK_NOTIFY_COMM_MAIN) {
                    xResult = comm_Main_SendTask_QueueEmitWithBuildCover(0xD2, buff, 1);
                    if (xResult != pdPASS) {
                        break;
                    }
                }
                break;
            case STORGE_TASK_NOTIFY_IN_EEPROM_READ:
                for (i = 0; i < ((gStorgeTaskInfo.num + STORGE_EEPROM_PART_NUM - 1) / STORGE_EEPROM_PART_NUM); ++i) {
                    readCnt =
                        (gStorgeTaskInfo.num >= STORGE_EEPROM_PART_NUM * (i + 1)) ? (STORGE_EEPROM_PART_NUM) : (gStorgeTaskInfo.num % STORGE_EEPROM_PART_NUM);
                    length = I2C_EEPROM_Read(gStorgeTaskInfo.addr + STORGE_EEPROM_PART_NUM * i, buff + 5, readCnt, 30);
                    length = readCnt;                                                     /* 调试用! */
                    memset(buff, i, ARRAY_LEN(buff));                                     /* 调试用! */
                    buff[0] = gStorgeTaskInfo.num & 0xFF;                                 /* 信息总长度 小端模式 */
                    buff[1] = gStorgeTaskInfo.num >> 8;                                   /* 信息总长度 小端模式 */
                    buff[2] = (gStorgeTaskInfo.addr + STORGE_EEPROM_PART_NUM * i) & 0xFF; /* 地址信息 小端模式 */
                    buff[3] = (gStorgeTaskInfo.addr + STORGE_EEPROM_PART_NUM * i) >> 8;   /* 地址信息 小端模式 */
                    buff[4] = length;                                                     /* 数据长度 小端模式 */
                    if (ulNotifyValue & STORGE_TASK_NOTIFY_COMM_OUT) {
                        xResult = comm_Out_SendTask_QueueEmitWithBuildCover(eProtocoleRespPack_Client_ID_CARD, buff, length + 5);
                        if (xResult != pdPASS) {
                            break;
                        }
                    }
                    if (ulNotifyValue & STORGE_TASK_NOTIFY_COMM_MAIN) {
                        xResult = comm_Main_SendTask_QueueEmitWithBuildCover(eProtocoleRespPack_Client_ID_CARD, buff, length + 5);
                        if (xResult != pdPASS) {
                            break;
                        }
                    }
                }
                break;
            case STORGE_TASK_NOTIFY_IN_EEPROM_WRITE:
                wroteCnt = I2C_EEPROM_Write(gStorgeTaskInfo.addr, gStorgeTaskInfo.buffer, gStorgeTaskInfo.num, 30);
                if (wroteCnt != gStorgeTaskInfo.num) {
                    buff[0] = 1;
                } else {
                    buff[0] = 0;
                }
                if (ulNotifyValue & STORGE_TASK_NOTIFY_COMM_OUT) {
                    xResult = comm_Out_SendTask_QueueEmitWithBuildCover(0xD4, buff, 1);
                    if (xResult != pdPASS) {
                        break;
                    }
                }
                if (ulNotifyValue & STORGE_TASK_NOTIFY_COMM_MAIN) {
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
        soft_timer_Temp_Resume();     /* 恢复温度上送 */
    }
}

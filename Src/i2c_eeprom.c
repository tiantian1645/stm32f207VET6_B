/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private variables ---------------------------------------------------------*/
extern I2C_HandleTypeDef hi2c1;

/* Private includes ----------------------------------------------------------*/
#include "i2c_eeprom.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
#define AT24CXX_DEV_ADDR 0xA0
#define AT24CXX_MEM_ADDR_SIZE I2C_MEMADD_SIZE_16BIT
#define AT24CXX_I2C_HANDLE hi2c1

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/* Private user code ---------------------------------------------------------*/

/**
 * @brief  I2C EEPROM 读取数据
 * @param  memAddr     操作地址
 * @param  pOutBuff    输出指针
 * @param  length      读取长度
 * @param  timeout     超时时间
 * @retval 读取数量
 */
uint16_t I2C_EEPROM_Read(uint32_t memAddr, uint8_t * pOutBuff, uint16_t length, uint32_t timeout)
{
    uint8_t dealNum;
    uint16_t readCnt = 0;

    if (length == 0) { /* 长度数据无效 */
        return readCnt;
    }

    dealNum = 32 - memAddr % 32; /* 首次操作长度 */
    if (dealNum > length) {
        dealNum = length;
    }

    do {
        if (HAL_I2C_Mem_Read(&AT24CXX_I2C_HANDLE, AT24CXX_DEV_ADDR, memAddr, AT24CXX_MEM_ADDR_SIZE, pOutBuff, dealNum, timeout) != HAL_OK) {
            return readCnt;
        }
        readCnt += dealNum; /* 已读取数量 */
        length -= dealNum;  /* 待处理长度缩减 */
        if (length == 0) {  /* 操作完成 提前返回 */
            return readCnt;
        }
        vTaskDelay(2);                              /* 操作间隔延时 */
        pOutBuff += dealNum;                        /* 数据指针位移 */
        memAddr += dealNum;                         /* 操作地址位移 */
        dealNum = (length >= 32) ? (32) : (length); /* 下次处理长度 */
    } while (length);
    return readCnt;
}

/**
 * @brief  I2C EEPROM 写入数据
 * @param  memAddr     操作地址
 * @param  pOutBuff    输入指针
 * @param  length      写入长度
 * @param  timeout     超时时间
 * @retval 写入数量
 */
uint16_t I2C_EEPROM_Write(uint32_t memAddr, uint8_t * pOutBuff, uint16_t length, uint32_t timeout)
{
    uint8_t dealNum;
    uint16_t wroteCnt = 0;

    if (length == 0) { /* 长度数据无效 */
        return wroteCnt;
    }

    dealNum = 32 - memAddr % 32; /* 首次操作长度 */
    if (dealNum > length) {
        dealNum = length;
    }

    do {
        if (HAL_I2C_Mem_Write(&AT24CXX_I2C_HANDLE, AT24CXX_DEV_ADDR, memAddr, AT24CXX_MEM_ADDR_SIZE, pOutBuff, dealNum, timeout) != HAL_OK) {
            return wroteCnt;
        }
        wroteCnt += dealNum; /* 已写入数量 */
        length -= dealNum;   /* 待处理长度缩减 */
        if (length == 0) {   /* 操作完成 提前返回 */
            return wroteCnt;
        }
        vTaskDelay(4);                              /* 操作间隔延时 */
        pOutBuff += dealNum;                        /* 数据指针位移 */
        memAddr += dealNum;                         /* 操作地址位移 */
        dealNum = (length >= 32) ? (32) : (length); /* 下次处理长度 */
    } while (length);
    return wroteCnt;
}

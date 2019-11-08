/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private variables ---------------------------------------------------------*/
extern I2C_HandleTypeDef hi2c1;

/* Private includes ----------------------------------------------------------*/
#include "i2c_eeprom.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
#define I2C_HW 1

#define AT24CXX_PAGE_WRITE_LENGTH 32
#define AT24CXX_DEV_ADDR 0xA0
#define AT24CXX_MEM_ADDR_SIZE I2C_MEMADD_SIZE_8BIT
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
uint16_t I2C_EEPROM_Read(uint16_t memAddr, uint8_t * pOutBuff, uint16_t length, uint32_t timeout)
{
    uint8_t memAddrBuffer[2];

    memAddrBuffer[0] = memAddr >> 8;
    memAddrBuffer[1] = memAddr & 0xFF;

    if (HAL_I2C_Master_Transmit(&AT24CXX_I2C_HANDLE, (uint16_t)(AT24CXX_DEV_ADDR), memAddrBuffer, 2, 10) == HAL_OK) {
        if (HAL_I2C_Master_Receive(&AT24CXX_I2C_HANDLE, (uint16_t)(AT24CXX_DEV_ADDR), pOutBuff, length, timeout) == HAL_OK) {
            return length;
        }
    }
    if (HAL_GPIO_ReadPin(CARD_IN_GPIO_Port, CARD_IN_Pin) == GPIO_PIN_SET) {   /* 检查ID卡是否插入 */
        error_Emit(eError_Peripheral_Storge_ID_Card, eError_Storge_Hardware); /* 提交错误信息 */
    }
    return 0; /* 直接返回0 */
}

/**
 * @brief  I2C EEPROM 写入数据
 * @param  memAddr     操作地址
 * @param  pOutBuff    输入指针
 * @param  length      写入长度
 * @param  timeout     超时时间
 * @retval 写入数量
 */
uint16_t I2C_EEPROM_Write(uint16_t memAddr, uint8_t * pOutBuff, uint16_t length, uint32_t timeout)
{
    uint8_t dealNum;
    uint16_t wroteCnt = 0;

    if (length == 0) { /* 长度数据无效 */
        return wroteCnt;
    }

    dealNum = AT24CXX_PAGE_WRITE_LENGTH - memAddr % AT24CXX_PAGE_WRITE_LENGTH; /* 首次操作长度 */
    if (dealNum > length) {
        dealNum = length;
    }

    do {
        if (HAL_I2C_Mem_Write(&AT24CXX_I2C_HANDLE, (uint16_t)AT24CXX_DEV_ADDR, memAddr, AT24CXX_MEM_ADDR_SIZE, pOutBuff, dealNum, timeout) != HAL_OK) {
            return wroteCnt;
        }
        wroteCnt += dealNum; /* 已写入数量 */
        length -= dealNum;   /* 待处理长度缩减 */
        if (length == 0) {   /* 操作完成 提前返回 */
            return wroteCnt;
        }
        vTaskDelay(4);                                                                            /* 操作间隔延时 */
        pOutBuff += dealNum;                                                                      /* 数据指针位移 */
        memAddr += dealNum;                                                                       /* 操作地址位移 */
        dealNum = (length >= AT24CXX_PAGE_WRITE_LENGTH) ? (AT24CXX_PAGE_WRITE_LENGTH) : (length); /* 下次处理长度 */
    } while (length);
    return wroteCnt;
}

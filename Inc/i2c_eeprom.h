/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __I2C_EEPROM_H
#define __I2C_EEPROM_H
/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/
typedef enum {
    eI2C_EEPROM_OK,
    eI2C_EEPROM_ERROR,
} eI2C_EEPROM_Result;

typedef enum {
    eI2C_EEPROM_Card_None, /* 未开始检测 */
    eI2C_EEPROM_Card_In,   /* 检测到已插卡 */
    eI2C_EEPROM_Card_Out,  /* 检测到未插卡 */
} eI2C_EEPROM_Card_Status;

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
eI2C_EEPROM_Card_Status gI2C_EEPROM_Card_Status_Get(void);
void I2C_EEPROM_Card_Status_Update(void);

uint16_t I2C_EEPROM_Read(uint16_t memAddr, uint8_t * pOutBuff, uint16_t length, uint32_t timeout);
uint16_t I2C_EEPROM_Write(uint16_t memAddr, uint8_t * pOutBuff, uint16_t length, uint32_t timeout);

/* Private defines -----------------------------------------------------------*/

#endif
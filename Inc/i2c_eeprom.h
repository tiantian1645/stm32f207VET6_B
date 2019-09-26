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

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
uint16_t I2C_EEPROM_Read(uint32_t memAddr, uint8_t * pOutBuff, uint16_t length, uint32_t timeout);
uint16_t I2C_EEPROM_Write(uint32_t memAddr, uint8_t * pOutBuff, uint16_t length, uint32_t timeout);

/* Private defines -----------------------------------------------------------*/

#endif
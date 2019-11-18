/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __INNATE_FLASH_H
#define __INNATE_FLASH_H
/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/
#define INNATE_FLASH_ADDR_TEMP (0x080E0000)
#define INNATE_FLASH_ADDR_REAL (0x08000000)

/* Exported functions prototypes ---------------------------------------------*/

/* Private defines -----------------------------------------------------------*/
uint8_t Innate_Flash_Erase_Temp(void);
uint8_t Innate_Flash_Erase_Real(void);
uint32_t Innate_Flash_Get_Data_By_Addr(uint32_t addr, uint8_t type);
uint8_t Innate_Flash_Write(uint32_t addr, uint8_t * pBuffer, uint16_t length);
uint8_t Innate_Flash_Dump(uint16_t total, uint32_t check_sum);

unsigned int crc32b(const unsigned char * buf, uint32_t len);
#endif

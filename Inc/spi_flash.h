/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __SPI_FLASH_H
#define __SPI_FLASH_H
/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/
typedef enum {
    eSPI_Flash_TR_OK,
    eSPI_Flash_TR_Failed,
} eSPI_Flash_Result;

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
eSPI_Flash_Result SPI_Flash_Read_ID(uint32_t * pFlash_id);
eSPI_Flash_Result SPI_Flash_Enable_Write(uint8_t value);
eSPI_Flash_Result SPI_Flash_Chip_Erase(void);
eSPI_Flash_Result SPI_Flash_Read(uint32_t addr, uint16_t length, uint8_t * pOutBuff);
eSPI_Flash_Result SPI_Flash_Write(uint32_t addr, uint16_t length, uint8_t * pInBuff);

/* Private defines -----------------------------------------------------------*/

#define spi_FlashMAX_PAGE_SIZE (4 * 1024)

/* 定义串行Flash ID */
enum { SST25VF016B_ID = 0xBF2541, MX25L1606E_ID = 0xC22015, W25Q64BV_ID = 0xEF4017 };

typedef struct {
    uint32_t ChipID;    /* 芯片ID */
    char ChipName[16];  /* 芯片型号字符串，主要用于显示 */
    uint32_t TotalSize; /* 总容量 */
    uint16_t PageSize;  /* 页面大小 */
} SFLASH_T;

void bsp_spi_FlashInit(void);
uint32_t spi_FlashReadID(void);
void spi_FlashEraseChip(void);
void spi_FlashEraseSector(uint32_t _uiSectorAddr);
void spi_FlashPageWrite(uint8_t * _pBuf, uint32_t _uiWriteAddr, uint16_t _usSize);
uint16_t spi_FlashWriteBuffer(uint32_t _uiWriteAddr, uint8_t * _pBuf, uint16_t _usWriteSize);
uint32_t spi_FlashReadBuffer(uint32_t _uiReadAddr, uint8_t * _pBuf, uint32_t _uiSize);
void spi_FlashReadInfo(void);

extern SFLASH_T g_tSF;

#endif

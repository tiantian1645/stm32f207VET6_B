/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __SERIAL_H
#define __SERIAL_H

/* Exported constants --------------------------------------------------------*/

/* Exported defines ----------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/
typedef struct {
    uint8_t * pSerialBuff;
    uint8_t minLength;
    uint16_t maxLength;
    uint16_t validLength;
    uint8_t (*has_head)(uint8_t * pBuff, uint16_t length);
    uint16_t (*has_tail)(uint8_t * pBuff, uint16_t length);
    uint8_t (*is_cmop)(uint8_t * pBuff, uint16_t length);
    BaseType_t (*callback)(uint8_t * pbuff, uint16_t length);
} sSerialRecord;

typedef enum {
    eSerialIndex_1 = (uint8_t)(0x01),
    eSerialIndex_2 = (uint8_t)(0x02),
    eSerialIndex_3 = (uint8_t)(0x03),
    eSerialIndex_5 = (uint8_t)(0x05),
} eSerialIndex;

typedef struct {
    uint8_t * pDMA_Buff;
    uint16_t curPos;
    uint16_t oldPos;
    uint16_t buffLength;
    void (*callback)(uint8_t * pBuff, uint16_t length, sSerialRecord * psrd);
} sDMA_Record;

/* Exported functions prototypes ---------------------------------------------*/
void SerialInit(void);
void serialGenerateCallback(uint8_t * pBuff, uint16_t length, sSerialRecord * psrd);
void serialGenerateDealRecv(UART_HandleTypeDef * huart, sDMA_Record * pDMA_Record, DMA_HandleTypeDef * phdma, sSerialRecord * psrd);
BaseType_t serialSendStartDMA(eSerialIndex serialIndex, uint8_t * pSendBuff, uint8_t sendLength, uint32_t timeout);

#endif

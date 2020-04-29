/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __SERIAL_H
#define __SERIAL_H

/* Exported constants --------------------------------------------------------*/

/* Exported defines ----------------------------------------------------------*/
#define COMM_MAIN_SERIAL_INDEX eSerialIndex_1
#define COMM_DATA_SERIAL_INDEX eSerialIndex_2
#define COMM_OUT_SERIAL_INDEX eSerialIndex_5

/* Exported types ------------------------------------------------------------*/
typedef struct {
    uint8_t * pSerialBuff;
    uint8_t minLength;
    uint16_t maxLength;
    uint16_t validLength;
    uint8_t (*has_head)(uint8_t * pBuff, uint16_t length);
    uint16_t (*has_tail)(uint8_t * pBuff, uint16_t length);
    uint8_t (*is_cmop)(uint8_t * pBuff, uint16_t length);
    uint8_t (*callback)(uint8_t * pbuff, uint16_t length);
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

typedef enum {
    eSerial_Source_COMM_Out_Send_Buffer_Bit = (1 << 0),
    eSerial_Source_COMM_Out_Send_Buffer_ISR_Bit = (1 << 1),
    eSerial_Source_COMM_Main_Send_Buffer_Bit = (1 << 2),
    eSerial_Source_COMM_Main_Send_Buffer_ISR_Bit = (1 << 3),
    eSerial_Source_COMM_Data_Send_Buffer_Bit = (1 << 4),
    eSerial_Source_COMM_Data_Send_Buffer_ISR_Bit = (1 << 5),
} eSerial_Source_Bits;

/* Exported functions prototypes ---------------------------------------------*/
void SerialInit(void);

EventBits_t serialSourceFlagsGet(void);
EventBits_t serialSourceFlagsGet_FromISR(void);
EventBits_t serialSourceFlagsWait(EventBits_t flag_bits, uint32_t timeout);
EventBits_t serialSourceFlagsSet_FromISR(EventBits_t flag_bits);
EventBits_t serialSourceFlagsSet(EventBits_t flag_bits);
EventBits_t serialSourceFlagsClear_FromISR(EventBits_t flag_bits);
EventBits_t serialSourceFlagsClear(EventBits_t flag_bits);

void serialGenerateCallback(uint8_t * pBuff, uint16_t length, sSerialRecord * psrd);
void serialGenerateDealRecv(UART_HandleTypeDef * huart, sDMA_Record * pDMA_Record, DMA_HandleTypeDef * phdma, sSerialRecord * psrd);
BaseType_t serialSendStartDMA(eSerialIndex serialIndex, uint8_t * pSendBuff, uint8_t sendLength, uint32_t timeout);
BaseType_t serialSendStartIT(eSerialIndex serialIndex, uint8_t * pSendBuff, uint8_t sendLength);
#endif

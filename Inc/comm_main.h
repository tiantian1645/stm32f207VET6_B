/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __COMM_MAIN_H
#define __COMM_MAIN_H

/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/
#define COMM_MAIN_DMA_RX_SIZE 256
#define COMM_MAIN_SER_RX_SIZE COMM_MAIN_DMA_RX_SIZE

#define COMM_MAIN_SER_TX_SIZE 255

#define COMM_MAIN_SER_TX_RETRY_NUM 3
#define COMM_MAIN_SER_TX_RETRY_INT 2000
#define COMM_MAIN_SER_TX_RETRY_WC 50
#define COMM_MAIN_SER_TX_RETRY_WT ((COMM_MAIN_SER_TX_RETRY_INT) / (COMM_MAIN_SER_TX_RETRY_WC))
#define COMM_MAIN_SER_TX_RETRY_SUM ((COMM_MAIN_SER_TX_RETRY_NUM) * (COMM_MAIN_SER_TX_RETRY_INT))

#define COMM_MAIN_SEND_QUEU_LENGTH 14
#define COMM_MAIN_ERROR_SEND_QUEU_LENGTH 16
#define COMM_MAIN_ACK_SEND_QUEU_LENGTH 6

/* Exported types ------------------------------------------------------------*/
/* 串口 1 接收数据定义*/
typedef struct {
    uint8_t length;
    uint8_t buff[COMM_MAIN_SER_RX_SIZE];
} sComm_Main_RecvInfo;

/* 串口 1 发送数据定义*/
typedef struct {
    uint8_t length;
    uint8_t buff[COMM_MAIN_SER_TX_SIZE];
} sComm_Main_SendInfo;

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void comm_Main_Init(void);
void comm_Main_IRQ_RX_Deal(UART_HandleTypeDef * huart);
void comm_Main_DMA_TX_CallBack(void);
void comm_Main_DMA_RX_Restore(void);

BaseType_t comm_Main_DMA_TX_Wait(uint32_t timeout);
BaseType_t comm_Main_DMA_TX_Enter(uint32_t timeout);
void comm_Main_DMA_TX_Error(void);

UBaseType_t comm_Main_SendTask_Queue_GetWaiting(void);
UBaseType_t comm_Main_SendTask_Queue_GetFree(void);
UBaseType_t comm_Main_SendTask_Queue_GetWaiting_FromISR(void);
UBaseType_t comm_Main_SendTask_Queue_GetFree_FromISR(void);

BaseType_t comm_Main_SendTask_ErrorInfoQueueEmit(uint16_t * pErrorCode, uint32_t timeout);
BaseType_t comm_Main_SendTask_ErrorInfoQueueEmitFromISR(uint16_t * pErrorCode);

BaseType_t comm_Main_SendTask_ACK_QueueEmitFromISR(uint8_t * pPackIndex);

BaseType_t comm_Main_SendTask_QueueEmit(uint8_t * pdata, uint8_t length, uint32_t timeout);
#define comm_Main_SendTask_QueueEmitCover(pdata, length) comm_Main_SendTask_QueueEmit((pdata), (length), (COMM_MAIN_SER_TX_RETRY_SUM))
BaseType_t comm_Main_SendTask_QueueEmitWithBuild(uint8_t cmdType, uint8_t * pData, uint8_t length, uint32_t timeout);
void gComm_Mian_Block_Enable(void);
void gComm_Mian_Block_Disable(void);
BaseType_t comm_Main_SendTask_QueueEmitWithBuildCover(uint8_t cmdType, uint8_t * pData, uint8_t length);

BaseType_t comm_Main_SendTask_QueueEmitWithBuild_FromISR(uint8_t cmdType, uint8_t * pData, uint8_t length);

BaseType_t comm_Main_Send_ACK_Give_From_ISR(uint8_t packIndex);

/* Private defines -----------------------------------------------------------*/

#endif

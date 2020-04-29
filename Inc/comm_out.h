/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __COMM_OUT_H
#define __COMM_OUT_H

/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/
#define COMM_OUT_DMA_RX_SIZE 256
#define COMM_OUT_SER_RX_SIZE COMM_OUT_DMA_RX_SIZE

#define COMM_OUT_SER_TX_SIZE 255

#define COMM_OUT_SER_TX_RETRY_NUM 3
#define COMM_OUT_SER_TX_RETRY_INT 200
#define COMM_OUT_SER_TX_RETRY_WC 5
#define COMM_OUT_SER_TX_RETRY_WT ((COMM_OUT_SER_TX_RETRY_INT) / (COMM_OUT_SER_TX_RETRY_WC))
#define COMM_OUT_SER_TX_RETRY_SUM ((COMM_OUT_SER_TX_RETRY_NUM) * (COMM_OUT_SER_TX_RETRY_INT))

/* Exported types ------------------------------------------------------------*/
/* 串口 1 接收数据定义*/
typedef struct {
    uint8_t length;
    uint8_t buff[COMM_OUT_SER_RX_SIZE];
} sComm_Out_RecvInfo;

/* 串口 1 发送数据定义*/
typedef struct {
    uint8_t length;
    uint8_t buff[COMM_OUT_SER_TX_SIZE];
} sComm_Out_SendInfo;

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void comm_Out_Init(void);
void comm_Out_IRQ_RX_Deal(UART_HandleTypeDef * huart);
void comm_Out_DMA_TX_CallBack(void);
void comm_Out_DMA_RX_Restore(void);

BaseType_t comm_Out_DMA_TX_Wait(uint32_t timeout);

BaseType_t comm_Out_DMA_TX_Enter(uint32_t timeout);
void comm_Out_DMA_TX_Error(void);

BaseType_t comm_Out_DMA_TX_Enter_From_ISR(void);
void comm_Out_DMA_TX_Error_From_ISR(void);

UBaseType_t comm_Out_SendTask_Queue_GetWaiting(void);

BaseType_t comm_Out_SendTask_QueueEmit(uint8_t * pdata, uint8_t length, uint32_t timeout);
#define comm_Out_SendTask_QueueEmitCover(pdata, length) comm_Out_SendTask_QueueEmit((pdata), (length), (COMM_OUT_SER_TX_RETRY_SUM))

BaseType_t comm_Out_SendTask_QueueEmitWithModify(uint8_t * pdata, uint8_t length, uint32_t timeout);
BaseType_t comm_Out_SendTask_QueueEmitWithModify_FromISR(uint8_t * pData, uint8_t length);

BaseType_t comm_Out_SendTask_QueueEmitWithBuild(uint8_t cmdType, uint8_t * pData, uint8_t length, uint32_t timeout);
#define comm_Out_SendTask_QueueEmitWithBuildCover(cmdType, pdata, length)                                                                                      \
    comm_Out_SendTask_QueueEmitWithBuild((cmdType), (pdata), (length), (COMM_OUT_SER_TX_RETRY_SUM))

BaseType_t comm_Out_SendTask_QueueEmitWithBuild_FromISR(uint8_t cmdType, uint8_t * pData, uint8_t length);

BaseType_t comm_Out_SendTask_ErrorInfoQueueEmit(uint16_t * pErrorCode, uint32_t timeout);
BaseType_t comm_Out_SendTask_ErrorInfoQueueEmitFromISR(uint16_t * pErrorCode);

BaseType_t comm_Out_SendTask_ACK_QueueEmitFromISR(uint8_t * pPackIndex);
BaseType_t comm_Out_Send_ACK_Give_From_ISR(uint8_t packIndex);

/* Private defines -----------------------------------------------------------*/

#endif

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __COMM_DATA_H
#define __COMM_DATA_H

/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/
#define COMM_DATA_DMA_RX_SIZE 256
#define COMM_DATA_SER_RX_SIZE ((COMM_DATA_DMA_RX_SIZE + 1) / 2 * 3)

#define COMM_DATA_SER_TX_SIZE 32

#define COMM_DATA_SER_TX_RETRY_NUM 3   /* 重发次数 */
#define COMM_DATA_SER_TX_RETRY_INT 200 /* 重发间隔 200mS 内应有响应包 */
#define COMM_DATA_SER_TX_RETRY_SUM ((COMM_DATA_SER_TX_RETRY_NUM) * (COMM_DATA_SER_TX_RETRY_INT))

#define COMM_DATA_PD_TIMER_PRESCALER (10800 - 1)
#define COMM_DATA_PD_TIMER_PERIOD (100 - 1) /* 100C 主频切半 20 mS */
#define COMM_DATA_PD_TIMER_TIME (2 * 1000 / (COMM_DATA_PD_TIMER_PERIOD + 1))

/* Exported types ------------------------------------------------------------*/
/* 采集板串口 接收数据定义*/
typedef struct {
    uint8_t length;
    uint8_t buff[COMM_DATA_SER_RX_SIZE];
} sComm_Data_RecvInfo;

/* 采集板串口 发送数据定义*/
typedef struct {
    uint8_t length;
    uint8_t buff[COMM_DATA_SER_TX_SIZE];
} sComm_Data_SendInfo;

/* 采样板测试方法 */
/* http://www.dxy.cn/bbs/thread/34787555?sf=2&dn=5#34787555 */
typedef enum {
    eComm_Data_Sample_Assay_None = 0,
    eComm_Data_Sample_Assay_Continuous = 1,
    eComm_Data_Sample_Assay_EndPoint = 2,
    eComm_Data_Sample_Assay_Fixed = 3,
} eComm_Data_Sample_Assay;

/* 采样板测试波长 */
typedef enum {
    eComm_Data_Sample_Radiant_610 = 1,
    eComm_Data_Sample_Radiant_550 = 2,
    eComm_Data_Sample_Radiant_405 = 3,
} eComm_Data_Sample_Radiant;

typedef struct {
    eComm_Data_Sample_Assay assay;
    eComm_Data_Sample_Radiant radiant;
    uint8_t points_num;
} sComm_Data_Sample_Conf_Unit;

typedef enum {
    eComm_Data_Outbound_CMD_CONF = 0x26,  /* 测试项信息帧 */
    eComm_Data_Outbound_CMD_START = 0x27, /* 采样开始控制帧 */
    eComm_Data_Outbound_CMD_STRAY = 0x28, /* 杂散光采集帧 */
    eComm_Data_Outbound_CMD_TEST = 0x30,  /* 工装测试配置帧 */
} eComm_Data_Outbound_CMD;

typedef enum {
    eComm_Data_Inbound_CMD_DATA = 0xB3,  /* 采集数据帧 */
    eComm_Data_Inbound_CMD_OVER = 0x34,  /* 采集数据完成帧 */
    eComm_Data_Inbound_CMD_ERROR = 0xB5, /* 错误信息帧 */
} eComm_Data_Inbound_CMD;

typedef struct {
    uint8_t num;
    uint8_t channel;
    uint16_t data[120];
} eComm_Data_Sample;

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void comm_Data_Init(void);
void comm_Data_IRQ_RX_Deal(UART_HandleTypeDef * huart);

BaseType_t comm_Data_DMA_TX_Wait(uint32_t timeout);
void comm_Data_DMA_RX_Restore(void);

void comm_Data_DMA_TX_CallBack(void);
BaseType_t comm_Data_DMA_TX_Enter(uint32_t timeout);

void comm_Data_DMA_TX_Error(void);
BaseType_t comm_Data_SendTask_QueueEmit(uint8_t * pdata, uint8_t length, uint32_t timeout);
#define comm_Data_SendTask_QueueEmitCover(pdata, length) comm_Data_SendTask_QueueEmit((pdata), (length), (COMM_DATA_SER_TX_RETRY_SUM))
BaseType_t comm_Data_Send_ACK_Give(uint8_t packIndex);

uint8_t gComm_Data_Sample_Max_Point_Get(void);
void gComm_Data_Sample_Max_Point_Clear(void);

uint8_t comm_Data_Sample_Start(void);
uint8_t comm_Data_Sample_Force_Stop(void);

void comm_Data_PD_Time_Deal_FromISR(void);

void gComm_Data_TIM_StartFlag_Clear(void);

uint8_t comm_Data_Build_Sample_Conf_Pack(uint8_t * pData);

BaseType_t comm_Data_Sample_Send_Conf(uint8_t * pData);
BaseType_t comm_Data_Sample_Send_Conf_TV(uint8_t * pData);

BaseType_t comm_Data_Conf_Sem_Wait(uint32_t timeout);
BaseType_t comm_Data_Conf_Sem_Give(void);

BaseType_t comm_Data_Sample_Owari(void);

uint8_t gComm_Data_Sample_PD_WH_Idx_Get(void);

BaseType_t comm_Data_Start_Stary_Test(void);
void comm_Data_Stary_Test_Mark(void);
void comm_Data_Stary_Test_Clear(void);
uint8_t comm_Data_Stary_Test_Is_Running(void);

void gComm_Data_Sample_Period_Set(uint8_t se);
void gComm_Data_Sample_Next_Idle_Set(uint16_t idle);
void gComm_Data_Sample_Next_Idle_Clr(void);
/* Private defines -----------------------------------------------------------*/

#endif

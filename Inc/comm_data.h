/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __COMM_DATA_H
#define __COMM_DATA_H

/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/
#define COMM_DATA_DMA_RX_SIZE 256
#define COMM_DATA_SER_RX_SIZE COMM_DATA_DMA_RX_SIZE

#define COMM_DATA_SER_TX_SIZE 255

#define COMM_DATA_SER_TX_RETRY_NUM 3   /* 重发次数 */
#define COMM_DATA_SER_TX_RETRY_INT 200 /* 重发间隔 200mS 内应有响应包 */
#define COMM_DATA_SER_TX_RETRY_WC 40
#define COMM_DATA_SER_TX_RETRY_WT ((COMM_DATA_SER_TX_RETRY_INT) / (COMM_DATA_SER_TX_RETRY_WC))
#define COMM_DATA_SER_TX_RETRY_SUM ((COMM_DATA_SER_TX_RETRY_NUM) * (COMM_DATA_SER_TX_RETRY_INT))

#define COMM_DATA_WH_TIMER_PRESCALER (54000 - 1) /* TIMER 6 主频切半 1 mS */
#define COMM_DATA_WH_TIMER_PERIOD (10000 - 1)    /* 10000C 10 S */

#define COMM_DATA_PD_TIMER_PRESCALER (54000 - 1) /* TIMER 7 主频切半 1 mS */
#define COMM_DATA_PD_TIMER_PERIOD (500 - 1)      /* 500C 500 mS */

#define COMM_DATA_LED_VOLTAGE_INIT_610 30  /* 35 ~ 54 avg 43.9667 */
#define COMM_DATA_LED_VOLTAGE_INIT_550 400 /* 440 ~ 575 avg 498.7667 */
#define COMM_DATA_LED_VOLTAGE_INIT_405 40  /* 42 ~ 78 avg 65.7097 */

#define COMM_DATA_LED_VOLTAGE_UNIT_610 4
#define COMM_DATA_LED_VOLTAGE_UNIT_550 16
#define COMM_DATA_LED_VOLTAGE_UNIT_405 4

#define COMM_DATA_LED_VOLTAGE_MAX_610 70
#define COMM_DATA_LED_VOLTAGE_MAX_550 650
#define COMM_DATA_LED_VOLTAGE_MAX_405 84

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

typedef struct {
    sComm_Data_Sample_Conf_Unit conf;
    uint8_t num;
    uint8_t wave;
    uint8_t data_type;
    uint8_t raw_datas[240];
} sComm_Data_Sample;

typedef enum {
    eComm_Data_Outbound_CMD_CONF = 0x26,              /* 测试项信息帧 */
    eComm_Data_Outbound_CMD_START = 0x27,             /* 采样开始控制帧 */
    eComm_Data_Outbound_CMD_STRAY = 0x28,             /* 杂散光采集帧 */
    eComm_Data_Outbound_CMD_TEST = 0x30,              /* 工装测试配置帧 */
    eComm_Data_Outbound_CMD_LED_GET = 0x32,           /* LED电压读取 */
    eComm_Data_Outbound_CMD_LED_SET = 0x33,           /* LED电压设置 */
    eComm_Data_Outbound_CMD_FA_PD_SET = 0x34,         /* 工装PD测试 */
    eComm_Data_Outbound_CMD_FA_LED_SET = 0x35,        /* 工装PD测试 */
    eComm_Data_Outbound_CMD_OFFSET_GET = 0x36,        /* 杂散光读取 */
    eComm_Data_Outbound_CMD_WHITE_MAGNIFY_GET = 0x37, /* 白板PD放大倍数读取 */
    eComm_Data_Outbound_CMD_WHITE_MAGNIFY_SET = 0x38, /* 白板PD放大倍数设置 */
} eComm_Data_Outbound_CMD;

typedef enum {
    eComm_Data_Inbound_CMD_DATA = 0xB3,              /* 采集数据帧 */
    eComm_Data_Inbound_CMD_OVER = 0x34,              /* 采集数据完成帧 */
    eComm_Data_Inbound_CMD_ERROR = 0xB5,             /* 错误信息帧 */
    eComm_Data_Inbound_CMD_LED_GET = 0x32,           /* LED电压读取 */
    eComm_Data_Inbound_CMD_FA_DEBUG = 0xD3,          /* 工装PD采样输出 */
    eComm_Data_Inbound_CMD_OFFSET_GET = 0xB4,        /* Offset读取 */
    eComm_Data_Inbound_CMD_WHITE_MAGNIFY_GET = 0x37, /* 白板PD放大倍数读取 */
    /* Bootloader */
    eComm_Data_Inbound_CMD_BL_INSTR = 0x90,    /* 采样板BL命令帧 */
    eComm_Data_Inbound_CMD_BL_DATA = 0x91,     /* 采样板BL数据帧 */
    eComm_Data_Inbound_CMD_GET_VERSION = 0x92, /* 采样板APP版本 */
} eComm_Data_Inbound_CMD;

typedef struct {
    uint16_t led_voltage_610;
    uint16_t led_voltage_550;
    uint16_t led_voltage_405;
} sComm_LED_Voltage;

typedef enum {
    eComm_Data_Sample_Data_ERROR,
    eComm_Data_Sample_Data_U16,
    eComm_Data_Sample_Data_U32,
    eComm_Data_Sample_Data_MIX,
    eComm_Data_Sample_Data_UNKNOW,
} eComm_Data_Sample_Data;

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void comm_Data_Init(void);
void comm_Data_IRQ_RX_Deal(UART_HandleTypeDef * huart);

BaseType_t comm_Data_DMA_TX_Wait(uint32_t timeout);
void comm_Data_DMA_RX_Restore(void);

void comm_Data_DMA_TX_CallBack(void);

BaseType_t comm_Data_DMA_TX_Enter(uint32_t timeout);
void comm_Data_DMA_TX_Error(void);

BaseType_t comm_Data_DMA_TX_Enter_From_ISR(void);
void comm_Data_DMA_TX_Error_From_ISR(void);

BaseType_t comm_Data_SendTask_QueueEmit(uint8_t * pdata, uint8_t length, uint32_t timeout);
#define comm_Data_SendTask_QueueEmitCover(pdata, length) comm_Data_SendTask_QueueEmit((pdata), (length), (COMM_DATA_SER_TX_RETRY_SUM))

BaseType_t comm_Data_SendTask_QueueEmit_FromISR(uint8_t * pData, uint8_t length);

BaseType_t comm_Data_Send_ACK_Give_From_ISR(uint8_t packIndex);

uint8_t gComm_Data_AgingLoop_Mode_Get(void);

uint8_t gComm_Data_Sample_Max_Point_Get(void);
void gComm_Data_Sample_Max_Point_Clear(void);

void comm_Data_RecordInit(void);
uint8_t comm_Data_Get_LED_Voltage();
uint8_t comm_Data_Set_LED_Voltage(eComm_Data_Sample_Radiant radiant, uint16_t voltage);
uint8_t comm_Data_Check_LED(eComm_Data_Sample_Radiant radiant, uint16_t dac, uint8_t idx);
uint8_t comm_Data_Wait_Data(uint8_t mask, uint32_t timeout);
uint8_t comm_Data_Copy_Data_U32(uint8_t mask, uint32_t * pBuffer);
int16_t gComm_Data_LED_Voltage_Interval_Get(void);
uint8_t gComm_Data_LED_Voltage_Points_Get(void);

uint8_t comm_Data_Sample_Start(void);
uint8_t comm_Data_sample_Start_PD(void);
uint8_t comm_Data_Sample_Force_Stop(void);
uint8_t comm_Data_Sample_Force_Stop_FromISR(void);

void comm_Data_WH_Time_Deal_FromISR(void);
void comm_Data_PD_Time_Deal_FromISR(void);

void gComm_Data_TIM_StartFlag_Clear(void);

uint8_t comm_Data_Build_Sample_Conf_Pack(uint8_t * pData);

BaseType_t comm_Data_Sample_Send_Clear_Conf(void);
BaseType_t comm_Data_Sample_Send_Clear_Conf_FromISR(void);
BaseType_t comm_Data_Sample_Send_Conf(uint8_t * pData);
BaseType_t comm_Data_Sample_Send_Conf_FromISR(uint8_t * pData);
BaseType_t comm_Data_Sample_Send_Conf_TV(uint8_t * pData);
BaseType_t comm_Data_Sample_Send_Conf_TV_FromISR(uint8_t * pData);
BaseType_t comm_Data_Sample_Send_Conf_Re(void);

BaseType_t comm_Data_Conf_Sem_Wait(uint32_t timeout);
BaseType_t comm_Data_Conf_Sem_Give(void);
BaseType_t comm_Data_Conf_Sem_Give_FromISR(void);

BaseType_t comm_Data_Conf_LED_Voltage_Get(void);
BaseType_t comm_Data_Conf_LED_Voltage_Get_FromISR(void);
BaseType_t comm_Data_Conf_LED_Voltage_Set(uint8_t * pData);
BaseType_t comm_Data_Conf_LED_Voltage_Set_FromISR(uint8_t * pData);

BaseType_t comm_Data_Conf_FA_PD_Set_FromISR(uint8_t * pData);
BaseType_t comm_Data_Conf_FA_LED_Set_FromISR(uint8_t * pData);

BaseType_t comm_Data_Conf_Offset_Get_FromISR(void);

BaseType_t comm_Data_Conf_White_Magnify_Get_FromISR(void);
BaseType_t comm_Data_Conf_White_Magnify_Set_FromISR(uint8_t * pData);

BaseType_t comm_Data_Transit_FromISR(uint8_t * pData, uint8_t length);

BaseType_t comm_Data_Sample_Owari(void);

uint8_t gComm_Data_Sample_PD_WH_Idx_Get(void);

BaseType_t comm_Data_Start_Stary_Test(void);
void comm_Data_Stary_Test_Mark(void);
void comm_Data_Stary_Test_Clear(void);
uint8_t comm_Data_Stary_Test_Is_Running(void);

uint8_t comm_Data_Sample_Data_Fetch(uint8_t channel, uint8_t * pBuffer, uint8_t * pLength);
eComm_Data_Sample_Data comm_Data_Sample_Data_Commit(uint8_t channel, uint8_t * pBuffer, uint8_t length, uint8_t replcae);
uint8_t comm_Data_Sample_Data_Correct(uint8_t channel, uint8_t * pBuffer, uint8_t * pLength);

void gComm_Data_Correct_Flag_Mark(void);
void gComm_Data_Correct_Flag_Clr(void);
uint8_t gComm_Data_Correct_Flag_Check(void);
BaseType_t comm_Data_Sample_Send_Conf_Correct(uint8_t * pData, eComm_Data_Sample_Radiant wave, uint8_t point_num, uint8_t cmd_type);
eComm_Data_Sample_Radiant comm_Data_Get_Correct_Wave(void);

uint8_t comm_Data_Get_Corretc_Stage(uint8_t channel);
void comm_Data_Set_Corretc_Stage(uint8_t channel, uint8_t idx);

void gComm_Data_Lamp_BP_Flag_Mark(void);
void gComm_Data_Lamp_BP_Flag_Clr(void);
uint8_t gComm_Data_Lamp_BP_Flag_Check(void);

void gComm_Data_SP_LED_Flag_Mark(eComm_Data_Sample_Radiant radiant);
void gComm_Data_SP_LED_Flag_Clr(void);
eComm_Data_Sample_Radiant comm_Data_SP_LED_Is_Running(void);
void gComm_Data_LED_Voltage_Interval_Set(int16_t interval);

void gComm_Data_SelfCheck_PD_Flag_Mark(eComm_Data_Sample_Radiant radiant);
void gComm_Data_SelfCheck_PD_Flag_Clr(void);
eComm_Data_Sample_Radiant gComm_Data_SelfCheck_PD_Flag_Get(void);

void comm_Data_GPIO_Init(void);
void comm_Data_Board_Reset(void);
void comm_Data_ISR_Deal(void);
void comm_Data_ISR_Tran(uint8_t wp);

BaseType_t comm_Data_SendTask_ACK_QueueEmitFromISR(uint8_t * pPackIndex);

/* Private defines -----------------------------------------------------------*/

#endif

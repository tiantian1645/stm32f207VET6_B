/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __BARCODE_SCAN_H
#define __BARCODE_SCAN_H
/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/
#define BARCODE_QR_LENGTH 120
#define BARCODE_BA_LENGTH 20
#define BARCODE_INDEX_NUM 6

/* Exported types ------------------------------------------------------------*/
/* 条码索引 32细分步下标准 */
/*
In [606]: 123.62 - 43.52
Out[606]: 80.1

In [607]: _ / 3950
Out[607]: 0.020278481012658226
*/
typedef enum {
    eBarcodeIndex_0 = 0,
    eBarcodeIndex_1 = 3156,
    eBarcodeIndex_2 = 6312,
    eBarcodeIndex_3 = 9468,
    eBarcodeIndex_4 = 12624,
    eBarcodeIndex_5 = 15780,
    eBarcodeIndex_6 = 18680,
} eBarcodeIndex;

typedef enum {
    eBarcodeState_OK,
    eBarcodeState_Tiemout,
    eBarcodeState_Busy,
    eBarcodeState_Interrupt,
    eBarcodeState_Error,
} eBarcodeState;

/* 扫码结果结构体 */
typedef struct {
    eBarcodeState state;
    uint8_t length;
    uint8_t * pData;
} sBarcoderesult;

/* 定标校正信息结构体 */
typedef struct {
    uint32_t branch;       /* 批次 */
    uint32_t date;         /* 日期 */
    uint8_t stages[6];     /* 定标段索引 */
    uint16_t i_values[13]; /* 标段数据 理论值 13个灯 */
    uint16_t o_values[13]; /* 标段数据 实际值 13个灯 */
    uint32_t check;        /* 校验数据 */
} sBarcodeCorrectInfo;

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void barcode_Test(uint32_t cnt);

void barcode_Init(void);
uint8_t barcode_Task_Notify(uint32_t mark);

eBarcodeState barcode_Motor_Enter(void);
eBarcodeState barcode_Motor_Init(void);
uint8_t barcode_Motor_Reset_Pos(void);

eBarcodeState barcode_Motor_Run_By_Index(eBarcodeIndex index);

eBarcodeState barcode_Scan_By_Index(eBarcodeIndex index);
eBarcodeState barcode_Scan_QR(void);
eBarcodeState barcode_Scan_Bar(void);
eBarcodeState barcode_Read_From_Serial(uint8_t * pOut_length, uint8_t * pData, uint8_t max_read_length, uint32_t timeout);

uint8_t barcode_Interrupt_Flag_Get(void);
void barcode_Interrupt_Flag_Mark(void);
void barcode_Interrupt_Flag_Clear(void);

void barcode_Scan_Bantch(uint8_t pos_mark, uint8_t scan_mark);
int32_t barcode_Motor_Read_Position(void);

uint8_t barcode_Scan_Decode_Correct_Info(uint8_t * pBuffer, uint8_t length);
uint8_t barcode_Scan_Decode_Correct_Info_From_Result(void);
/* Private defines -----------------------------------------------------------*/

#endif

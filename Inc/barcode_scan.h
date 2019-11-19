/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __BARCODE_SCAN_H
#define __BARCODE_SCAN_H
/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/
#define BARCODE_QR_LENGTH 100
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

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
uint8_t barcode_serial_Test(void);
void barcode_Test(uint32_t cnt);

void barcode_Init(void);
uint8_t barcode_Task_Notify(uint32_t mark);

eBarcodeState barcode_Motor_Enter(void);
eBarcodeState barcode_Motor_Init(void);
uint8_t barcode_Motor_Reset_Pos(void);

eBarcodeState barcode_Motor_Run_By_Index(eBarcodeIndex index);

eBarcodeState barcode_Scan_By_Index(eBarcodeIndex index);
eBarcodeState barcode_Scan_Whole(void);
eBarcodeState barcode_Read_From_Serial(uint8_t * pOut_length, uint8_t * pData, uint8_t max_read_length, uint32_t timeout);

uint8_t barcode_Interrupt_Flag_Get(void);
void barcode_Interrupt_Flag_Mark(void);
void barcode_Interrupt_Flag_Clear(void);

void barcode_Scan_Bantch(uint8_t pos_mark, uint8_t scan_mark);
int32_t barcode_Motor_Read_Position(void);

/* Private defines -----------------------------------------------------------*/

#endif

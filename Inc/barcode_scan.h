/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __BARCODE_SCAN_H
#define __BARCODE_SCAN_H
/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/
#define BARCODE_MAX_LENGTH 100
#define BARCODE_INDEX_NUM 6

/* Exported types ------------------------------------------------------------*/
/* 条码索引 32细分步下标准 */
typedef enum {
    eBarcodeIndex_0 = 0,
    eBarcodeIndex_1 = 3200,
    eBarcodeIndex_2 = 6400,
    eBarcodeIndex_3 = 9600,
    eBarcodeIndex_4 = 12800,
    eBarcodeIndex_5 = 16000,
} eBarcodeIndex;

typedef enum {
    eBarcodeState_OK,
    eBarcodeState_Tiemout,
    eBarcodeState_Busy,
    eBarcodeState_Error,
} eBarcodeState;

/* 扫码结果结构体 */
typedef struct {
    eBarcodeState state;
    eBarcodeIndex index;
    uint8_t length;
    uint8_t data[BARCODE_MAX_LENGTH];
} sBarcoderesult;

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void barcode_serial_Test(void);
void barcde_Test(uint32_t cnt);

void barcode_Init(void);
eBarcodeState barcode_Motor_Enter(void);
eBarcodeState barcode_Scan_By_Index(eBarcodeIndex index, uint8_t * pOut_length, uint8_t * pData, uint32_t timeout);
eBarcodeState barcode_Read_From_Serial(uint8_t * pOut_length, uint8_t * pData, uint32_t timeout);

/* Private defines -----------------------------------------------------------*/

#endif

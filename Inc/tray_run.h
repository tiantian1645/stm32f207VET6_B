/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __TRAY_RUN_H
#define __TRAY_RUN_H
/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/

/* 条码索引 32细分步下标准 */
typedef enum {
    eTrayIndex_0 = 0,
    eTrayIndex_1 = 5692,
    eTrayIndex_2 = 25600,
} eTrayIndex;

typedef enum {
    eTrayState_OK,
    eTrayState_Tiemout,
    eTrayState_Busy,
    eTrayState_Error,
} eTrayState;

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
int32_t tray_Motor_Get_Status_Position(void);

uint8_t tray_Motor_Reset_Pos();
eTrayState tray_Motor_Init(void);
eTrayState tray_Move_By_Index(eTrayIndex index, uint32_t timeout);

/* Private defines -----------------------------------------------------------*/

#endif

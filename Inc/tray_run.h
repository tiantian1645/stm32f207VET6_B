/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __TRAY_RUN_H
#define __TRAY_RUN_H
/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/
#define TRAY_MOTOR_IS_OPT_1 (motor_OPT_Status_Get_Tray() == eMotor_OPT_Status_OFF)                      /* 光耦输入 */
#define TRAY_MOTOR_IS_OPT_2 (motor_OPT_Status_Get(eMotor_OPT_Index_Tray_Scan) == eMotor_OPT_Status_OFF) /* 光耦输入 */

/* Exported types ------------------------------------------------------------*/

/* 条码索引 32细分步下标准 */
typedef enum {
    eTrayIndex_0 = 0,     /* 原点位置 */
    eTrayIndex_1 = 5692,  /* 扫码位置 步数定位 */
    eTrayIndex_2 = 26400, /* 加样位置 */
    eTrayIndex_3 = 5600,  /* 扫码位置 光耦定位 */
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

int32_t tray_Motor_Read_Position(void);
/* Private defines -----------------------------------------------------------*/

#endif

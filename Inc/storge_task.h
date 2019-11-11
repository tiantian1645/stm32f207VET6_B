/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STORGE_TASK_H
#define __STORGE_TASK_H
/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/
#include "protocol.h"

/* Exported types ------------------------------------------------------------*/
typedef enum {
    eStorgeNotifyConf_Read_Falsh = 0x00000001,
    eStorgeNotifyConf_Write_Falsh = 0x00000002,
    eStorgeNotifyConf_Load_Parmas = 0x00000004,
    eStorgeNotifyConf_Dump_Params = 0x00000008,
    eStorgeNotifyConf_Read_ID_Card = 0x00000010,
    eStorgeNotifyConf_Write_ID_Card = 0x00000020,
    eStorgeNotifyConf_COMM_Out = 0x10000000,
    eStorgeNotifyConf_COMM_Main = 0x20000000,
} eStorgeNotifyConf;

typedef enum {
    eStorgeParamIndex_Temp_CC_top_1,
    eStorgeParamIndex_Temp_CC_top_2,
    eStorgeParamIndex_Temp_CC_top_3,
    eStorgeParamIndex_Temp_CC_top_4,
    eStorgeParamIndex_Temp_CC_top_5,
    eStorgeParamIndex_Temp_CC_top_6,
    eStorgeParamIndex_Temp_CC_btm_1,
    eStorgeParamIndex_Temp_CC_btm_2,
    eStorgeParamIndex_Temp_CC_env,
    eStorgeParamIndex_Num,
} eStorgeParamIndex;

typedef struct {
    float temperature_cc_top_1;
    float temperature_cc_top_2;
    float temperature_cc_top_3;
    float temperature_cc_top_4;
    float temperature_cc_top_5;
    float temperature_cc_top_6;
    float temperature_cc_btm_1;
    float temperature_cc_btm_2;
    float temperature_cc_env;
} sStorgeParamInfo;

typedef struct {
    float default_;
    float min;
    float max;
} sStorgeParamLimitUnit;

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void storgeTaskInit(void);
uint8_t storgeReadConfInfo(uint32_t addr, uint32_t num, uint32_t timeout);
uint8_t storgeWriteConfInfo(uint32_t addr, uint8_t * pIn, uint32_t num, uint32_t timeout);
BaseType_t storgeTaskNotification(eStorgeNotifyConf type, eProtocol_COMM_Index index);

uint8_t storge_ParamSet(eStorgeParamIndex idx, uint8_t * pBuff, uint8_t length);
uint8_t storge_ParamGet(eStorgeParamIndex idx, uint8_t * pBuff);
/* Private defines -----------------------------------------------------------*/

#endif

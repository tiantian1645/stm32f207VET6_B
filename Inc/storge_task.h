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

typedef struct {
    float temperature_offset_top_1;
    float temperature_offset_top_2;
    float temperature_offset_top_3;
    float temperature_offset_top_4;
    float temperature_offset_top_5;
    float temperature_offset_top_6;
    float temperature_offset_btm_1;
    float temperature_offset_btm_2;
    float temperature_offset_env;
} sStorgeParamInfo;

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void storgeTaskInit(void);
uint8_t storgeReadConfInfo(uint32_t addr, uint32_t num, uint32_t timeout);
uint8_t storgeWriteConfInfo(uint32_t addr, uint8_t * pIn, uint32_t num, uint32_t timeout);
BaseType_t storgeTaskNotification(eStorgeNotifyConf type, eProtocol_COMM_Index index);

/* Private defines -----------------------------------------------------------*/

#endif

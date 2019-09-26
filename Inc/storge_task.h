/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STORGE_TASK_H
#define __STORGE_TASK_H
/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/
#include "protocol.h"

/* Exported types ------------------------------------------------------------*/
typedef enum {
    eStorgeHardwareType_Flash,  /* Flash 8MB */
    eStorgeHardwareType_EEPROM, /* EEPROM 64KB */
} eStorgeHardwareType;

typedef enum {
    eStorgeRWType_Read,  /* 读操作 */
    eStorgeRWType_Write, /* 写操作 */
} eStorgeRWType;

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void storgeTaskInit(void);
uint8_t storgeReadConfInfo(uint32_t addr, uint32_t num, uint32_t timeout);
uint8_t storgeWriteConfInfo(uint32_t addr, uint8_t * pIn, uint32_t num, uint32_t timeout);
BaseType_t storgeTaskNotification(eStorgeHardwareType hw, eStorgeRWType rw, eProtocol_COMM_Index index);

/* Private defines -----------------------------------------------------------*/

#endif

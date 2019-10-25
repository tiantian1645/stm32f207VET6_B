/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __TEMPERATURE_H
#define __TEMPERATURE_H

/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/
#define TEMP_PSC (60 - 1)  /* 1000 000 C/S */
#define TEMP_ARR (500 - 1) /* 500C -> 500uS 每毫秒2个采样点 */

#define TEMP_INVALID_DATA ((float)128) /* 无效温度值 */

/* Exported types ------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
uint32_t temp_Get_Conv_Cnt(void);
HAL_StatusTypeDef temp_Start_ADC_DMA(void);
HAL_StatusTypeDef temp_Stop_ADC_DMA(void);

float temp_Get_Temp_Data(uint8_t idx);
float temp_Get_Temp_Data_TOP(void);
float temp_Get_Temp_Data_BTM(void);
float temp_Get_Temp_Data_ENV(void);

/* Private defines -----------------------------------------------------------*/

#endif

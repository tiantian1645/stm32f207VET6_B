/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __TEMPERATURE_H
#define __TEMPERATURE_H

/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/
#define TEMP_PSC (54 - 1) /* 1000 000 C/S */
#define TEMP_ARR (40 - 1) /* 40C -> 40uS 每毫秒25个采样点 */

#define TEMP_INVALID_DATA ((float)128) /* 无效温度值 */

/* Exported types ------------------------------------------------------------*/
typedef enum {
    eTemp_NTC_Index_0,
    eTemp_NTC_Index_1,
    eTemp_NTC_Index_2,
    eTemp_NTC_Index_3,
    eTemp_NTC_Index_4,
    eTemp_NTC_Index_5,
    eTemp_NTC_Index_6,
    eTemp_NTC_Index_7,
    eTemp_NTC_Index_8,
} eTemp_NTC_Index;

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
uint16_t gTempADC_Results_Get_By_Index(uint8_t idx);
uint32_t temp_Random_Generate(void);
uint32_t temp_Get_Conv_Cnt(void);
HAL_StatusTypeDef temp_Start_ADC_DMA(void);
HAL_StatusTypeDef temp_Stop_ADC_DMA(void);

float temp_Get_Temp_Data(uint8_t idx);
float temp_Get_Temp_Data_TOP(void);
float temp_Get_Temp_Data_BTM(void);
float temp_Get_Temp_Data_ENV(void);

uint8_t temp_Wait_Stable_BTM(float temp1, float temp2, uint16_t duration);
/* Private defines -----------------------------------------------------------*/

#endif

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __BEEP_H
#define __BEEP_H

/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/
typedef enum {
    eBeep_Freq_do,
    eBeep_Freq_re,
    eBeep_Freq_mi,
    eBeep_Freq_fa,
    eBeep_Freq_so,
    eBeep_Freq_la,
    eBeep_Freq_ti,
} eBeep_Freq;

typedef enum {
    eBeep_Status_Hima,
    eBeep_Status_Isogasi,
} eBeep_Status;

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
uint8_t beep_Start(void);
void beep_Stop(void);
void beep_Deal(uint32_t res);
void beep_Init(void);

/* Private defines -----------------------------------------------------------*/

#endif

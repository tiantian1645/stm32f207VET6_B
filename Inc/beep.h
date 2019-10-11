/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __BEEP_H
#define __BEEP_H

/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/
#define BEEP_TIM_PSC (1 - 1)
#define BEEP_TIM_FREQ (120000000)

#define BEEP_NOTE_C ((uint32_t)((BEEP_TIM_FREQ / (BEEP_TIM_PSC + 1)) / (261.63 * 8)))
#define BEEP_NOTE_D ((uint32_t)((BEEP_TIM_FREQ / (BEEP_TIM_PSC + 1)) / (293.66 * 8)))
#define BEEP_NOTE_E ((uint32_t)((BEEP_TIM_FREQ / (BEEP_TIM_PSC + 1)) / (329.63 * 8)))
#define BEEP_NOTE_F ((uint32_t)((BEEP_TIM_FREQ / (BEEP_TIM_PSC + 1)) / (349.23 * 8)))
#define BEEP_NOTE_G ((uint32_t)((BEEP_TIM_FREQ / (BEEP_TIM_PSC + 1)) / (392.00 * 8)))
#define BEEP_NOTE_A ((uint32_t)((BEEP_TIM_FREQ / (BEEP_TIM_PSC + 1)) / (440.00 * 8)))
#define BEEP_NOTE_B ((uint32_t)((BEEP_TIM_FREQ / (BEEP_TIM_PSC + 1)) / (493.88 * 8)))

/* Exported types ------------------------------------------------------------*/

typedef enum {
    eBeep_Freq_do = BEEP_NOTE_C,
    eBeep_Freq_re = BEEP_NOTE_D,
    eBeep_Freq_mi = BEEP_NOTE_E,
    eBeep_Freq_fa = BEEP_NOTE_F,
    eBeep_Freq_so = BEEP_NOTE_G,
    eBeep_Freq_la = BEEP_NOTE_A,
    eBeep_Freq_ti = BEEP_NOTE_B,
} eBeep_Freq;

typedef enum {
    eBeep_Status_Hima,
    eBeep_Status_Isogasi,
} eBeep_Status;

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void beep_Start(void);
void beep_Start_With_Conf(eBeep_Freq freq, uint16_t t_on, uint16_t t_off, uint16_t period_cnt);
void beep_Start_With_Loop(void);
void beep_Stop(void);
void beep_Deal(uint32_t res);
void beep_Init(void);

void beep_Conf_Set_Freq(eBeep_Freq freq);
void beep_Conf_Set_T_on(uint16_t t_on);
void beep_Conf_Set_T_off(uint16_t t_off);
void beep_Conf_Set_Period_Cnt(uint16_t period_cnt);

/* Private defines -----------------------------------------------------------*/

#endif

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __LED_CTL_H
#define __LED_CTL_H
/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/
typedef enum {
    eLED_OUT_D1_D2_Index_00, /* D1 灭   D2 灭   */
    eLED_OUT_D1_D2_Index_RR, /* D1 红   D2 红   */
    eLED_OUT_D1_D2_Index_G0, /* D1 绿   D2 灭   */
    eLED_OUT_D1_D2_Index_YR, /* D1 红绿 D2 红   */
    eLED_OUT_D1_D2_Index_0G, /* D1 灭   D2 绿   */
    eLED_OUT_D1_D2_Index_RY, /* D1 红   D2 红绿 */
    eLED_OUT_D1_D2_Index_GG, /* D1 绿   D2 绿   */
    eLED_OUT_D1_D2_Index_YY, /* D1 红绿 D2 红绿 */
} eLED_OUT_D1_D2_Index;

typedef enum {
    eLED_Mode_Keep_Green,
    eLED_Mode_Kirakira_Green,
    eLED_Mode_Keep_Red,
    eLED_Mode_Kirakira_Red,
    eLED_Mode_Red_Green,
} eLED_Mode;

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void led_Board_Green_On(void);
void led_Board_Green_Off(void);
void led_Board_Green_Toggle(void);
void led_Out_D1_D2_Set(eLED_OUT_D1_D2_Index idx);
eLED_OUT_D1_D2_Index led_Out_D1_D2_Get(void);

void led_Mode_Set(eLED_Mode mode);
eLED_Mode led_Mode_Get(void);

void led_Out_Deal(TickType_t inTick);

/* Private defines -----------------------------------------------------------*/

#endif

/**
 * @file    led.c
 * @brief   LED控制
 *
 * 外接LED小板上共有4个LED 红色×2 绿色×2 全部独立状态共16个
 * 但控制线只有3条 控制状态共8个 2个红灯共用一条控制线
 *
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "led.h"

/* Extern variables ----------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
static eLED_Mode gLED_Mode = eLED_Mode_Keep_Green;

/* Private constants ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/* Private user code ---------------------------------------------------------*/
/**
 * @brief  板上运行绿灯 点亮
 * @param  None
 * @retval None
 */
void led_Board_Green_On(void)
{
    HAL_GPIO_WritePin(LED_RUN_GPIO_Port, LED_RUN_Pin, GPIO_PIN_RESET);
}

/**
 * @brief  板上运行绿灯 熄灭
 * @param  None
 * @retval None
 */
void led_Board_Green_Off(void)
{
    HAL_GPIO_WritePin(LED_RUN_GPIO_Port, LED_RUN_Pin, GPIO_PIN_SET);
}

/**
 * @brief  板上运行绿灯 翻转
 * @param  None
 * @retval None
 */
void led_Board_Green_Toggle(void)
{
    HAL_GPIO_TogglePin(LED_RUN_GPIO_Port, LED_RUN_Pin);
}

/**
 * @brief  外接LED板控制 灯运行模式
 * @param  mode 参考 eLED_Mode
 * @retval None
 */
void led_Mode_Set(eLED_Mode mode)
{
    gLED_Mode = mode;
}

/**
 * @brief  外接LED板控制 灯运行模式
 * @param  None
 * @retval mode 参考 eLED_Mode
 */
eLED_Mode led_Mode_Get(void)
{
    return gLED_Mode;
}

/**
 * @brief  外接LED板控制 设置
 * @param  idx 外接LED状态枚举 参考 eLED_OUT_D1_D2_Index
 * @retval None
 */
void led_Out_D1_D2_Set(eLED_OUT_D1_D2_Index idx)
{
    if (idx & (1 << 0)) {
        HAL_GPIO_WritePin(LAMP1_GPIO_Port, LAMP1_Pin, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(LAMP1_GPIO_Port, LAMP1_Pin, GPIO_PIN_RESET);
    }
    if (idx & (1 << 1)) {
        HAL_GPIO_WritePin(LAMP2_GPIO_Port, LAMP2_Pin, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(LAMP2_GPIO_Port, LAMP2_Pin, GPIO_PIN_RESET);
    }
    if (idx & (1 << 2)) {
        HAL_GPIO_WritePin(LAMP3_GPIO_Port, LAMP3_Pin, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(LAMP3_GPIO_Port, LAMP3_Pin, GPIO_PIN_RESET);
    }
}

/**
 * @brief  外接LED板控制 读取管脚输出状态
 * @param  None
 * @retval 外接LED状态枚举 参考 eLED_OUT_D1_D2_Index
 */
eLED_OUT_D1_D2_Index led_Out_D1_D2_Get(void)
{
    uint8_t status = 0;

    if (HAL_GPIO_ReadPin(LAMP1_GPIO_Port, LAMP1_Pin) == GPIO_PIN_SET) {
        status |= (1 << 0);
        HAL_GPIO_WritePin(LAMP1_GPIO_Port, LAMP1_Pin, GPIO_PIN_SET);
    }
    if (HAL_GPIO_ReadPin(LAMP2_GPIO_Port, LAMP2_Pin) == GPIO_PIN_SET) {
        status |= (1 << 1);
        HAL_GPIO_WritePin(LAMP2_GPIO_Port, LAMP2_Pin, GPIO_PIN_SET);
    }
    if (HAL_GPIO_ReadPin(LAMP3_GPIO_Port, LAMP3_Pin) == GPIO_PIN_SET) {
        status |= (1 << 2);
        HAL_GPIO_WritePin(LAMP3_GPIO_Port, LAMP3_Pin, GPIO_PIN_SET);
    }
    return (eLED_OUT_D1_D2_Index)status;
}

/**
 * @brief  外接LED板引用
 * @param  inTick 系统时刻
 * @retval None
 */
void led_Out_Deal(TickType_t inTick)
{
    switch (led_Mode_Get()) {
        case eLED_Mode_Keep_Green:
            led_Out_D1_D2_Set(eLED_OUT_D1_D2_Index_GG);
            break;
        case eLED_Mode_Kirakira_Green:
            if (inTick % 1000 < 500) {
                led_Out_D1_D2_Set(eLED_OUT_D1_D2_Index_00);
            } else {
                led_Out_D1_D2_Set(eLED_OUT_D1_D2_Index_GG);
            }

            break;
        case eLED_Mode_Keep_Red:
            led_Out_D1_D2_Set(eLED_OUT_D1_D2_Index_RR);
            break;
        case eLED_Mode_Kirakira_Red:
            if (inTick % 1000 < 500) {
                led_Out_D1_D2_Set(eLED_OUT_D1_D2_Index_00);
            } else {
                led_Out_D1_D2_Set(eLED_OUT_D1_D2_Index_RR);
            }
            break;
        default:
            led_Mode_Set(eLED_Mode_Keep_Green);
            led_Out_D1_D2_Set(eLED_OUT_D1_D2_Index_GG);
            break;
    }
}

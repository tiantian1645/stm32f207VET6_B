/**
 * @file    temperature.c
 * @brief   温度测控
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <math.h>
#include "temperature.h"
#include "storge_task.h"

/* Extern variables ----------------------------------------------------------*/
extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim2;

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
#define TEMP_NTC_TK (273.15)           /* 绝对零度 热力学零度 */
#define TEMP_NTC_B (3240)              /* NTC 温度探头 25摄氏度时 B值(电阻对数差 与 温度倒数差 之商) B25_37 (3238 ~ 3459) */
#define TEMP_NTC_TA (TEMP_NTC_TK + 25) /* 25摄氏度换算成热力学温度 */
#define TEMP_NTC_S 4095                /* 12位ADC 转换最大值 */

#define TEMP_NTC_TOP_NUM 6
#define TEMP_NTC_BTM_NUM 2

/* Private macro -------------------------------------------------------------*/
const eTemp_NTC_Index TEMP_NTC_TOP_IDXS[TEMP_NTC_TOP_NUM] = {eTemp_NTC_Index_0, eTemp_NTC_Index_1, eTemp_NTC_Index_2,
                                                             eTemp_NTC_Index_3, eTemp_NTC_Index_4, eTemp_NTC_Index_5};
const eTemp_NTC_Index TEMP_NTC_BTM_IDXS[TEMP_NTC_BTM_NUM] = {eTemp_NTC_Index_6, eTemp_NTC_Index_7};

#define TEMP_NTC_ENV_1 eTemp_NTC_Index_8

#define TEMP_NTC_NUM (9)                                              /* 温度探头数目 */
#define TEMP_STA_NUM (15)                                             /* 统计滤波缓存长度 */
#define TEMP_STA_HEAD (3)                                             /* 统计滤波去掉头部长度 */
#define TEMP_STA_TAIL (3)                                             /* 统计滤波去掉尾部长度 */
#define TEMP_STA_VAILD (TEMP_STA_NUM - TEMP_STA_HEAD - TEMP_STA_TAIL) /* 统计滤波中位有效长度 */

/* Private variables ---------------------------------------------------------*/
static uint32_t gTempADC_DMA_Buffer[TEMP_NTC_NUM * TEMP_STA_NUM];
static uint32_t gTempADC_Statictic_buffer[TEMP_NTC_NUM][TEMP_STA_NUM];
static uint32_t gTempADC_Results[TEMP_NTC_NUM];
static uint32_t gTempADC_Conv_Cnt = 0;

/* Private constants ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/* Private user code ---------------------------------------------------------*/

/**
 * @brief  温度 ADC 采样值做随机数
 * @param  None
 * @retval ADC 采样值之和
 */
uint32_t temp_Random_Generate(void)
{
    uint8_t i;
    uint32_t ran = 0;

    for (i = 0; i < ARRAY_LEN(gTempADC_Results); ++i) {
        ran += gTempADC_Results[i];
    }
    return ran;
}

/**
 * @brief  温度 ADC 采样次数 读取
 * @param  None
 * @retval ADC 采样次数
 */
uint32_t temp_Get_Conv_Cnt(void)
{
    return gTempADC_Conv_Cnt;
}

/**
 * @brief  温度 ADC 采样 启动
 * @param  None
 * @retval DMA启动结果
 */
HAL_StatusTypeDef temp_Start_ADC_DMA(void)
{
    HAL_ADC_Start_DMA(&hadc1, gTempADC_DMA_Buffer, ARRAY_LEN(gTempADC_DMA_Buffer));
    HAL_TIM_Base_Start_IT(&htim2);
    return HAL_OK;
}

/**
 * @brief  温度 ADC 采样 停止
 * @param  None
 * @retval DMA停止结果
 */
HAL_StatusTypeDef temp_Stop_ADC_DMA(void)
{
    return HAL_ADC_Stop_DMA(&hadc1);
}

/**
 * @brief  温度 ADC 值 转换成摄氏度
 * @param  sample_hex adc 采样值
 * @note   log 自然对数计算涉及 双精度浮点数 为非原子操作
 * @note   转换操作只在最终转换结果时使用 采用非中断版本开关中断
 * @retval 摄氏度温度值
 */
float temp_ADC_2_Temp(uint32_t sample_hex)
{
    float er;

    er = (float)(sample_hex) / (float)(TEMP_NTC_S - sample_hex);
    er = log(er) / TEMP_NTC_B;
    er = (float)(1) / TEMP_NTC_TA - er;
    er = ((float)(1) / er) - TEMP_NTC_TK;
    return er;
}

/**
 * @brief  数组排序
 * @note
 * @param  pData 数组指针 length 数组长度
 * @retval None
 */
void temp_Sort_Data(uint32_t * pData, uint8_t length)
{
    uint8_t i, j;
    uint32_t temp;

    for (i = 0; i < length - 1; i++) {
        // Last i elements are already in place
        for (j = 0; j < length - i - 1; j++)
            if (pData[j] > pData[j + 1]) {
                temp = pData[j];
                pData[j] = pData[j + 1];
                pData[j + 1] = temp;
            }
    }
}

/**
 * @brief  温度 ADC 裸数据滤波
 * @note   中位区间平均值 从 gTempADC_Statictic_buffer[i] 中 取中位区间 算出平均值--> gTempADC_Results[i]
 * @note   中位区间起始点 TEMP_STA_HEAD 中位区间长度 TEMP_STA_VAILD
 * @param  None
 * @retval None
 */
void temp_Filter_Deal(void)
{
    uint8_t i, j;
    uint32_t temp;

    for (i = 0; i < TEMP_NTC_NUM; ++i) {
        temp_Sort_Data(gTempADC_Statictic_buffer[i], TEMP_STA_NUM);
        temp = 0;
        for (j = 0; j < TEMP_STA_VAILD; ++j) {
            temp += gTempADC_Statictic_buffer[i][TEMP_STA_HEAD + j];
        }
        gTempADC_Results[i] = temp / TEMP_STA_VAILD;
    }
}

/**
 * @brief  温度 ADC 获取温度值 裸数据
 * @param  idx 探头索引
 * @retval 温度值 摄氏度
 */
float temp_Get_Temp_Data(eTemp_NTC_Index idx)
{
    float temp;
    uStorgeParamItem up;

    temp = temp_ADC_2_Temp(gTempADC_Results[idx]);
    up.f32 = 0;
    if (storge_ParamGet(eStorgeParamIndex_Temp_CC_top_1 + idx, up.u8s) == 4) {
        if (up.f32 > 5 || up.f32 < -5) {
            up.f32 = 0;
        }
        temp += up.f32;
    }

    return temp;
}

/**
 * @brief  温度浮点类型绝对值
 * @param  f 数据
 * @retval 绝对值
 */
float temp_fabs(float f)
{
    if (f < 0) {
        return f * -1.0;
    }
    return f;
}

/**
 * @brief  上加热体温度 PID输入处理
 * @note   37度附近时取偏离最远温度值 其他情况取平均值
 * @param  pData length 数组描述
 * @param  theshold 偏远判断阈值
 * @retval 温度值 摄氏度
 */
float temp_Deal_Temp_Data(float * pData, uint8_t length, float theshold)
{
    uint8_t i;
    float max, min, sum;

    if (length == 0) {
        return TEMP_INVALID_DATA;
    }

    max = pData[0];
    min = pData[0];
    sum = pData[0];

    for (i = 1; i < length; ++i) {
        if (pData[i] > max) {
            max = pData[i];
        }
        if (pData[i] < min) {
            min = pData[i];
        }
        sum += pData[i];
    }

    if (temp_fabs(max - 37) + temp_fabs(min - 37) < theshold) {
        return (temp_fabs(max - 37) > temp_fabs(min - 37)) ? (max) : (min);
    }
    return sum / length;
}

/**
 * @brief  温度 ADC 获取温度值 上加热体 最大偏差值
 * @note   37度附近 采用 temp_Deal_Temp_Data 取最大偏差值
 * @param  None
 * @retval 温度值 摄氏度
 */
float temp_Get_Temp_Data_TOP(void)
{
    float temp_list[TEMP_NTC_TOP_NUM], temp;
    uint8_t valid = 0, i;

    for (i = 0; i < TEMP_NTC_TOP_NUM; ++i) {
        temp = temp_Get_Temp_Data(TEMP_NTC_TOP_IDXS[i]);
        if (temp > 0 && temp < 55) {
            temp_list[valid] = temp;
            ++valid;
        }
    }
    return temp_Deal_Temp_Data(temp_list, valid, 0.2);
}

/**
 * @brief  温度 ADC 获取温度值 下加热体
 * @note   采用平均值
 * @param  None
 * @retval 温度值 摄氏度
 */
float temp_Get_Temp_Data_BTM(void)
{
    float temp = 0, sum = 0;
    uint8_t valid = 0, i;

    for (i = 0; i < TEMP_NTC_BTM_NUM; ++i) {
        temp = temp_Get_Temp_Data(TEMP_NTC_BTM_IDXS[i]);
        if (temp > 0 && temp < 55) {
            sum += temp;
            ++valid;
        }
    }

    if (valid > 0) {
        return sum / valid;
    } else {
        return TEMP_INVALID_DATA;
    }
}

/**
 * @brief  温度 ADC 获取温度值 环境
 * @param  None
 * @retval 温度值 摄氏度
 */
float temp_Get_Temp_Data_ENV(void)
{
    float temp = 0;
    temp = temp_Get_Temp_Data(TEMP_NTC_ENV_1);
    if (temp > 0 && temp < 55) {
        return temp;
    }
    return TEMP_INVALID_DATA;
}

/**
 * @brief  温度 ADC 采样完成回调
 * @param  hadc ADC 句柄指针
 * @retval None
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef * hadc)
{
    uint8_t i, j;
    if (hadc == (&hadc1)) {
        for (i = 0; i < TEMP_NTC_NUM; ++i) {
            for (j = 0; j < TEMP_STA_NUM; ++j) {
                gTempADC_Statictic_buffer[i][j] = gTempADC_DMA_Buffer[i + TEMP_NTC_NUM * j];
            }
        }
        ++gTempADC_Conv_Cnt;
        if (gTempADC_Conv_Cnt % TEMP_STA_NUM == 0) {
            temp_Filter_Deal();
            if (gTempADC_Conv_Cnt == 0xffffffa0) {
                gTempADC_Conv_Cnt = 0;
            }
        }
    }
}

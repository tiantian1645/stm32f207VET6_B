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
typedef struct {
    float r;
    float b;
} sRBP_Type;

/* Private define ------------------------------------------------------------*/
#define TEMP_RANGE_START (0)
#define TEMP_RANGE_STOP (75)

#define TEMP_NTC_TK (273.15)           /* 绝对零度 热力学零度 */
#define TEMP_NTC_B (3240)              /* NTC 温度探头 25摄氏度时 B值(电阻对数差 与 温度倒数差 之商) B25_37 (3238 ~ 3459) */
#define TEMP_NTC_TA (TEMP_NTC_TK + 25) /* 25摄氏度换算成热力学温度 */
#define TEMP_NTC_S 4095                /* 12位ADC 转换最大值 */

#define TEMP_NTC_TOP_NUM 2
#define TEMP_NTC_BTM_NUM 1

/* Private macro -------------------------------------------------------------*/
const eTemp_NTC_Index TEMP_NTC_TOP_IDXS[TEMP_NTC_TOP_NUM] = {eTemp_NTC_Index_0, eTemp_NTC_Index_1};
const eTemp_NTC_Index TEMP_NTC_BTM_IDXS[TEMP_NTC_BTM_NUM] = {eTemp_NTC_Index_6};

#define TEMP_NTC_ENV_1 eTemp_NTC_Index_8

#define TEMP_NTC_NUM (9)                                              /* 温度探头数目 */
#define TEMP_STA_NUM (15)                                             /* 统计滤波缓存长度 */
#define TEMP_STA_HEAD (3)                                             /* 统计滤波去掉头部长度 */
#define TEMP_STA_TAIL (3)                                             /* 统计滤波去掉尾部长度 */
#define TEMP_STA_VAILD (TEMP_STA_NUM - TEMP_STA_HEAD - TEMP_STA_TAIL) /* 统计滤波中位有效长度 */

/* Private variables ---------------------------------------------------------*/
static uint32_t gTempADC_DMA_Buffer[TEMP_NTC_NUM * TEMP_STA_NUM];
static uint32_t gTempADC_Statictic_buffer[TEMP_NTC_NUM][TEMP_STA_NUM];
static uint16_t gTempADC_Results[TEMP_NTC_NUM];
static uint32_t gTempADC_Conv_Cnt = 0;

/* Private constants ---------------------------------------------------------*/
/* 15 ℃ 1653 ADC 14.773140 kΩ */
/* 22 ℃ 1925 ADC 11.272727 kΩ */
/* 27 ℃ 2120 ADC  9.316038 kΩ */
/* 32 ℃ 2307 ADC  7.750325 kΩ */
/* 37 ℃ 2489 ADC  6.562391 kΩ */

static const sRBP_Type cRB_Pairs[124] = {
    {27.513, 3296.9174642092316}, {26.271, 3289.5406289495836}, {25.162, 3291.2469316004663}, {24.107, 3293.0548811901517}, {23.101, 3294.630603545126},
    {22.144, 3296.407527512391},  {21.231, 3297.942193625932},  {20.362, 3299.701481822821},  {19.533, 3301.3205639087555}, {18.742, 3302.7850157150087},
    {18.016, 3313.1098094200065}, {17.269, 3306.037786383082},  {16.583, 3307.787759468269},  {15.928, 3309.488790349294},  {15.302, 3310.9059783556045},
    {14.704, 3312.200773274644},  {14.134, 3314.2806148700643}, {13.588, 3315.4487926769025}, {13.067, 3317.303975134466},  {12.568, 3318.229479281136},
    {12.092, 3320.5845720538455}, {11.636, 3322.0775055050126}, {11.199, 3321.6501532979987}, {10.782, 3324.076917383555},  {10.382, 3321.2989481929408},
    {9.633, 3334.9076107902683},  {9.282, 3333.852456580904},   {8.946, 3333.48215236086},    {8.623, 3336.60702527403},    {8.314, 3337.7835877376588},
    {8.018, 3338.561575724125},   {7.734, 3339.748873956778},   {7.461, 3341.888813780338},   {7.199, 3344.006898438757},   {6.948, 3345.4529878848857},
    {6.707, 3346.9998024233096},  {6.476, 3348.090581802104},   {6.254, 3349.4298731165095},  {6.04, 3351.6345788889953},   {5.835, 3353.139911316231},
    {5.638, 3354.661712231957},   {5.449, 3355.8407238071645},  {5.267, 3357.3578417599624},  {5.091, 3359.8585633223497},  {4.923, 3361.080781281653},
    {4.761, 3362.705330763885},   {4.605, 3364.458656578187},   {4.455, 3366.092914682925},   {4.311, 3367.3794671039473},  {4.168, 3372.7244517166555},
    {4.038, 3370.8237083480913},  {3.909, 3372.5682822528647},  {3.785, 3374.0731687134453},  {3.665, 3376.0760616223547},  {3.55, 3377.48734788177},
    {3.439, 3379.060381009354},   {3.332, 3380.6383759321657},  {3.229, 3382.070243095161},   {3.129, 3384.139836152872},   {3.033, 3385.7820188160017},
    {2.941, 3386.856541521189},   {2.851, 3389.11824729597},    {2.765, 3390.559991981822},   {2.682, 3392.0060782582796},  {2.602, 3393.341591599563},
    {2.524, 3395.429799046603},   {2.449, 3397.1980717202514},  {2.377, 3398.5346455118074},  {2.307, 3400.3322330124624},  {2.24, 3401.49318013811},
    {2.175, 3402.930130493356},   {2.112, 3404.5615177839786},  {2.051, 3406.30643720193},    {1.992, 3408.084233274003},   {1.935, 3409.814146994328},
    {1.88, 3411.415020947881},    {1.827, 3412.805062479552},   {1.776, 3413.901666136739},   {1.726, 3415.747463003872},   {1.678, 3417.1598404270817},
    {1.632, 3418.0534923703012},  {1.587, 3419.5120981118084},  {1.543, 3421.4935745957223},  {1.501, 3422.753823278711},   {1.461, 3423.202354405346},
    {1.421, 3425.2165427942905},  {1.383, 3426.299232451841},   {1.346, 3427.624662180451},   {1.31, 3429.1482970000916},   {1.275, 3430.8252225349893},
    {1.242, 3431.2849663512548},  {1.209, 3433.111916911746},   {1.178, 3433.589842928027},   {1.147, 3435.3774343556593},  {1.117, 3437.0825091617507},
    {1.089, 3437.2311593841623},  {1.061, 3438.59957200551},    {1.034, 3439.732616806538},   {1.008, 3440.577220053995},   {0.983, 3441.079209470028},
    {0.958, 3442.7147238702664},  {0.934, 3443.940735776823},   {0.911, 3444.6998009889803},  {0.889, 3444.933411164917},   {0.867, 3446.2067367646496},
    {0.846, 3446.8813663440346},  {0.825, 3448.569784341653},   {0.805, 3449.5836413195266},  {0.786, 3449.8582799113547},  {0.767, 3451.079569962614},
    {0.749, 3451.48033973496},    {0.732, 3450.9927930470853},  {0.714, 3453.2088026556708},  {0.698, 3452.6448354100235},  {0.682, 3452.9193878095216},
    {0.666, 3454.04180264026},    {0.651, 3454.079932729298},   {0.636, 3454.9292382287053},  {0.622, 3454.599089784649},   {0.608, 3455.040856355746},
    {0.594, 3456.2640405687043},  {0.581, 3456.1878006447055},  {0.568, 3456.8513098334683},  {0.556, 3456.111284160398},
};

/* Private function prototypes -----------------------------------------------*/

/* Private user code ---------------------------------------------------------*/

/**
 * @brief  原始ADC值获取
 * @param  idx 索引
 * @retval ADC值
 */
uint16_t gTempADC_Results_Get_By_Index(uint8_t idx)
{
    return gTempADC_Results[idx % ARRAY_LEN(gTempADC_Results)];
}

/**
 * @brief  通过电阻值计算B值
 * @param  None
 * @retval B值
 */
float temp_get_B_by_R(float r)
{
    float b = 3240;
    uint8_t i;

    for (i = 0; i < ARRAY_LEN(cRB_Pairs) - 1; ++i) {
        if (r < cRB_Pairs[i].r && r > cRB_Pairs[i + 1].r) {
            b = cRB_Pairs[i].b * (cRB_Pairs[i].r - r) + cRB_Pairs[i + 1].b * (r - cRB_Pairs[i + 1].r);
            b /= (cRB_Pairs[i].r - cRB_Pairs[i + 1].r);
            return b;
        }
    }
    return b;
}

/**
 * @brief  温度 ADC 采样值做随机数
 * @param  None
 * @retval ADC 采样值之和
 */
uint32_t temp_Random_Generate(void)
{
    uint8_t i;
    uint32_t ran;

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
 * @note   R = 10k * (4095 - sample) / sample
 * @retval 摄氏度温度值
 */
float temp_ADC_2_Temp(uint32_t sample_hex)
{
    float er;

    if (sample_hex < 75) { /* -55℃ | r = 541.187 | (10 / (10 + r)) * 4095 = 74.29420505200594 */
        return -60;
    }

    er = (float)(sample_hex) / (float)(TEMP_NTC_S - sample_hex);
    er = log(er) / temp_get_B_by_R(10.0 / er);
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
    eStorgeParamIndex s_idx;

    temp = temp_ADC_2_Temp(gTempADC_Results[idx]);
    up.f32 = 0;
    switch (idx) {
        case eTemp_NTC_Index_0:
        case eTemp_NTC_Index_1:
        case eTemp_NTC_Index_2:
        case eTemp_NTC_Index_3:
        case eTemp_NTC_Index_4:
        case eTemp_NTC_Index_5:
            s_idx = eStorgeParamIndex_Temp_CC_top;
            break;
        case eTemp_NTC_Index_6:
        case eTemp_NTC_Index_7:
            s_idx = eStorgeParamIndex_Temp_CC_btm;
            break;
        case eTemp_NTC_Index_8:
            s_idx = eStorgeParamIndex_Temp_CC_env;
            break;
        default:
        	return TEMP_INVALID_DATA;
    }
    if (storge_ParamReadSingle(s_idx, up.u8s) == 4) {
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
        if (temp > TEMP_RANGE_START && temp < TEMP_RANGE_STOP) {
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
        if (temp > TEMP_RANGE_START && temp < TEMP_RANGE_STOP) {
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
    if (temp > TEMP_RANGE_START && temp < TEMP_RANGE_STOP) {
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

/**
 * @brief  下加热体温度稳定等待
 * @param  temp1 temp2 温度范围
 * @param  timeout 超时时间 单位秒
 * @retval 0 稳定 1 未稳
 */
uint8_t temp_Wait_Stable_BTM(float temp1, float temp2, uint16_t duration)
{
    TickType_t xTick;
    float temp;
    static float records[4] = {0, 0, 0, 0};
    uint8_t i = 0, j;

    xTick = xTaskGetTickCount();

    if (temp1 > temp2) {
        temp = temp2;
        temp2 = temp1;
        temp1 = temp;
    }

    do {
        vTaskDelay(400);
        temp = temp_Get_Temp_Data_BTM();
        records[i % 4] = temp;
        ++i;
        for (j = 0; j < ARRAY_LEN(records); ++j) {
            if (records[j] < temp1 || records[j] > temp2) { /* 存在超范围数据 */
                break;
            }
        }
        if (j == ARRAY_LEN(records)) { /* 全部在 范围内 */
            return 0;
        }
    } while ((xTaskGetTickCount() - xTick) / (pdMS_TO_TICKS(1000)) < duration);
    return 1;
}

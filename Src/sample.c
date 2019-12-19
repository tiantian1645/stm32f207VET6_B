/**
 * @file    sample.c
 * @brief   采样数据处理
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "sample.h"
#include "storge_task.h"
#include "comm_data.h"

/* Extern variables ----------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Private constants ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
static void sample_first_degree_cal_param(uint32_t x0, uint32_t x1, uint32_t y0, uint32_t y1, float * pk, float * pb);
static void sample_first_degree_cal_by_index(eStorgeParamIndex norm_idx, uint32_t input, uint32_t * pOutput);

/* Private user code ---------------------------------------------------------*/

/**
 * @brief  无符号求差
 * @param  a, b 被减数 减数
 * @retval 差值
 */
static int32_t sample_sub(uint32_t a, uint32_t b)
{
    return (a > b) ? (a - b) : (-1 * (b - a));
}

/**
 * @brief  两点间直线参数
 * @param  x0 x1 y0 y1
 * @param  pk 直线斜率 pb 直线截距
 * @retval None
 */
static void sample_first_degree_cal_param(uint32_t x0, uint32_t x1, uint32_t y0, uint32_t y1, float * pk, float * pb)
{
    float m;

    if (x0 == x1) { /* 斜率无穷 */
        *pk = 1;
        *pb = 0;
        return;
    }

    m = sample_sub(x0, x1);
    *pk = sample_sub(y0, y1) / m;
    *pb = sample_sub(x0 * y1, x1 * y0) / m;
}

/**
 * @brief  根据校正点投影到标准点
 * @param  norm_idx 标准点存储索引
 * @param  input 输入值
 * @param  pOutput 输出指针
 * @retval None
 */
static void sample_first_degree_cal_by_index(eStorgeParamIndex norm_idx, uint32_t input, uint32_t * pOutput)
{
    float k, b;
    uint32_t x0, x1, y0, y1;
    uStorgeParamItem read_data;

    storge_ParamReadSingle(norm_idx + 6, read_data.u8s); /* 校正时左端测试点 */
    x0 = read_data.u32;
    storge_ParamReadSingle(norm_idx + 7, read_data.u8s); /* 校正时右端测试点 */
    x1 = read_data.u32;

    storge_ParamReadSingle(norm_idx + 0, read_data.u8s); /* 左端测试点对应标准点 */
    y0 = read_data.u32;
    storge_ParamReadSingle(norm_idx + 1, read_data.u8s); /* 右端测试点对应标准点 */
    y1 = read_data.u32;
    sample_first_degree_cal_param(x0, x1, y0, y1, &k, &b); /* 计算直线拟合 */
    *pOutput = k * input + b + 0.5;                        /* 取整误差 */
}

/**
 * @brief  根据校正点投影到标准点
 * @param  channel 通道索引
 * @param  wave 波长索引
 * @param  input 输入值
 * @param  pOutput 输出指针
 * @retval 0 正常 1 数据异常 2 范围丢失
 */
uint8_t sample_first_degree_cal(uint8_t channel, uint8_t wave, uint32_t input, uint32_t * pOutput)
{
    uint8_t i;
    eStorgeParamIndex norm_start;
    uStorgeParamItem read_data;

    switch (channel) {
        case 1:
            if (wave < eComm_Data_Sample_Radiant_610 || wave > eComm_Data_Sample_Radiant_405) { /* 只有第一通道有405 */
                return 1;                                                                       /* 波长索引越限 */
            }
            norm_start = eStorgeParamIndex_Illumine_CC_t1_610_i0 + (wave - eComm_Data_Sample_Radiant_610) * 12; /* 标准点 */
            break;
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
            if (wave < eComm_Data_Sample_Radiant_610 || wave > eComm_Data_Sample_Radiant_550) { /* 其余通道只有610和550 */
                return 1;                                                                       /* 波长索引越限 */
            }
            norm_start = eStorgeParamIndex_Illumine_CC_t2_610_i0 + (channel - 2) * 24 + (wave - eComm_Data_Sample_Radiant_610) * 12; /* 标准点 */
            break;
        default:
            return 1; /* 通道索引越限 */
    }
    for (i = 0; i < 6; ++i) {
        storge_ParamReadSingle(norm_start + 6 + i, read_data.u8s); /* 遍历每个测试点 */
        if (read_data.u32 == 0) {                                  /* 存在零值测试点 */
            *pOutput = input;                                      /* 不经投影 */
            return 0;                                              /* 提前返回 */
        }
    }

    /* below point 0 or point 1 */                                    /* x <- P0 or x <- P1*/
    storge_ParamReadSingle(norm_start + 6 + 1, read_data.u8s);        /* 校正时第二测试点 */
    if (input < read_data.u32) {                                      /* 测试值小于最小校准点测试值 */
        sample_first_degree_cal_by_index(norm_start, input, pOutput); /* 使用第一段 */
        return 0;
    }
    /* overtake point 4 or point 5 */                                     /* P4 -> x or P5 -> x*/
    storge_ParamReadSingle(norm_start + 6 + 4, read_data.u8s);            /* 校正时第五测试点 */
    if (input >= read_data.u32) {                                         /* 测试值大于最大校准点测试值 */
        sample_first_degree_cal_by_index(norm_start + 4, input, pOutput); /* 使用第五段 */
        return 0;
    }
    /* below point 2 and point 3 and point 4 */                                   /* x <- P2 or x <- P3 or x <- P4 */
    for (i = 2; i < 5; ++i) {                                                     /* 检测中间点 */
        storge_ParamReadSingle(norm_start + 6 + i, read_data.u8s);                /* 校正时第二、三、四、五、六测试点 */
        if (input < read_data.u32) {                                              /* 测试值小于该校准点测试值 */
            sample_first_degree_cal_by_index(norm_start + i - 1, input, pOutput); /* 使用第一、二、三、四、五段 */
            return 0;
        }
    }
    *pOutput = input;
    return 2;
}

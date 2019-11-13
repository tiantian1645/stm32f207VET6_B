/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STORGE_TASK_H
#define __STORGE_TASK_H
/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/
#include "protocol.h"

/* Exported types ------------------------------------------------------------*/
typedef enum {
    eStorgeNotifyConf_Read_Falsh = 0x00000001,
    eStorgeNotifyConf_Write_Falsh = 0x00000002,
    eStorgeNotifyConf_Load_Parmas = 0x00000004,
    eStorgeNotifyConf_Dump_Params = 0x00000008,
    eStorgeNotifyConf_Read_ID_Card = 0x00000010,
    eStorgeNotifyConf_Write_ID_Card = 0x00000020,
    eStorgeNotifyConf_COMM_Out = 0x10000000,
    eStorgeNotifyConf_COMM_Main = 0x20000000,
} eStorgeNotifyConf;

typedef enum {
    eStorgeParamIndex_Temp_CC_top_1,
    eStorgeParamIndex_Temp_CC_top_2,
    eStorgeParamIndex_Temp_CC_top_3,
    eStorgeParamIndex_Temp_CC_top_4,
    eStorgeParamIndex_Temp_CC_top_5,
    eStorgeParamIndex_Temp_CC_top_6,
    eStorgeParamIndex_Temp_CC_btm_1,
    eStorgeParamIndex_Temp_CC_btm_2,
    eStorgeParamIndex_Temp_CC_env,

    eStorgeParamIndex_Illumine_CC_t1_610_i0,
    eStorgeParamIndex_Illumine_CC_t1_610_i1,
    eStorgeParamIndex_Illumine_CC_t1_610_i2,
    eStorgeParamIndex_Illumine_CC_t1_610_i3,
    eStorgeParamIndex_Illumine_CC_t1_610_i4,
    eStorgeParamIndex_Illumine_CC_t1_610_i5,

    eStorgeParamIndex_Illumine_CC_t1_610_o0,
    eStorgeParamIndex_Illumine_CC_t1_610_o1,
    eStorgeParamIndex_Illumine_CC_t1_610_o2,
    eStorgeParamIndex_Illumine_CC_t1_610_o3,
    eStorgeParamIndex_Illumine_CC_t1_610_o4,
    eStorgeParamIndex_Illumine_CC_t1_610_o5,

    eStorgeParamIndex_Illumine_CC_t1_550_i0,
    eStorgeParamIndex_Illumine_CC_t1_550_i1,
    eStorgeParamIndex_Illumine_CC_t1_550_i2,
    eStorgeParamIndex_Illumine_CC_t1_550_i3,
    eStorgeParamIndex_Illumine_CC_t1_550_i4,
    eStorgeParamIndex_Illumine_CC_t1_550_i5,

    eStorgeParamIndex_Illumine_CC_t1_550_o0,
    eStorgeParamIndex_Illumine_CC_t1_550_o1,
    eStorgeParamIndex_Illumine_CC_t1_550_o2,
    eStorgeParamIndex_Illumine_CC_t1_550_o3,
    eStorgeParamIndex_Illumine_CC_t1_550_o4,
    eStorgeParamIndex_Illumine_CC_t1_550_o5,

    eStorgeParamIndex_Illumine_CC_t1_405_i0,
    eStorgeParamIndex_Illumine_CC_t1_405_i1,
    eStorgeParamIndex_Illumine_CC_t1_405_i2,
    eStorgeParamIndex_Illumine_CC_t1_405_i3,
    eStorgeParamIndex_Illumine_CC_t1_405_i4,
    eStorgeParamIndex_Illumine_CC_t1_405_i5,

    eStorgeParamIndex_Illumine_CC_t1_405_o0,
    eStorgeParamIndex_Illumine_CC_t1_405_o1,
    eStorgeParamIndex_Illumine_CC_t1_405_o2,
    eStorgeParamIndex_Illumine_CC_t1_405_o3,
    eStorgeParamIndex_Illumine_CC_t1_405_o4,
    eStorgeParamIndex_Illumine_CC_t1_405_o5,

    eStorgeParamIndex_Illumine_CC_t2_610_i0,
    eStorgeParamIndex_Illumine_CC_t2_610_i1,
    eStorgeParamIndex_Illumine_CC_t2_610_i2,
    eStorgeParamIndex_Illumine_CC_t2_610_i3,
    eStorgeParamIndex_Illumine_CC_t2_610_i4,
    eStorgeParamIndex_Illumine_CC_t2_610_i5,

    eStorgeParamIndex_Illumine_CC_t2_610_o0,
    eStorgeParamIndex_Illumine_CC_t2_610_o1,
    eStorgeParamIndex_Illumine_CC_t2_610_o2,
    eStorgeParamIndex_Illumine_CC_t2_610_o3,
    eStorgeParamIndex_Illumine_CC_t2_610_o4,
    eStorgeParamIndex_Illumine_CC_t2_610_o5,

    eStorgeParamIndex_Illumine_CC_t2_550_i0,
    eStorgeParamIndex_Illumine_CC_t2_550_i1,
    eStorgeParamIndex_Illumine_CC_t2_550_i2,
    eStorgeParamIndex_Illumine_CC_t2_550_i3,
    eStorgeParamIndex_Illumine_CC_t2_550_i4,
    eStorgeParamIndex_Illumine_CC_t2_550_i5,

    eStorgeParamIndex_Illumine_CC_t2_550_o0,
    eStorgeParamIndex_Illumine_CC_t2_550_o1,
    eStorgeParamIndex_Illumine_CC_t2_550_o2,
    eStorgeParamIndex_Illumine_CC_t2_550_o3,
    eStorgeParamIndex_Illumine_CC_t2_550_o4,
    eStorgeParamIndex_Illumine_CC_t2_550_o5,

    eStorgeParamIndex_Illumine_CC_t3_610_i0,
    eStorgeParamIndex_Illumine_CC_t3_610_i1,
    eStorgeParamIndex_Illumine_CC_t3_610_i2,
    eStorgeParamIndex_Illumine_CC_t3_610_i3,
    eStorgeParamIndex_Illumine_CC_t3_610_i4,
    eStorgeParamIndex_Illumine_CC_t3_610_i5,

    eStorgeParamIndex_Illumine_CC_t3_610_o0,
    eStorgeParamIndex_Illumine_CC_t3_610_o1,
    eStorgeParamIndex_Illumine_CC_t3_610_o2,
    eStorgeParamIndex_Illumine_CC_t3_610_o3,
    eStorgeParamIndex_Illumine_CC_t3_610_o4,
    eStorgeParamIndex_Illumine_CC_t3_610_o5,

    eStorgeParamIndex_Illumine_CC_t3_550_i0,
    eStorgeParamIndex_Illumine_CC_t3_550_i1,
    eStorgeParamIndex_Illumine_CC_t3_550_i2,
    eStorgeParamIndex_Illumine_CC_t3_550_i3,
    eStorgeParamIndex_Illumine_CC_t3_550_i4,
    eStorgeParamIndex_Illumine_CC_t3_550_i5,

    eStorgeParamIndex_Illumine_CC_t3_550_o0,
    eStorgeParamIndex_Illumine_CC_t3_550_o1,
    eStorgeParamIndex_Illumine_CC_t3_550_o2,
    eStorgeParamIndex_Illumine_CC_t3_550_o3,
    eStorgeParamIndex_Illumine_CC_t3_550_o4,
    eStorgeParamIndex_Illumine_CC_t3_550_o5,

    eStorgeParamIndex_Illumine_CC_t4_610_i0,
    eStorgeParamIndex_Illumine_CC_t4_610_i1,
    eStorgeParamIndex_Illumine_CC_t4_610_i2,
    eStorgeParamIndex_Illumine_CC_t4_610_i3,
    eStorgeParamIndex_Illumine_CC_t4_610_i4,
    eStorgeParamIndex_Illumine_CC_t4_610_i5,

    eStorgeParamIndex_Illumine_CC_t4_610_o0,
    eStorgeParamIndex_Illumine_CC_t4_610_o1,
    eStorgeParamIndex_Illumine_CC_t4_610_o2,
    eStorgeParamIndex_Illumine_CC_t4_610_o3,
    eStorgeParamIndex_Illumine_CC_t4_610_o4,
    eStorgeParamIndex_Illumine_CC_t4_610_o5,

    eStorgeParamIndex_Illumine_CC_t4_550_i0,
    eStorgeParamIndex_Illumine_CC_t4_550_i1,
    eStorgeParamIndex_Illumine_CC_t4_550_i2,
    eStorgeParamIndex_Illumine_CC_t4_550_i3,
    eStorgeParamIndex_Illumine_CC_t4_550_i4,
    eStorgeParamIndex_Illumine_CC_t4_550_i5,

    eStorgeParamIndex_Illumine_CC_t4_550_o0,
    eStorgeParamIndex_Illumine_CC_t4_550_o1,
    eStorgeParamIndex_Illumine_CC_t4_550_o2,
    eStorgeParamIndex_Illumine_CC_t4_550_o3,
    eStorgeParamIndex_Illumine_CC_t4_550_o4,
    eStorgeParamIndex_Illumine_CC_t4_550_o5,

    eStorgeParamIndex_Illumine_CC_t5_610_i0,
    eStorgeParamIndex_Illumine_CC_t5_610_i1,
    eStorgeParamIndex_Illumine_CC_t5_610_i2,
    eStorgeParamIndex_Illumine_CC_t5_610_i3,
    eStorgeParamIndex_Illumine_CC_t5_610_i4,
    eStorgeParamIndex_Illumine_CC_t5_610_i5,

    eStorgeParamIndex_Illumine_CC_t5_610_o0,
    eStorgeParamIndex_Illumine_CC_t5_610_o1,
    eStorgeParamIndex_Illumine_CC_t5_610_o2,
    eStorgeParamIndex_Illumine_CC_t5_610_o3,
    eStorgeParamIndex_Illumine_CC_t5_610_o4,
    eStorgeParamIndex_Illumine_CC_t5_610_o5,

    eStorgeParamIndex_Illumine_CC_t5_550_i0,
    eStorgeParamIndex_Illumine_CC_t5_550_i1,
    eStorgeParamIndex_Illumine_CC_t5_550_i2,
    eStorgeParamIndex_Illumine_CC_t5_550_i3,
    eStorgeParamIndex_Illumine_CC_t5_550_i4,
    eStorgeParamIndex_Illumine_CC_t5_550_i5,

    eStorgeParamIndex_Illumine_CC_t5_550_o0,
    eStorgeParamIndex_Illumine_CC_t5_550_o1,
    eStorgeParamIndex_Illumine_CC_t5_550_o2,
    eStorgeParamIndex_Illumine_CC_t5_550_o3,
    eStorgeParamIndex_Illumine_CC_t5_550_o4,
    eStorgeParamIndex_Illumine_CC_t5_550_o5,

    eStorgeParamIndex_Illumine_CC_t6_610_i0,
    eStorgeParamIndex_Illumine_CC_t6_610_i1,
    eStorgeParamIndex_Illumine_CC_t6_610_i2,
    eStorgeParamIndex_Illumine_CC_t6_610_i3,
    eStorgeParamIndex_Illumine_CC_t6_610_i4,
    eStorgeParamIndex_Illumine_CC_t6_610_i5,

    eStorgeParamIndex_Illumine_CC_t6_610_o0,
    eStorgeParamIndex_Illumine_CC_t6_610_o1,
    eStorgeParamIndex_Illumine_CC_t6_610_o2,
    eStorgeParamIndex_Illumine_CC_t6_610_o3,
    eStorgeParamIndex_Illumine_CC_t6_610_o4,
    eStorgeParamIndex_Illumine_CC_t6_610_o5,

    eStorgeParamIndex_Illumine_CC_t6_550_i0,
    eStorgeParamIndex_Illumine_CC_t6_550_i1,
    eStorgeParamIndex_Illumine_CC_t6_550_i2,
    eStorgeParamIndex_Illumine_CC_t6_550_i3,
    eStorgeParamIndex_Illumine_CC_t6_550_i4,
    eStorgeParamIndex_Illumine_CC_t6_550_i5,

    eStorgeParamIndex_Illumine_CC_t6_550_o0,
    eStorgeParamIndex_Illumine_CC_t6_550_o1,
    eStorgeParamIndex_Illumine_CC_t6_550_o2,
    eStorgeParamIndex_Illumine_CC_t6_550_o3,
    eStorgeParamIndex_Illumine_CC_t6_550_o4,
    eStorgeParamIndex_Illumine_CC_t6_550_o5,

    eStorgeParamIndex_Num,
} eStorgeParamIndex;

typedef struct {
    float temperature_cc_top_1; /* 000 */
    float temperature_cc_top_2; /* 004 */
    float temperature_cc_top_3; /* 008 */
    float temperature_cc_top_4; /* 012 */
    float temperature_cc_top_5; /* 016 */
    float temperature_cc_top_6; /* 020 */
    float temperature_cc_btm_1; /* 024 */
    float temperature_cc_btm_2; /* 028 */
    float temperature_cc_env;   /* 032 */

    uint32_t illumine_CC_t1_610_i0; /* 036 */
    uint32_t illumine_CC_t1_610_i1; /* 040 */
    uint32_t illumine_CC_t1_610_i2; /* 044 */
    uint32_t illumine_CC_t1_610_i3; /* 048 */
    uint32_t illumine_CC_t1_610_i4; /* 052 */
    uint32_t illumine_CC_t1_610_i5; /* 056 */

    uint32_t illumine_CC_t1_610_o0; /* 060 */
    uint32_t illumine_CC_t1_610_o1; /* 064 */
    uint32_t illumine_CC_t1_610_o2; /* 068 */
    uint32_t illumine_CC_t1_610_o3; /* 072 */
    uint32_t illumine_CC_t1_610_o4; /* 076 */
    uint32_t illumine_CC_t1_610_o5; /* 080 */

    uint32_t illumine_CC_t1_550_i0; /* 084 */
    uint32_t illumine_CC_t1_550_i1; /* 088 */
    uint32_t illumine_CC_t1_550_i2; /* 092 */
    uint32_t illumine_CC_t1_550_i3; /* 096 */
    uint32_t illumine_CC_t1_550_i4; /* 100 */
    uint32_t illumine_CC_t1_550_i5; /* 104 */

    uint32_t illumine_CC_t1_550_o0; /* 108 */
    uint32_t illumine_CC_t1_550_o1; /* 112 */
    uint32_t illumine_CC_t1_550_o2; /* 116 */
    uint32_t illumine_CC_t1_550_o3; /* 120 */
    uint32_t illumine_CC_t1_550_o4; /* 124 */
    uint32_t illumine_CC_t1_550_o5; /* 128 */

    uint32_t illumine_CC_t1_405_i0; /* 132 */
    uint32_t illumine_CC_t1_405_i1; /* 136 */
    uint32_t illumine_CC_t1_405_i2; /* 140 */
    uint32_t illumine_CC_t1_405_i3; /* 144 */
    uint32_t illumine_CC_t1_405_i4; /* 148 */
    uint32_t illumine_CC_t1_405_i5; /* 152 */

    uint32_t illumine_CC_t1_405_o0; /* 156 */
    uint32_t illumine_CC_t1_405_o1; /* 160 */
    uint32_t illumine_CC_t1_405_o2; /* 164 */
    uint32_t illumine_CC_t1_405_o3; /* 168 */
    uint32_t illumine_CC_t1_405_o4; /* 172 */
    uint32_t illumine_CC_t1_405_o5; /* 176 */

    uint32_t illumine_CC_t2_610_i0; /* 180 */
    uint32_t illumine_CC_t2_610_i1; /* 184 */
    uint32_t illumine_CC_t2_610_i2; /* 188 */
    uint32_t illumine_CC_t2_610_i3; /* 192 */
    uint32_t illumine_CC_t2_610_i4; /* 196 */
    uint32_t illumine_CC_t2_610_i5; /* 200 */

    uint32_t illumine_CC_t2_610_o0; /* 204 */
    uint32_t illumine_CC_t2_610_o1; /* 208 */
    uint32_t illumine_CC_t2_610_o2; /* 212 */
    uint32_t illumine_CC_t2_610_o3; /* 216 */
    uint32_t illumine_CC_t2_610_o4; /* 220 */
    uint32_t illumine_CC_t2_610_o5; /* 224 */

    uint32_t illumine_CC_t2_550_i0; /* 228 */
    uint32_t illumine_CC_t2_550_i1; /* 232 */
    uint32_t illumine_CC_t2_550_i2; /* 236 */
    uint32_t illumine_CC_t2_550_i3; /* 240 */
    uint32_t illumine_CC_t2_550_i4; /* 244 */
    uint32_t illumine_CC_t2_550_i5; /* 248 */

    uint32_t illumine_CC_t2_550_o0; /* 252 */
    uint32_t illumine_CC_t2_550_o1; /* 256 */
    uint32_t illumine_CC_t2_550_o2; /* 260 */
    uint32_t illumine_CC_t2_550_o3; /* 264 */
    uint32_t illumine_CC_t2_550_o4; /* 268 */
    uint32_t illumine_CC_t2_550_o5; /* 272 */

    uint32_t illumine_CC_t3_610_i0; /* 276 */
    uint32_t illumine_CC_t3_610_i1; /* 280 */
    uint32_t illumine_CC_t3_610_i2; /* 284 */
    uint32_t illumine_CC_t3_610_i3; /* 288 */
    uint32_t illumine_CC_t3_610_i4; /* 292 */
    uint32_t illumine_CC_t3_610_i5; /* 296 */

    uint32_t illumine_CC_t3_610_o0; /* 300 */
    uint32_t illumine_CC_t3_610_o1; /* 304 */
    uint32_t illumine_CC_t3_610_o2; /* 308 */
    uint32_t illumine_CC_t3_610_o3; /* 312 */
    uint32_t illumine_CC_t3_610_o4; /* 316 */
    uint32_t illumine_CC_t3_610_o5; /* 320 */

    uint32_t illumine_CC_t3_550_i0; /* 324 */
    uint32_t illumine_CC_t3_550_i1; /* 328 */
    uint32_t illumine_CC_t3_550_i2; /* 332 */
    uint32_t illumine_CC_t3_550_i3; /* 336 */
    uint32_t illumine_CC_t3_550_i4; /* 340 */
    uint32_t illumine_CC_t3_550_i5; /* 344 */

    uint32_t illumine_CC_t3_550_o0; /* 348 */
    uint32_t illumine_CC_t3_550_o1; /* 352 */
    uint32_t illumine_CC_t3_550_o2; /* 356 */
    uint32_t illumine_CC_t3_550_o3; /* 360 */
    uint32_t illumine_CC_t3_550_o4; /* 364 */
    uint32_t illumine_CC_t3_550_o5; /* 368 */

    uint32_t illumine_CC_t4_610_i0; /* 372 */
    uint32_t illumine_CC_t4_610_i1; /* 376 */
    uint32_t illumine_CC_t4_610_i2; /* 380 */
    uint32_t illumine_CC_t4_610_i3; /* 384 */
    uint32_t illumine_CC_t4_610_i4; /* 388 */
    uint32_t illumine_CC_t4_610_i5; /* 392 */

    uint32_t illumine_CC_t4_610_o0; /* 396 */
    uint32_t illumine_CC_t4_610_o1; /* 400 */
    uint32_t illumine_CC_t4_610_o2; /* 404 */
    uint32_t illumine_CC_t4_610_o3; /* 408 */
    uint32_t illumine_CC_t4_610_o4; /* 412 */
    uint32_t illumine_CC_t4_610_o5; /* 416 */

    uint32_t illumine_CC_t4_550_i0; /* 420 */
    uint32_t illumine_CC_t4_550_i1; /* 424 */
    uint32_t illumine_CC_t4_550_i2; /* 428 */
    uint32_t illumine_CC_t4_550_i3; /* 432 */
    uint32_t illumine_CC_t4_550_i4; /* 436 */
    uint32_t illumine_CC_t4_550_i5; /* 440 */

    uint32_t illumine_CC_t4_550_o0; /* 444 */
    uint32_t illumine_CC_t4_550_o1; /* 448 */
    uint32_t illumine_CC_t4_550_o2; /* 452 */
    uint32_t illumine_CC_t4_550_o3; /* 456 */
    uint32_t illumine_CC_t4_550_o4; /* 460 */
    uint32_t illumine_CC_t4_550_o5; /* 464 */

    uint32_t illumine_CC_t5_610_i0; /* 468 */
    uint32_t illumine_CC_t5_610_i1; /* 472 */
    uint32_t illumine_CC_t5_610_i2; /* 476 */
    uint32_t illumine_CC_t5_610_i3; /* 480 */
    uint32_t illumine_CC_t5_610_i4; /* 484 */
    uint32_t illumine_CC_t5_610_i5; /* 488 */

    uint32_t illumine_CC_t5_610_o0; /* 492 */
    uint32_t illumine_CC_t5_610_o1; /* 496 */
    uint32_t illumine_CC_t5_610_o2; /* 500 */
    uint32_t illumine_CC_t5_610_o3; /* 504 */
    uint32_t illumine_CC_t5_610_o4; /* 508 */
    uint32_t illumine_CC_t5_610_o5; /* 512 */

    uint32_t illumine_CC_t5_550_i0; /* 516 */
    uint32_t illumine_CC_t5_550_i1; /* 520 */
    uint32_t illumine_CC_t5_550_i2; /* 524 */
    uint32_t illumine_CC_t5_550_i3; /* 528 */
    uint32_t illumine_CC_t5_550_i4; /* 532 */
    uint32_t illumine_CC_t5_550_i5; /* 536 */

    uint32_t illumine_CC_t5_550_o0; /* 540 */
    uint32_t illumine_CC_t5_550_o1; /* 544 */
    uint32_t illumine_CC_t5_550_o2; /* 548 */
    uint32_t illumine_CC_t5_550_o3; /* 552 */
    uint32_t illumine_CC_t5_550_o4; /* 556 */
    uint32_t illumine_CC_t5_550_o5; /* 560 */

    uint32_t illumine_CC_t6_610_i0; /* 564 */
    uint32_t illumine_CC_t6_610_i1; /* 568 */
    uint32_t illumine_CC_t6_610_i2; /* 572 */
    uint32_t illumine_CC_t6_610_i3; /* 576 */
    uint32_t illumine_CC_t6_610_i4; /* 580 */
    uint32_t illumine_CC_t6_610_i5; /* 584 */

    uint32_t illumine_CC_t6_610_o0; /* 588 */
    uint32_t illumine_CC_t6_610_o1; /* 592 */
    uint32_t illumine_CC_t6_610_o2; /* 596 */
    uint32_t illumine_CC_t6_610_o3; /* 600 */
    uint32_t illumine_CC_t6_610_o4; /* 604 */
    uint32_t illumine_CC_t6_610_o5; /* 608 */

    uint32_t illumine_CC_t6_550_i0; /* 612 */
    uint32_t illumine_CC_t6_550_i1; /* 616 */
    uint32_t illumine_CC_t6_550_i2; /* 620 */
    uint32_t illumine_CC_t6_550_i3; /* 624 */
    uint32_t illumine_CC_t6_550_i4; /* 628 */
    uint32_t illumine_CC_t6_550_i5; /* 632 */

    uint32_t illumine_CC_t6_550_o0; /* 636 */
    uint32_t illumine_CC_t6_550_o1; /* 640 */
    uint32_t illumine_CC_t6_550_o2; /* 644 */
    uint32_t illumine_CC_t6_550_o3; /* 648 */
    uint32_t illumine_CC_t6_550_o4; /* 652 */
    uint32_t illumine_CC_t6_550_o5; /* 656 */

} sStorgeParamInfo;

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void storgeTaskInit(void);
uint8_t storgeReadConfInfo(uint32_t addr, uint32_t num, uint32_t timeout);
uint8_t storgeWriteConfInfo(uint32_t addr, uint8_t * pIn, uint32_t num, uint32_t timeout);
BaseType_t storgeTaskNotification(eStorgeNotifyConf type, eProtocol_COMM_Index index);

uint8_t storge_ParamSet(eStorgeParamIndex idx, uint8_t * pBuff, uint8_t length);
uint8_t storge_ParamGet(eStorgeParamIndex idx, uint8_t * pBuff);
/* Private defines -----------------------------------------------------------*/

#endif

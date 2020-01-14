/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STORGE_TASK_H
#define __STORGE_TASK_H
/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/
#include "protocol.h"

/* Exported types ------------------------------------------------------------*/
typedef enum {
    eStorgeNotifyConf_Read_Flash = 0x00000001,
    eStorgeNotifyConf_Write_Flash = 0x00000002,
    eStorgeNotifyConf_Read_Parmas = 0x00000004,
    eStorgeNotifyConf_Write_Parmas = 0x00000008,
    eStorgeNotifyConf_Load_Parmas = 0x00000010,
    eStorgeNotifyConf_Dump_Params = 0x00000020,
    eStorgeNotifyConf_Read_ID_Card = 0x00000040,
    eStorgeNotifyConf_Write_ID_Card = 0x00000080,
    eStorgeNotifyConf_Test_Flash = 0x00000100,
    eStorgeNotifyConf_Test_ID_Card = 0x00000200,
    eStorgeNotifyConf_Test_All = 0x00000400,
    eStorgeNotifyConf_COMM_Out = 0x10000000,
    eStorgeNotifyConf_COMM_Main = 0x20000000,
} eStorgeNotifyConf;

typedef union {
    float f32;
    uint32_t u32;
    uint16_t u16s[2];
    uint8_t u8s[4];
} uStorgeParamItem;

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

    eStorgeParamIndex_Heater_Offset_BTM,
    eStorgeParamIndex_Heater_Offset_TOP,

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

    float heater_offset_btm; /* 036 */
    float heater_offset_top; /* 040 */

    uint32_t illumine_CC_t1_610_i0; /* 044 */
    uint32_t illumine_CC_t1_610_i1; /* 048 */
    uint32_t illumine_CC_t1_610_i2; /* 052 */
    uint32_t illumine_CC_t1_610_i3; /* 056 */
    uint32_t illumine_CC_t1_610_i4; /* 060 */
    uint32_t illumine_CC_t1_610_i5; /* 064 */

    uint32_t illumine_CC_t1_610_o0; /* 068 */
    uint32_t illumine_CC_t1_610_o1; /* 072 */
    uint32_t illumine_CC_t1_610_o2; /* 076 */
    uint32_t illumine_CC_t1_610_o3; /* 080 */
    uint32_t illumine_CC_t1_610_o4; /* 084 */
    uint32_t illumine_CC_t1_610_o5; /* 088 */

    uint32_t illumine_CC_t1_550_i0; /* 092 */
    uint32_t illumine_CC_t1_550_i1; /* 096 */
    uint32_t illumine_CC_t1_550_i2; /* 100 */
    uint32_t illumine_CC_t1_550_i3; /* 104 */
    uint32_t illumine_CC_t1_550_i4; /* 108 */
    uint32_t illumine_CC_t1_550_i5; /* 112 */

    uint32_t illumine_CC_t1_550_o0; /* 116 */
    uint32_t illumine_CC_t1_550_o1; /* 120 */
    uint32_t illumine_CC_t1_550_o2; /* 124 */
    uint32_t illumine_CC_t1_550_o3; /* 128 */
    uint32_t illumine_CC_t1_550_o4; /* 132 */
    uint32_t illumine_CC_t1_550_o5; /* 136 */

    uint32_t illumine_CC_t1_405_i0; /* 140 */
    uint32_t illumine_CC_t1_405_i1; /* 144 */
    uint32_t illumine_CC_t1_405_i2; /* 148 */
    uint32_t illumine_CC_t1_405_i3; /* 152 */
    uint32_t illumine_CC_t1_405_i4; /* 156 */
    uint32_t illumine_CC_t1_405_i5; /* 160 */

    uint32_t illumine_CC_t1_405_o0; /* 164 */
    uint32_t illumine_CC_t1_405_o1; /* 168 */
    uint32_t illumine_CC_t1_405_o2; /* 172 */
    uint32_t illumine_CC_t1_405_o3; /* 176 */
    uint32_t illumine_CC_t1_405_o4; /* 180 */
    uint32_t illumine_CC_t1_405_o5; /* 184 */

    uint32_t illumine_CC_t2_610_i0; /* 188 */
    uint32_t illumine_CC_t2_610_i1; /* 192 */
    uint32_t illumine_CC_t2_610_i2; /* 196 */
    uint32_t illumine_CC_t2_610_i3; /* 200 */
    uint32_t illumine_CC_t2_610_i4; /* 204 */
    uint32_t illumine_CC_t2_610_i5; /* 208 */

    uint32_t illumine_CC_t2_610_o0; /* 212 */
    uint32_t illumine_CC_t2_610_o1; /* 216 */
    uint32_t illumine_CC_t2_610_o2; /* 220 */
    uint32_t illumine_CC_t2_610_o3; /* 224 */
    uint32_t illumine_CC_t2_610_o4; /* 228 */
    uint32_t illumine_CC_t2_610_o5; /* 232 */

    uint32_t illumine_CC_t2_550_i0; /* 236 */
    uint32_t illumine_CC_t2_550_i1; /* 240 */
    uint32_t illumine_CC_t2_550_i2; /* 244 */
    uint32_t illumine_CC_t2_550_i3; /* 248 */
    uint32_t illumine_CC_t2_550_i4; /* 252 */
    uint32_t illumine_CC_t2_550_i5; /* 256 */

    uint32_t illumine_CC_t2_550_o0; /* 260 */
    uint32_t illumine_CC_t2_550_o1; /* 264 */
    uint32_t illumine_CC_t2_550_o2; /* 268 */
    uint32_t illumine_CC_t2_550_o3; /* 272 */
    uint32_t illumine_CC_t2_550_o4; /* 276 */
    uint32_t illumine_CC_t2_550_o5; /* 280 */

    uint32_t illumine_CC_t3_610_i0; /* 284 */
    uint32_t illumine_CC_t3_610_i1; /* 288 */
    uint32_t illumine_CC_t3_610_i2; /* 292 */
    uint32_t illumine_CC_t3_610_i3; /* 296 */
    uint32_t illumine_CC_t3_610_i4; /* 300 */
    uint32_t illumine_CC_t3_610_i5; /* 304 */

    uint32_t illumine_CC_t3_610_o0; /* 308 */
    uint32_t illumine_CC_t3_610_o1; /* 312 */
    uint32_t illumine_CC_t3_610_o2; /* 316 */
    uint32_t illumine_CC_t3_610_o3; /* 320 */
    uint32_t illumine_CC_t3_610_o4; /* 324 */
    uint32_t illumine_CC_t3_610_o5; /* 328 */

    uint32_t illumine_CC_t3_550_i0; /* 332 */
    uint32_t illumine_CC_t3_550_i1; /* 336 */
    uint32_t illumine_CC_t3_550_i2; /* 340 */
    uint32_t illumine_CC_t3_550_i3; /* 344 */
    uint32_t illumine_CC_t3_550_i4; /* 348 */
    uint32_t illumine_CC_t3_550_i5; /* 352 */

    uint32_t illumine_CC_t3_550_o0; /* 356 */
    uint32_t illumine_CC_t3_550_o1; /* 360 */
    uint32_t illumine_CC_t3_550_o2; /* 364 */
    uint32_t illumine_CC_t3_550_o3; /* 368 */
    uint32_t illumine_CC_t3_550_o4; /* 372 */
    uint32_t illumine_CC_t3_550_o5; /* 376 */

    uint32_t illumine_CC_t4_610_i0; /* 380 */
    uint32_t illumine_CC_t4_610_i1; /* 384 */
    uint32_t illumine_CC_t4_610_i2; /* 388 */
    uint32_t illumine_CC_t4_610_i3; /* 392 */
    uint32_t illumine_CC_t4_610_i4; /* 396 */
    uint32_t illumine_CC_t4_610_i5; /* 400 */

    uint32_t illumine_CC_t4_610_o0; /* 404 */
    uint32_t illumine_CC_t4_610_o1; /* 408 */
    uint32_t illumine_CC_t4_610_o2; /* 412 */
    uint32_t illumine_CC_t4_610_o3; /* 416 */
    uint32_t illumine_CC_t4_610_o4; /* 420 */
    uint32_t illumine_CC_t4_610_o5; /* 424 */

    uint32_t illumine_CC_t4_550_i0; /* 428 */
    uint32_t illumine_CC_t4_550_i1; /* 432 */
    uint32_t illumine_CC_t4_550_i2; /* 436 */
    uint32_t illumine_CC_t4_550_i3; /* 440 */
    uint32_t illumine_CC_t4_550_i4; /* 444 */
    uint32_t illumine_CC_t4_550_i5; /* 448 */

    uint32_t illumine_CC_t4_550_o0; /* 452 */
    uint32_t illumine_CC_t4_550_o1; /* 456 */
    uint32_t illumine_CC_t4_550_o2; /* 460 */
    uint32_t illumine_CC_t4_550_o3; /* 464 */
    uint32_t illumine_CC_t4_550_o4; /* 468 */
    uint32_t illumine_CC_t4_550_o5; /* 472 */

    uint32_t illumine_CC_t5_610_i0; /* 476 */
    uint32_t illumine_CC_t5_610_i1; /* 480 */
    uint32_t illumine_CC_t5_610_i2; /* 484 */
    uint32_t illumine_CC_t5_610_i3; /* 488 */
    uint32_t illumine_CC_t5_610_i4; /* 492 */
    uint32_t illumine_CC_t5_610_i5; /* 496 */

    uint32_t illumine_CC_t5_610_o0; /* 500 */
    uint32_t illumine_CC_t5_610_o1; /* 504 */
    uint32_t illumine_CC_t5_610_o2; /* 508 */
    uint32_t illumine_CC_t5_610_o3; /* 512 */
    uint32_t illumine_CC_t5_610_o4; /* 516 */
    uint32_t illumine_CC_t5_610_o5; /* 520 */

    uint32_t illumine_CC_t5_550_i0; /* 524 */
    uint32_t illumine_CC_t5_550_i1; /* 528 */
    uint32_t illumine_CC_t5_550_i2; /* 532 */
    uint32_t illumine_CC_t5_550_i3; /* 536 */
    uint32_t illumine_CC_t5_550_i4; /* 540 */
    uint32_t illumine_CC_t5_550_i5; /* 544 */

    uint32_t illumine_CC_t5_550_o0; /* 548 */
    uint32_t illumine_CC_t5_550_o1; /* 552 */
    uint32_t illumine_CC_t5_550_o2; /* 556 */
    uint32_t illumine_CC_t5_550_o3; /* 560 */
    uint32_t illumine_CC_t5_550_o4; /* 564 */
    uint32_t illumine_CC_t5_550_o5; /* 568 */

    uint32_t illumine_CC_t6_610_i0; /* 572 */
    uint32_t illumine_CC_t6_610_i1; /* 576 */
    uint32_t illumine_CC_t6_610_i2; /* 580 */
    uint32_t illumine_CC_t6_610_i3; /* 584 */
    uint32_t illumine_CC_t6_610_i4; /* 588 */
    uint32_t illumine_CC_t6_610_i5; /* 592 */

    uint32_t illumine_CC_t6_610_o0; /* 596 */
    uint32_t illumine_CC_t6_610_o1; /* 600 */
    uint32_t illumine_CC_t6_610_o2; /* 604 */
    uint32_t illumine_CC_t6_610_o3; /* 608 */
    uint32_t illumine_CC_t6_610_o4; /* 612 */
    uint32_t illumine_CC_t6_610_o5; /* 616 */

    uint32_t illumine_CC_t6_550_i0; /* 620 */
    uint32_t illumine_CC_t6_550_i1; /* 624 */
    uint32_t illumine_CC_t6_550_i2; /* 628 */
    uint32_t illumine_CC_t6_550_i3; /* 632 */
    uint32_t illumine_CC_t6_550_i4; /* 636 */
    uint32_t illumine_CC_t6_550_i5; /* 640 */

    uint32_t illumine_CC_t6_550_o0; /* 644 */
    uint32_t illumine_CC_t6_550_o1; /* 648 */
    uint32_t illumine_CC_t6_550_o2; /* 652 */
    uint32_t illumine_CC_t6_550_o3; /* 656 */
    uint32_t illumine_CC_t6_550_o4; /* 660 */
    uint32_t illumine_CC_t6_550_o5; /* 664 */

} sStorgeParamInfo;

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
uint8_t gStorgeTaskInfoLockWait(uint32_t timeout);

void storgeTaskInit(void);
uint8_t storgeReadConfInfo(uint32_t addr, uint32_t num, uint32_t timeout);
uint8_t storgeReadConfInfo_FromISR(uint32_t addr, uint32_t num);
uint8_t storgeWriteConfInfo(uint32_t addr, uint8_t * pIn, uint32_t num, uint32_t timeout);
uint8_t storgeWriteConfInfo_FromISR(uint32_t addr, uint8_t * pIn, uint32_t num);
void storgeTaskNotification(eStorgeNotifyConf type, eProtocol_COMM_Index index);
void storgeTaskNotification_FromISR(eStorgeNotifyConf type, eProtocol_COMM_Index index);

uint8_t storge_ParamWriteSingle(eStorgeParamIndex idx, uint8_t * pBuff, uint8_t length);
uint8_t storge_ParamReadSingle(eStorgeParamIndex idx, uint8_t * pBuff);

uint16_t storge_ParamWrite(eStorgeParamIndex idx, uint16_t num, uint8_t * pBuff);
uint16_t storge_ParamRead(eStorgeParamIndex idx, uint16_t num, uint8_t * pBuff);

uint8_t stroge_Conf_CC_O_Data(uint8_t * pBuffer);
uint8_t stroge_Conf_CC_O_Data_From_B3(uint8_t * pBuffer);

void gStorgeIllumineCnt_Clr(void);
uint8_t gStorgeIllumineCnt_Check(uint8_t target);
/* Private defines -----------------------------------------------------------*/

#endif

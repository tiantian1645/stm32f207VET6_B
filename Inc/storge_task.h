/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STORGE_TASK_H
#define __STORGE_TASK_H
/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/
#include "protocol.h"
#include "comm_data.h"

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
    eStorgeParamIndex_Temp_CC_top,
    eStorgeParamIndex_Temp_CC_btm,
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
    float temperature_cc_top; /* 000 */
    float temperature_cc_btm; /* 004 */
    float temperature_cc_env; /* 008 */

    uint32_t illumine_CC_t1_610_i0; /* 012 */
    uint32_t illumine_CC_t1_610_i1; /* 016 */
    uint32_t illumine_CC_t1_610_i2; /* 020 */
    uint32_t illumine_CC_t1_610_i3; /* 024 */
    uint32_t illumine_CC_t1_610_i4; /* 028 */
    uint32_t illumine_CC_t1_610_i5; /* 032 */

    uint32_t illumine_CC_t1_610_o0; /* 036 */
    uint32_t illumine_CC_t1_610_o1; /* 040 */
    uint32_t illumine_CC_t1_610_o2; /* 044 */
    uint32_t illumine_CC_t1_610_o3; /* 048 */
    uint32_t illumine_CC_t1_610_o4; /* 052 */
    uint32_t illumine_CC_t1_610_o5; /* 056 */

    uint32_t illumine_CC_t1_550_i0; /* 060 */
    uint32_t illumine_CC_t1_550_i1; /* 064 */
    uint32_t illumine_CC_t1_550_i2; /* 068 */
    uint32_t illumine_CC_t1_550_i3; /* 072 */
    uint32_t illumine_CC_t1_550_i4; /* 076 */
    uint32_t illumine_CC_t1_550_i5; /* 080 */

    uint32_t illumine_CC_t1_550_o0; /* 084 */
    uint32_t illumine_CC_t1_550_o1; /* 088 */
    uint32_t illumine_CC_t1_550_o2; /* 092 */
    uint32_t illumine_CC_t1_550_o3; /* 096 */
    uint32_t illumine_CC_t1_550_o4; /* 100 */
    uint32_t illumine_CC_t1_550_o5; /* 104 */

    uint32_t illumine_CC_t1_405_i0; /* 108 */
    uint32_t illumine_CC_t1_405_i1; /* 112 */
    uint32_t illumine_CC_t1_405_i2; /* 116 */
    uint32_t illumine_CC_t1_405_i3; /* 120 */
    uint32_t illumine_CC_t1_405_i4; /* 124 */
    uint32_t illumine_CC_t1_405_i5; /* 128 */

    uint32_t illumine_CC_t1_405_o0; /* 132 */
    uint32_t illumine_CC_t1_405_o1; /* 136 */
    uint32_t illumine_CC_t1_405_o2; /* 140 */
    uint32_t illumine_CC_t1_405_o3; /* 144 */
    uint32_t illumine_CC_t1_405_o4; /* 148 */
    uint32_t illumine_CC_t1_405_o5; /* 152 */

    uint32_t illumine_CC_t2_610_i0; /* 156 */
    uint32_t illumine_CC_t2_610_i1; /* 160 */
    uint32_t illumine_CC_t2_610_i2; /* 164 */
    uint32_t illumine_CC_t2_610_i3; /* 168 */
    uint32_t illumine_CC_t2_610_i4; /* 172 */
    uint32_t illumine_CC_t2_610_i5; /* 176 */

    uint32_t illumine_CC_t2_610_o0; /* 180 */
    uint32_t illumine_CC_t2_610_o1; /* 184 */
    uint32_t illumine_CC_t2_610_o2; /* 188 */
    uint32_t illumine_CC_t2_610_o3; /* 192 */
    uint32_t illumine_CC_t2_610_o4; /* 196 */
    uint32_t illumine_CC_t2_610_o5; /* 200 */

    uint32_t illumine_CC_t2_550_i0; /* 204 */
    uint32_t illumine_CC_t2_550_i1; /* 208 */
    uint32_t illumine_CC_t2_550_i2; /* 212 */
    uint32_t illumine_CC_t2_550_i3; /* 216 */
    uint32_t illumine_CC_t2_550_i4; /* 220 */
    uint32_t illumine_CC_t2_550_i5; /* 224 */

    uint32_t illumine_CC_t2_550_o0; /* 228 */
    uint32_t illumine_CC_t2_550_o1; /* 232 */
    uint32_t illumine_CC_t2_550_o2; /* 236 */
    uint32_t illumine_CC_t2_550_o3; /* 240 */
    uint32_t illumine_CC_t2_550_o4; /* 244 */
    uint32_t illumine_CC_t2_550_o5; /* 248 */

    uint32_t illumine_CC_t3_610_i0; /* 252 */
    uint32_t illumine_CC_t3_610_i1; /* 256 */
    uint32_t illumine_CC_t3_610_i2; /* 260 */
    uint32_t illumine_CC_t3_610_i3; /* 264 */
    uint32_t illumine_CC_t3_610_i4; /* 268 */
    uint32_t illumine_CC_t3_610_i5; /* 272 */

    uint32_t illumine_CC_t3_610_o0; /* 276 */
    uint32_t illumine_CC_t3_610_o1; /* 280 */
    uint32_t illumine_CC_t3_610_o2; /* 284 */
    uint32_t illumine_CC_t3_610_o3; /* 288 */
    uint32_t illumine_CC_t3_610_o4; /* 292 */
    uint32_t illumine_CC_t3_610_o5; /* 296 */

    uint32_t illumine_CC_t3_550_i0; /* 300 */
    uint32_t illumine_CC_t3_550_i1; /* 304 */
    uint32_t illumine_CC_t3_550_i2; /* 308 */
    uint32_t illumine_CC_t3_550_i3; /* 312 */
    uint32_t illumine_CC_t3_550_i4; /* 316 */
    uint32_t illumine_CC_t3_550_i5; /* 320 */

    uint32_t illumine_CC_t3_550_o0; /* 324 */
    uint32_t illumine_CC_t3_550_o1; /* 328 */
    uint32_t illumine_CC_t3_550_o2; /* 332 */
    uint32_t illumine_CC_t3_550_o3; /* 336 */
    uint32_t illumine_CC_t3_550_o4; /* 340 */
    uint32_t illumine_CC_t3_550_o5; /* 344 */

    uint32_t illumine_CC_t4_610_i0; /* 348 */
    uint32_t illumine_CC_t4_610_i1; /* 352 */
    uint32_t illumine_CC_t4_610_i2; /* 356 */
    uint32_t illumine_CC_t4_610_i3; /* 360 */
    uint32_t illumine_CC_t4_610_i4; /* 364 */
    uint32_t illumine_CC_t4_610_i5; /* 368 */

    uint32_t illumine_CC_t4_610_o0; /* 372 */
    uint32_t illumine_CC_t4_610_o1; /* 376 */
    uint32_t illumine_CC_t4_610_o2; /* 380 */
    uint32_t illumine_CC_t4_610_o3; /* 384 */
    uint32_t illumine_CC_t4_610_o4; /* 388 */
    uint32_t illumine_CC_t4_610_o5; /* 392 */

    uint32_t illumine_CC_t4_550_i0; /* 396 */
    uint32_t illumine_CC_t4_550_i1; /* 400 */
    uint32_t illumine_CC_t4_550_i2; /* 404 */
    uint32_t illumine_CC_t4_550_i3; /* 408 */
    uint32_t illumine_CC_t4_550_i4; /* 412 */
    uint32_t illumine_CC_t4_550_i5; /* 416 */

    uint32_t illumine_CC_t4_550_o0; /* 420 */
    uint32_t illumine_CC_t4_550_o1; /* 424 */
    uint32_t illumine_CC_t4_550_o2; /* 428 */
    uint32_t illumine_CC_t4_550_o3; /* 432 */
    uint32_t illumine_CC_t4_550_o4; /* 436 */
    uint32_t illumine_CC_t4_550_o5; /* 440 */

    uint32_t illumine_CC_t5_610_i0; /* 444 */
    uint32_t illumine_CC_t5_610_i1; /* 448 */
    uint32_t illumine_CC_t5_610_i2; /* 452 */
    uint32_t illumine_CC_t5_610_i3; /* 456 */
    uint32_t illumine_CC_t5_610_i4; /* 460 */
    uint32_t illumine_CC_t5_610_i5; /* 464 */

    uint32_t illumine_CC_t5_610_o0; /* 468 */
    uint32_t illumine_CC_t5_610_o1; /* 472 */
    uint32_t illumine_CC_t5_610_o2; /* 476 */
    uint32_t illumine_CC_t5_610_o3; /* 480 */
    uint32_t illumine_CC_t5_610_o4; /* 484 */
    uint32_t illumine_CC_t5_610_o5; /* 488 */

    uint32_t illumine_CC_t5_550_i0; /* 492 */
    uint32_t illumine_CC_t5_550_i1; /* 496 */
    uint32_t illumine_CC_t5_550_i2; /* 500 */
    uint32_t illumine_CC_t5_550_i3; /* 504 */
    uint32_t illumine_CC_t5_550_i4; /* 508 */
    uint32_t illumine_CC_t5_550_i5; /* 512 */

    uint32_t illumine_CC_t5_550_o0; /* 516 */
    uint32_t illumine_CC_t5_550_o1; /* 520 */
    uint32_t illumine_CC_t5_550_o2; /* 524 */
    uint32_t illumine_CC_t5_550_o3; /* 528 */
    uint32_t illumine_CC_t5_550_o4; /* 532 */
    uint32_t illumine_CC_t5_550_o5; /* 536 */

    uint32_t illumine_CC_t6_610_i0; /* 540 */
    uint32_t illumine_CC_t6_610_i1; /* 544 */
    uint32_t illumine_CC_t6_610_i2; /* 548 */
    uint32_t illumine_CC_t6_610_i3; /* 552 */
    uint32_t illumine_CC_t6_610_i4; /* 556 */
    uint32_t illumine_CC_t6_610_i5; /* 560 */

    uint32_t illumine_CC_t6_610_o0; /* 564 */
    uint32_t illumine_CC_t6_610_o1; /* 568 */
    uint32_t illumine_CC_t6_610_o2; /* 572 */
    uint32_t illumine_CC_t6_610_o3; /* 576 */
    uint32_t illumine_CC_t6_610_o4; /* 580 */
    uint32_t illumine_CC_t6_610_o5; /* 584 */

    uint32_t illumine_CC_t6_550_i0; /* 588 */
    uint32_t illumine_CC_t6_550_i1; /* 592 */
    uint32_t illumine_CC_t6_550_i2; /* 596 */
    uint32_t illumine_CC_t6_550_i3; /* 600 */
    uint32_t illumine_CC_t6_550_i4; /* 604 */
    uint32_t illumine_CC_t6_550_i5; /* 608 */

    uint32_t illumine_CC_t6_550_o0; /* 612 */
    uint32_t illumine_CC_t6_550_o1; /* 616 */
    uint32_t illumine_CC_t6_550_o2; /* 620 */
    uint32_t illumine_CC_t6_550_o3; /* 624 */
    uint32_t illumine_CC_t6_550_o4; /* 628 */
    uint32_t illumine_CC_t6_550_o5; /* 632 */

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
eStorgeParamIndex storge_Param_Illumine_CC_Get_Index(uint8_t channel, eComm_Data_Sample_Radiant wave);
void storge_Param_Illumine_CC_Set_Single(eStorgeParamIndex idx, uint32_t data);

void gStorgeIllumineCnt_Clr(void);
uint8_t gStorgeIllumineCnt_Check(uint8_t target);
/* Private defines -----------------------------------------------------------*/

#endif

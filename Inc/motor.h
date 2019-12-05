/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MOTOR_H
#define __MOTOR_H

/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/
#define MOTOR_TASK_NOTIFY_SCAN (1 << 0)
#define MOTOR_TASK_NOTIFY_TRAY (1 << 1)
#define MOTOR_TASK_NOTIFY_WHITE (1 << 2)
#define MOTOR_TASK_NOTIFY_HEATER (1 << 3)

/* Exported types ------------------------------------------------------------*/

typedef enum {
    eMotorIndex_0,
    eMotorIndex_1,
    eMotorIndex_2,
    eMotorIndex_3,
} eMotorIndex;

/* 电机运动方向枚举 */
typedef enum {
    eMotorDir_FWD, /* 向前 */
    eMotorDir_REV, /* 倒车 */
} eMotorDir;

/* 电机运动状态结构 */
typedef struct {
    int32_t position;   /* 当前电机位置 */
    uint32_t tick_init; /* 电机开始运动时系统时钟脉搏数 运动超时判断 */
} sMotorRunStatus;

/* 电机运动命令信息 */
typedef struct {
    uint8_t (*pfEnter)(void);
    eMotorDir dir;
    uint32_t step;
    uint32_t timeout;
    uint8_t (*pfLeave)(void);
} sMoptorRunCmdInfo;

/* 电机任务队列效果枚举 */
typedef enum {
    eMotor_Fun_In,           /* 入仓 */
    eMotor_Fun_Out,          /* 出仓 */
    eMotor_Fun_Scan,         /* 扫码 */
    eMotor_Fun_PD,           /* PD值测试 */
    eMotor_Fun_WH,           /* 白底值测试 */
    eMotor_Fun_Sample_Start, /* 开始测试 */
    eMotor_Fun_SYK,          /* 交错 */
    eMotor_Fun_RLB,          /* 回滚 */
    eMotor_Fun_PRE_TRAY,     /* 压力测试 托盘 */
    eMotor_Fun_PRE_BARCODE,  /* 压力测试 扫码 */
    eMotor_Fun_PRE_HEATER,   /* 压力测试 上加热体 */
    eMotor_Fun_PRE_WHITE,    /* 压力测试 白板 */
    eMotor_Fun_PRE_ALL,      /* 压力测试 */
    eMotor_Fun_Self_Check,   /* 自检测试 */
} eMotor_Fun;

/* 电机任务队列效果结构 */
typedef struct {
    eMotor_Fun fun_type;
} sMotor_Fun;

typedef enum {
    eMotorNotifyValue_TG = (1 << 0),     /* 本次采样完成 */
    eMotorNotifyValue_BR = (1 << 1),     /* 终止采样 */
    eMotorNotifyValue_BR_ERR = (1 << 2), /* 终止采样 */
    eMotorNotifyValue_SP = (1 << 3),     /* 杂散光采样完成 */
    eMotorNotifyValue_SP_ERR = (1 << 4), /* 杂散光采样命令发送无回应 */
} eMotorNotifyValue;

typedef enum {
    eMotor_OPT_Status_ON = 0,
    eMotor_OPT_Status_OFF = 1,
    eMotor_OPT_Status_None = 0xFF,
} eMotor_OPT_Status;

typedef enum {
    eMotor_OPT_Index_Scan = 0,
    eMotor_OPT_Index_Tray = 1,
    eMotor_OPT_Index_Heater = 3,
    eMotor_OPT_Index_White = 4,
} eMotor_OPT_Index;

/* Exported define -----------------------------------------------------------*/
#define motor_Status_Set_Position(__RUNSTATUS__, __POSITION__) ((__RUNSTATUS__)->position = (__POSITION__))
#define motor_Status_Set_TickInit(__RUNSTATUS__, __TICKINIT__) ((__RUNSTATUS__)->tick_init = (__TICKINIT__))

#define motor_Status_Get_Position(__RUNSTATUS__) ((__RUNSTATUS__)->position)
#define motor_Status_Get_TickInit(__RUNSTATUS__) ((__RUNSTATUS__)->tick_init)

#define motor_CMD_Info_Set_PF_Enter(__COM_INFO__, __PF_ENTER__) ((__COM_INFO__)->pfEnter = (__PF_ENTER__))
#define motor_CMD_Info_Set_Dir(__COM_INFO__, __DIR__) ((__COM_INFO__)->dir = (__DIR__))
#define motor_CMD_Info_Set_Step(__COM_INFO__, __STEP__) ((__COM_INFO__)->step = (__STEP__))
#define motor_CMD_Info_Set_Tiemout(__COM_INFO__, __TIMEOUT__) ((__COM_INFO__)->timeout = (__TIMEOUT__))
#define motor_CMD_Info_Set_PF_Leave(__COM_INFO__, __PF_LEAVE__) ((__COM_INFO__)->pfLeave = (__PF_LEAVE__))

#define motor_CMD_Info_Get_PF_Enter(__COM_INFO__) ((__COM_INFO__)->pfEnter)
#define motor_CMD_Info_Get_Dir(__COM_INFO__) ((__COM_INFO__)->dir)
#define motor_CMD_Info_Get_Step(__COM_INFO__) ((__COM_INFO__)->step)
#define motor_CMD_Info_Get_Tiemout(__COM_INFO__) ((__COM_INFO__)->timeout)
#define motor_CMD_Info_Get_PF_Leave(__COM_INFO__) ((__COM_INFO__)->pfLeave)

void motor_Init(void);
uint8_t motor_Emit(sMotor_Fun * pFun_type, uint32_t timeout);

BaseType_t motor_Sample_Info_ISR(eMotorNotifyValue info);
BaseType_t motor_Sample_Info(eMotorNotifyValue info);

void motor_OPT_Status_Init(void);
uint8_t motor_OPT_Status_Init_Wait_Complete(void);
void motor_OPT_Status_Update(void);
eMotor_OPT_Status motor_OPT_Status_Get(eMotor_OPT_Index idx);

eMotor_OPT_Status motor_OPT_Status_Get_White(void);

void gMotorPRessureStopBits_Set(eMotor_Fun fun, uint8_t b);
void gMotorPRessureStopBits_Clear(void);
/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/

/* Private defines -----------------------------------------------------------*/

#endif

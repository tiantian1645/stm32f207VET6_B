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

#define MOTOR_CORRECT_POINT_NUM    12

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
    eMotor_Fun_In,                      /* 入仓 */
    eMotor_Fun_Out,                     /* 出仓 */
    eMotor_Fun_Sample_Start,            /* 开始测试 */
    eMotor_Fun_SYK,                     /* 交错 */
    eMotor_Fun_RLB,                     /* 回滚 */
    eMotor_Fun_PRE_TRAY,                /* 压力测试 托盘 */
    eMotor_Fun_PRE_BARCODE,             /* 压力测试 扫码 */
    eMotor_Fun_PRE_HEATER,              /* 压力测试 上加热体 */
    eMotor_Fun_PRE_WHITE,               /* 压力测试 白板 */
    eMotor_Fun_PRE_ALL,                 /* 压力测试 */
    eMotor_Fun_Self_Check,              /* 自检测试 */
    eMotor_Fun_Self_Check_FA,           /* 自检测试 生产板厂 */
    eMotor_Fun_Self_Check_Motor_White,  /* 自检测试 单项 白板电机 */
    eMotor_Fun_Self_Check_Motor_Heater, /* 自检测试 单项 上加热体电机 */
    eMotor_Fun_Self_Check_Motor_Tray,   /* 自检测试 单项 托盘电机 */
    eMotor_Fun_Self_Check_Motor_Scan,   /* 自检测试 单项 扫码电机*/
    eMotor_Fun_Self_Check_Scan,         /* 自检测试 单项 扫码头 */
    eMotor_Fun_Self_Check_PD,           /* 自检测试 单项 PD */
    eMotor_Fun_Stary_Test,              /* 杂散光测试 */
    eMotor_Fun_Correct,                 /* 定标 */
    eMotor_Fun_Debug_Tray_Scan,         /* 托盘电机扫码位置 */
    eMotor_Fun_Debug_Heater,            /* 上加热 */
    eMotor_Fun_Debug_White,             /* 白板电机 */
    eMotor_Fun_Debug_Scan,              /* 扫码电机 */
    eMotor_Fun_Lamp_BP,                 /* 灯光测试 */
    eMotor_Fun_SP_LED,                  /* 采样板LED校正 */
    eMotor_Fun_AgingLoop,               /* 老化测试 */
} eMotor_Fun;

/* 电机任务队列效果结构 */
typedef struct {
    eMotor_Fun fun_type;
    uint32_t fun_param_1;
} sMotor_Fun;

typedef enum {
    eMotorNotifyValue_TG = (1 << 0),      /* 本次采样完成 */
    eMotorNotifyValue_BR = (1 << 1),      /* 终止采样 */
    eMotorNotifyValue_SP = (1 << 2),      /* 杂散光采样完成 */
    eMotorNotifyValue_SP_ERR = (1 << 3),  /* 杂散光采样命令发送无回应 */
    eMotorNotifyValue_LAMP_BP = (1 << 4), /* 灯光测试 */
} eMotorNotifyValue;

typedef enum {
    eMotor_OPT_Status_ON = 0,      /* 光耦通路 */
    eMotor_OPT_Status_OFF = 1,     /* 光耦被遮挡 */
    eMotor_OPT_Status_None = 0xFF, /* 初始状态 */
} eMotor_OPT_Status;

typedef enum {
    eMotor_OPT_Index_Scan,
    eMotor_OPT_Index_Tray,
    eMotor_OPT_Index_Tray_Scan,
    eMotor_OPT_Index_Heater,
    eMotor_OPT_Index_White_In,
    eMotor_OPT_Index_White_Out,
    eMotor_OPT_Index_NUM,
} eMotor_OPT_Index;

typedef enum {
    eMotor_Sampl_Comm_None,
    eMotor_Sampl_Comm_Main,
    eMotor_Sampl_Comm_Out,
} eMotor_Sampl_Comm;

typedef enum {
    eMotor_Correct_Stary, /* 杂散光测试 */
    eMotor_Correct_610, /* 定标测试 610 */
    eMotor_Correct_550, /* 定标测试 550 */
    eMotor_Correct_405, /* 定标测试 405 */
}eMotor_Correct;

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
uint8_t motor_Emit_FromISR(sMotor_Fun * pFun_type);

BaseType_t motor_Sample_Info_ISR(eMotorNotifyValue info);
BaseType_t motor_Sample_Info(eMotorNotifyValue info);
BaseType_t motor_Sample_Info_From_ISR(eMotorNotifyValue info);

void motor_OPT_Status_Init(void);
uint8_t motor_OPT_Status_Init_Wait_Complete(void);
void motor_OPT_Status_Update(void);
eMotor_OPT_Status motor_OPT_Status_Get(eMotor_OPT_Index idx);

eMotor_OPT_Status motor_OPT_Status_Get_Tray(void);
eMotor_OPT_Status motor_OPT_Status_Get_White_In(void);
eMotor_OPT_Status motor_OPT_Status_Get_White_Out(void);

void gMotorPressureStopBits_Set(eMotor_Fun fun, uint8_t b);
void gMotorPressureStopBits_Clear(void);

eMotor_Sampl_Comm gMotor_Sampl_Comm_Get(void);
void gMotor_Sampl_Comm_Set(eMotor_Sampl_Comm b);
void gMotor_Sampl_Comm_Init(void);

void gMotor_Aging_Sleep_Set(uint8_t sleep);

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/

/* Private defines -----------------------------------------------------------*/

#endif

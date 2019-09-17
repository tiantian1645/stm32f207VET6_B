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

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/

/* Private defines -----------------------------------------------------------*/

#endif

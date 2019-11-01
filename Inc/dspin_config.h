/**
 ******************************************************************************
 * @file    dspin_config.h
 * @author  IPC Rennes
 * @version V2.0
 * @date    octobre 15, 2013
 * @brief   Header with configuration parameters for dspin.c module
 * @note    (C) COPYRIGHT 2013 STMicroelectronics
 ******************************************************************************
 * @copy
 *
 * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
 * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
 * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
 * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
 * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
 * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
 *
 * <h2><center>&copy; COPYRIGHT 2013 STMicroelectronics</center></h2>
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __DSPIN_CONFIG_H
#define __DSPIN_CONFIG_H

/* Includes ------------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported functions ------------------------------------------------------- */

/****************************************************************************/
/******************No Daisy Chain Mode **************************************/
/****************************************************************************/

/************************ Phase Current Control *****************************/

/* Register : ACC */
/* Acceleration rate in step/s2, range 14.55 to 59590 steps/s2 */
#define dSPIN_CONF_PARAM_ACC (2008.164)

/* Register : DEC */
/* Deceleration rate in step/s2, range 14.55 to 59590 steps/s2 */
#define dSPIN_CONF_PARAM_DEC (2008.164)

/* Register : MAX_SPEED */
/* Maximum speed in step/s, range 15.25 to 15610 steps/s */
#define dSPIN_CONF_PARAM_MAX_SPEED (793.457)

/* Register : MIN_SPEED */
/* Minimum speed in step/s, range 0 to 976.3 steps/s */
#define dSPIN_CONF_PARAM_MIN_SPEED (0)

/* Register : FS_SPD */
/* Full step speed in step/s, range 7.63 to 15625 steps/s */
#define dSPIN_CONF_PARAM_FS_SPD (595.093)

/************************ Phase Current Control *****************************/

/* Register : KVAL_HOLD */
/* Hold duty cycle (torque) in %, range 0 to 99.6% */
#define dSPIN_CONF_PARAM_KVAL_HOLD (25)

/* Register : KVAL_RUN */
/* Run duty cycle (torque) in %, range 0 to 99.6% */
#define dSPIN_CONF_PARAM_KVAL_RUN (1601.56)

/* Register : KVAL_ACC */
/* Acceleration duty cycle (torque) in %, range 0 to 99.6% */
#define dSPIN_CONF_PARAM_KVAL_ACC (1601.56)

/* Register : KVAL_DEC */
/* Deceleration duty cycle (torque) in %, range 0 to 99.6% */
#define dSPIN_CONF_PARAM_KVAL_DEC (1601.56)

/* Register : CONFIG - field : EN_VSCOMP */
/* Motor Supply Voltage Compensation enabling , enum dSPIN_CONFIG_EN_VSCOMP_TypeDef */
#define dSPIN_CONF_PARAM_VS_COMP (dSPIN_CONFIG_VS_COMP_DISABLE)

/* Register : MIN_SPEED - field : LSPD_OPT */
/* Low speed optimization bit, enum dSPIN_LSPD_OPT_TypeDef */
#define dSPIN_CONF_PARAM_LSPD_BIT (dSPIN_LSPD_OPT_OFF)

/* Register : K_THERM */
/* Thermal compensation param, range 1 to 1.46875 */
#define dSPIN_CONF_PARAM_K_THERM (1)

/* Register : INT_SPEED */
/* Intersect speed settings for BEMF compensation in steps/s, range 0 to 3906 steps/s */
#define dSPIN_CONF_PARAM_INT_SPD (61.512)

/* Register : ST_SLP */
/* BEMF start slope settings for BEMF compensation in % step/s, range 0 to 0.4% s/step */
#define dSPIN_CONF_PARAM_ST_SLP (0.03815)

/* Register : FN_SLP_ACC */
/* BEMF final dec slope settings for BEMF compensation in % step/s, range 0 to 0.4% s/step  */
#define dSPIN_CONF_PARAM_FN_SLP_ACC (0.06256)

/* Register : FN_SLP_DEC */
/* BEMF final dec slope settings for BEMF compensation in % step/s, range 0 to 0.4% s/step */
#define dSPIN_CONF_PARAM_FN_SLP_DEC (0.06256)

/* Register : CONFIG - field : F_PWM_INT */
/* PWM Frequency Integer division, enum dSPIN_CONFIG_F_PWM_INT_TypeDef */
#define dSPIN_CONF_PARAM_PWM_DIV (dSPIN_CONFIG_PWM_DIV_2)

/* Register : CONFIG - field : F_PWM_DEC */
/* PWM Frequency Integer Multiplier, enum dSPIN_CONFIG_F_PWM_INT_TypeDef */
#define dSPIN_CONF_PARAM_PWM_MUL (dSPIN_CONFIG_PWM_MUL_1)

/******************************* Others *************************************/
/* Register : OCD_TH */
/* Overcurrent threshold settings via enum dSPIN_OCD_TH_TypeDef */
#define dSPIN_CONF_PARAM_OCD_TH (dSPIN_OCD_TH_3375mA)

/* Register : STALL_TH */
/* Stall threshold settings in mA, range 31.25mA to 4000mA */
#define dSPIN_CONF_PARAM_STALL_TH (2031.25)

/* Register : ALARM_EN */
/* Alarm settings via bitmap enum dSPIN_ALARM_EN_TypeDef */
#define dSPIN_CONF_PARAM_ALARM_EN                                                                                                                              \
    (dSPIN_ALARM_EN_OVERCURRENT | dSPIN_ALARM_EN_THERMAL_SHUTDOWN | dSPIN_ALARM_EN_THERMAL_WARNING | dSPIN_ALARM_EN_UNDER_VOLTAGE |                            \
     dSPIN_ALARM_EN_STALL_DET_A | dSPIN_ALARM_EN_STALL_DET_B | dSPIN_ALARM_EN_WRONG_NPERF_CMD)

/* Register : STEP_MODE - field : STEP_MODE */
/* Step mode settings via enum dSPIN_STEP_SEL_TypeDef */
#define dSPIN_CONF_PARAM_STEP_MODE (dSPIN_STEP_SEL_1_8)

/* Register : STEP_MODE - Field : SYNC_MODE and SYNC_EN */
/* Synch. Mode settings via enum dSPIN_SYNC_SEL_TypeDef */
#define dSPIN_CONF_PARAM_SYNC_MODE (dSPIN_SYNC_SEL_DISABLED)

/* Register : CONFIG - field : POW_SR */
/* Slew rate, enum dSPIN_CONFIG_POW_SR_TypeDef */
#define dSPIN_CONF_PARAM_SR (dSPIN_CONFIG_SR_110V_us)

/* Register : CONFIG - field : OC_SD */
/* Over current shutwdown enabling, enum dSPIN_CONFIG_OC_SD_TypeDef */
#define dSPIN_CONF_PARAM_OC_SD (dSPIN_CONFIG_OC_SD_DISABLE)

/* Register : CONFIG - field : SW_MODE */
/* External switch hard stop interrupt mode, enum dSPIN_CONFIG_SW_MODE_TypeDef */
#define dSPIN_CONF_PARAM_SW_MODE (dSPIN_CONFIG_SW_HARD_STOP)

/* Register : CONFIG - field : OSC_CLK_SEL */
/* Clock setting , enum dSPIN_CONFIG_OSC_MGMT_TypeDef */
#define dSPIN_CONF_PARAM_CLOCK_SETTING (dSPIN_CONFIG_INT_16MHZ)

/* Exported types ------------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported functions ------------------------------------------------------- */
#endif

/******************* (C) COPYRIGHT 2013 STMicroelectronics *****END OF FILE****/

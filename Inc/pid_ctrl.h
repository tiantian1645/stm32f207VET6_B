/*	Floating point PID control loop for Microcontrollers
        Copyright (C) 2014 Jesus Ruben Santa Anna Zamudio.

        This program is free software: you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation, either version 3 of the License, or
        (at your option) any later version.

        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with this program.  If not, see <http://www.gnu.org/licenses/>.

        Author website: http://www.geekfactory.mx
        Author e-mail: ruben at geekfactory dot mx
 */
#ifndef __PID_CTL_H
#define __PID_CTL_H
/*-------------------------------------------------------------*/
/*		Includes and dependencies			*/
/*-------------------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>

/*-------------------------------------------------------------*/
/*		Macros and definitions				*/
/*-------------------------------------------------------------*/

/*-------------------------------------------------------------*/
/*		Typedefs enums & structs			*/
/*-------------------------------------------------------------*/

/**
 * Defines if the controler is direct or reverse
 */
typedef enum {
    E_PID_DIRECT,
    E_PID_REVERSE,
} ePID_Ctrl_Dir;

/**
 * Structure that holds PID all the PID controller data, multiple instances are
 * posible using different structures for each controller
 */
typedef struct {
    // Input, output and setpoint
    float * input;    //!< Current Process Value
    float * output;   //!< Corrective Output from PID Controller
    float * setpoint; //!< Controller Setpoint
    // Tuning parameters
    float Kp; //!< Stores the gain for the Proportional term
    float Ki; //!< Stores the gain for the Integral term
    float Kd; //!< Stores the gain for the Derivative term
    // Output minimum and maximum values
    float omin; //!< Maximum value allowed at the output
    float omax; //!< Minimum value allowed at the output
    // Variables for PID algorithm
    float iterm;  //!< Accumulator for integral term
    float lastin; //!< Last input value for differential term
    // Time related
    uint32_t lasttime;   //!< Stores the time when the control loop ran last time
    uint32_t sampletime; //!< Defines the PID sample time
    // Operation mode
    uint8_t automode; //!< Defines if the PID controller is enabled or disabled
    ePID_Ctrl_Dir direction;
    // Output
    float Op; //!< Proportional output
    float Oi; //!< Integral output
    float Od; //!< Derivative output
} sPID_Ctrl_Conf;

/*-------------------------------------------------------------*/
/*		Function prototypes				*/
/*-------------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief Creates a new PID controller
 *
 * Creates a new pid controller and initializes it�s input, output and internal
 * variables. Also we set the tuning parameters
 *
 * @param pid A pointer to a pid_controller structure
 * @param in Pointer to float value for the process input
 * @param out Poiter to put the controller output value
 * @param set Pointer float with the process setpoint value
 * @param kp Proportional gain
 * @param ki Integral gain
 * @param kd Diferential gain
 *
 * @return returns a spid_t controller handle
 */
void pid_ctrl_init(sPID_Ctrl_Conf * pPID_Info, uint32_t duration, float * in, float * out, float * set, float kp, float ki, float kd);

/**
 * @brief Check if PID loop needs to run
 *
 * Determines if the PID control algorithm should compute a new output value,
 * if this returs true, the user should read process feedback (sensors) and
 * place the reading in the input variable, then call the pid_ctrl_compute() function.
 *
 * @return return Return true if PID control algorithm is required to run
 */
bool pid_ctrl_need_compute(sPID_Ctrl_Conf * pPID_Info);

/**
 * @brief Computes the output of the PID control
 *
 * This function computes the PID output based on the parameters, setpoint and
 * current system input.
 *
 * @param pid The PID controller instance which will be used for computation
 */
void pid_ctrl_compute(sPID_Ctrl_Conf * pPID_Info);

/**
 * @brief Sets new PID tuning parameters
 *
 * Sets the gain for the Proportional (Kp), Integral (Ki) and Derivative (Kd)
 * terms.
 *
 * @param pid The PID controller instance to modify
 * @param kp Proportional gain
 * @param ki Integral gain
 * @param kd Derivative gain
 */
void pid_ctrl_tune(sPID_Ctrl_Conf * pPID_Info, float kp, float ki, float kd);

/**
 * @brief Sets the pid algorithm period
 *
 * Changes the between PID control loop computations.
 *
 * @param pid The PID controller instance to modify
 * @param time The time in milliseconds between computations
 */
void pid_ctrl_sample(sPID_Ctrl_Conf * pPID_Info, uint32_t time);

/**
 * @brief Sets the limits for the PID controller output
 *
 * @param pid The PID controller instance to modify
 * @param min The minimum output value for the PID controller
 * @param max The maximum output value for the PID controller
 */
void pid_ctrl_limits(sPID_Ctrl_Conf * pPID_Info, float min, float max);

/**
 * @brief Enables automatic control using PID
 *
 * Enables the PID control loop. If manual output adjustment is needed you can
 * disable the PID control loop using pid_ctrl_manual(). This function enables PID
 * automatic control at program start or after calling pid_ctrl_manual()
 *
 * @param pid The PID controller instance to enable
 */
void pid_ctrl_auto(sPID_Ctrl_Conf * pPID_Info);

/**
 * @brief Disables automatic process control
 *
 * Disables the PID control loop. User can modify the value of the output
 * variable and the controller will not overwrite it.
 *
 * @param pid The PID controller instance to disable
 */
void pid_ctrl_manual(sPID_Ctrl_Conf * pPID_Info);

/**
 * @brief Configures the PID controller direction
 *
 * Sets the direction of the PID controller. The direction is "DIRECT" when a
 * increase of the output will cause a increase on the measured value and
 * "REVERSE" when a increase on the controller output will cause a decrease on
 * the measured value.
 *
 * @param pid The PID controller instance to modify
 * @param direction The new direction of the PID controller
 */
void pid_ctrl_direction(sPID_Ctrl_Conf * pPID_Info, ePID_Ctrl_Dir dir);

void pid_ctrl_log_k(sPID_Ctrl_Conf * pPID_Info);
void pid_ctrl_log_d(const char * head, sPID_Ctrl_Conf * pPID_Info);

#ifdef __cplusplus
}
#endif

#endif
// End of Header file

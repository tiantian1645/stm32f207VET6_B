################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
S_SRCS += \
../Startup/startup_stm32f207vetx.s 

OBJS += \
./Startup/startup_stm32f207vetx.o 

S_DEPS += \
./Startup/startup_stm32f207vetx.d 


# Each subdirectory must supply rules for building sources it contributes
Startup/startup_stm32f207vetx.o: ../Startup/startup_stm32f207vetx.s
	arm-none-eabi-gcc -mcpu=cortex-m3 -c -I../ -x assembler-with-cpp -MMD -MP -MF"Startup/startup_stm32f207vetx.d" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@" "$<"


/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private variables ---------------------------------------------------------*/
extern I2C_HandleTypeDef hi2c1;

/* Private includes ----------------------------------------------------------*/
#include "i2c_eeprom.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
#define I2C_HW 1

#define AT24CXX_UNIT_LENGTH 16
#define AT24CXX_DEV_ADDR 0xA0
#define AT24CXX_MEM_ADDR_SIZE I2C_MEMADD_SIZE_8BIT
#define AT24CXX_I2C_HANDLE hi2c1

/* Private macro -------------------------------------------------------------*/
#define I2C_SDA_H HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET)
#define I2C_SDA_L HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET)
#define I2C_SCL_H HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET)
#define I2C_SCL_L HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET)

/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/* Private user code ---------------------------------------------------------*/

void delay_us(uint32_t timeout)
{
    uint32_t i;
    do {
        for (i = 0; i < 12; i++)
            ;
    } while (--timeout);
}

void I2C_SCL_OUT(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /*Configure GPIO pins : SCL_Pin SDA_Pin */
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void I2C_SDA_OUT(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /*Configure GPIO pins : SCL_Pin SDA_Pin */
    GPIO_InitStruct.Pin = GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void I2C_SDA_IN(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /*Configure GPIO pin : CARD_IN_Pin */
    GPIO_InitStruct.Pin = GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

//产生起始信号
void I2C_Start(void)
{
    I2C_SDA_OUT();
    I2C_SDA_H;
    I2C_SCL_H;
    delay_us(5);
    I2C_SDA_L;
    delay_us(6);
    I2C_SCL_L;
}

//产生停止信号
void I2C_Stop(void)
{
    I2C_SDA_OUT();
    I2C_SCL_L;
    I2C_SDA_L;
    I2C_SCL_H;
    delay_us(6);
    I2C_SDA_H;
    delay_us(6);
}

//主机产生应答信号ACK
void I2C_Ack(void)
{
    I2C_SCL_L;
    I2C_SDA_OUT();
    I2C_SDA_L;
    delay_us(2);
    I2C_SCL_H;
    delay_us(5);
    I2C_SCL_L;
}

//主机不产生应答信号NACK
void I2C_NAck(void)
{
    I2C_SCL_L;
    I2C_SDA_OUT();
    I2C_SDA_H;
    delay_us(2);
    I2C_SCL_H;
    delay_us(5);
    I2C_SCL_L;
}

//等待从机应答信号
//返回值：1 接收应答失败
//        0 接收应答成功
uint8_t I2C_Wait_Ack(void)
{
    uint8_t tempTime = 0;

    I2C_SDA_IN();
    I2C_SDA_H;
    delay_us(1);
    I2C_SCL_H;
    delay_us(1);

    while (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7)) {
        tempTime++;
        if (tempTime > 250) {
            I2C_Stop();
            return 1;
        }
    }
    I2C_SCL_L;
    return 0;
}

// I2C 发送一个字节
void I2C_Send_Byte(uint8_t txd)
{
    uint8_t i = 0;

    I2C_SDA_OUT();
    I2C_SCL_L; //拉低时钟开始数据传输
    for (i = 0; i < 8; i++) {
        if ((txd & 0x80) > 0) // 0x80  1000 0000
            I2C_SDA_H;
        else
            I2C_SDA_L;
        txd <<= 1;
        I2C_SCL_H;
        delay_us(2); //发送数据
        I2C_SCL_L;
        delay_us(2);
    }
}

// I2C 读取一个字节
uint8_t I2C_Read_Byte(uint8_t ack)
{
    uint8_t i = 0, receive = 0;

    I2C_SDA_IN();

    for (i = 0; i < 8; i++) {
        I2C_SCL_L;
        delay_us(2);
        I2C_SCL_H;
        receive <<= 1;
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7))
            receive++;
        delay_us(1);
    }
    if (ack == 0)
        I2C_NAck();
    else

        I2C_Ack();
    return receive;
}

uint8_t AT24Cxx_ReadOneByte(uint16_t addr)
{
    uint8_t temp = 0;

    I2C_Start();
    I2C_Send_Byte(0xA0);
    I2C_Wait_Ack();
    I2C_Send_Byte(addr >> 8); //发送数据地址高位
    I2C_Wait_Ack();
    I2C_Send_Byte(addr % 256); //双字节是数据地址低位

    //单字节是数据地址低位
    I2C_Wait_Ack();
    I2C_Start();
    I2C_Send_Byte(0xA1);
    I2C_Wait_Ack();
    temp = I2C_Read_Byte(0); //  0   代表 NACK
    I2C_Stop();
    return temp;
}

uint16_t AT24Cxx_ReadTwoByte(uint16_t addr)
{

    uint16_t temp = 0;

    I2C_Start();
    I2C_Send_Byte(0xA0);
    I2C_Wait_Ack();
    I2C_Send_Byte(addr >> 8); //发送数据地址高位
    I2C_Wait_Ack();
    I2C_Send_Byte(addr % 256); //双字节是数据地址低位

    //单字节是数据地址低位
    I2C_Wait_Ack();
    I2C_Start();
    I2C_Send_Byte(0xA1);
    I2C_Wait_Ack();
    temp = I2C_Read_Byte(1); //  1   代表 ACK
    temp <<= 8;
    temp |= I2C_Read_Byte(0); //  0  代表 NACK
    I2C_Stop();
    return temp;
}

void AT24Cxx_WriteOneByte(uint16_t addr, uint8_t dt)
{
    I2C_Start();
    I2C_Send_Byte(0xA0);
    I2C_Wait_Ack();
    I2C_Send_Byte(addr >> 8);                  //发送数据地址高位
    I2C_Send_Byte(0xA0 + ((addr / 256) << 1)); //器件地址+数据地址
    I2C_Wait_Ack();
    I2C_Send_Byte(addr % 256); //双字节是数据地址低位
    //单字节是数据地址低位
    I2C_Wait_Ack();
    I2C_Send_Byte(dt);
    I2C_Wait_Ack();
    I2C_Stop();
    HAL_Delay(10);
}

void AT24Cxx_WriteTwoByte(uint16_t addr, uint16_t dt)
{
    I2C_Start();
    I2C_Send_Byte(0xA0);
    I2C_Wait_Ack();
    I2C_Send_Byte(addr >> 8); //发送数据地址高位

    I2C_Wait_Ack();
    I2C_Send_Byte(addr % 256); //双字节是数据地址低位
                               //单字节是数据地址低位
    I2C_Wait_Ack();
    I2C_Send_Byte(dt >> 8);
    I2C_Wait_Ack();
    I2C_Send_Byte(dt & 0xFF);
    I2C_Wait_Ack();
    I2C_Stop();
    HAL_Delay(10);
}

/**
 * @brief  I2C EEPROM 读取数据
 * @param  memAddr     操作地址
 * @param  pOutBuff    输出指针
 * @param  length      读取长度
 * @param  timeout     超时时间
 * @retval 读取数量
 */
uint16_t I2C_EEPROM_Read(uint16_t memAddr, uint8_t * pOutBuff, uint16_t length, uint32_t timeout)
{
#if I2C_HW
    uint8_t dealNum;
    uint16_t readCnt = 0;

    if (length == 0) { /* 长度数据无效 */
        return readCnt;
    }

    dealNum = AT24CXX_UNIT_LENGTH - memAddr % AT24CXX_UNIT_LENGTH; /* 首次操作长度 */
    if (dealNum > length) {
        dealNum = length;
    }

    do {
        if (HAL_I2C_Mem_Read(&AT24CXX_I2C_HANDLE, (uint16_t)(AT24CXX_DEV_ADDR), memAddr, AT24CXX_MEM_ADDR_SIZE, pOutBuff, dealNum, timeout) != HAL_OK) {
            return readCnt;
        }
        readCnt += dealNum; /* 已读取数量 */
        length -= dealNum;  /* 待处理长度缩减 */
        if (length == 0) {  /* 操作完成 提前返回 */
            return readCnt;
        }
        vTaskDelay(2);                                                                /* 操作间隔延时 */
        pOutBuff += dealNum;                                                          /* 数据指针位移 */
        memAddr += dealNum;                                                           /* 操作地址位移 */
        dealNum = (length >= AT24CXX_UNIT_LENGTH) ? (AT24CXX_UNIT_LENGTH) : (length); /* 下次处理长度 */
    } while (length);
    return readCnt;
#else
    uint8_t flag = 0; /* 读取内容全0xFF标识 未插卡情况下读取全为0xFF */
    uint16_t i;

    I2C_SCL_OUT();
    I2C_SDA_OUT();
    for (i = 0; i < length; ++i) {
        pOutBuff[i] = AT24Cxx_ReadOneByte(memAddr + i); /* 软件I2C读取单字节 */
        if (pOutBuff[i] != 0xFF) {                      /* 存在非0xFF数据 */
            flag = 1;
        }
    }
    if (flag == 0) {                                                            /* 全为0xFF */
        if (HAL_GPIO_ReadPin(CARD_IN_GPIO_Port, CARD_IN_Pin) == GPIO_PIN_SET) { /* 检查ID卡是否插入 */
            return 0;                                                           /* 未插入直接返回0 */
        }
    }
    return length;
#endif
}

/**
 * @brief  I2C EEPROM 写入数据
 * @param  memAddr     操作地址
 * @param  pOutBuff    输入指针
 * @param  length      写入长度
 * @param  timeout     超时时间
 * @retval 写入数量
 */
uint16_t I2C_EEPROM_Write(uint16_t memAddr, uint8_t * pOutBuff, uint16_t length, uint32_t timeout)
{
    uint8_t dealNum;
    uint16_t wroteCnt = 0;

    if (length == 0) { /* 长度数据无效 */
        return wroteCnt;
    }

    dealNum = AT24CXX_UNIT_LENGTH - memAddr % AT24CXX_UNIT_LENGTH; /* 首次操作长度 */
    if (dealNum > length) {
        dealNum = length;
    }

    do {
        if (HAL_I2C_Mem_Write(&AT24CXX_I2C_HANDLE, (uint16_t)AT24CXX_DEV_ADDR, memAddr, AT24CXX_MEM_ADDR_SIZE, pOutBuff, dealNum, timeout) != HAL_OK) {
            return wroteCnt;
        }
        wroteCnt += dealNum; /* 已写入数量 */
        length -= dealNum;   /* 待处理长度缩减 */
        if (length == 0) {   /* 操作完成 提前返回 */
            return wroteCnt;
        }
        vTaskDelay(4);                                                                /* 操作间隔延时 */
        pOutBuff += dealNum;                                                          /* 数据指针位移 */
        memAddr += dealNum;                                                           /* 操作地址位移 */
        dealNum = (length >= AT24CXX_UNIT_LENGTH) ? (AT24CXX_UNIT_LENGTH) : (length); /* 下次处理长度 */
    } while (length);
    return wroteCnt;
}

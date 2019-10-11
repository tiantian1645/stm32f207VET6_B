/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "spi_flash.h"

/* Extern variables ----------------------------------------------------------*/
extern SPI_HandleTypeDef hspi1;

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
/* 串行Flsh的片选GPIO端口  */
#define spi_FlashPORT_CS SPI1_NSS_GPIO_Port
#define spi_FlashPIN_CS SPI1_NSS_Pin
#define spi_FlashCS_ENABLE (HAL_GPIO_WritePin(spi_FlashPORT_CS, spi_FlashPIN_CS, GPIO_PIN_RESET)) /* 使能片选 */
#define spi_FlashCS_DISABLE (HAL_GPIO_WritePin(spi_FlashPORT_CS, spi_FlashPIN_CS, GPIO_PIN_SET))  /* 禁能片选 */
#define spi_FlashCS_0() spi_FlashCS_ENABLE
#define spi_FlashCS_1() spi_FlashCS_DISABLE

/* Private macro -------------------------------------------------------------*/
#define CMD_AAI 0x02    /* AAI 连续编程指令(FOR SST25VF016B) */
#define CMD_DISWR 0x04  /* 禁止写, 退出AAI状态 */
#define CMD_EWRSR 0x50  /* 允许写状态寄存器的命令 */
#define CMD_WRSR 0x01   /* 写状态寄存器命令 */
#define CMD_WREN 0x06   /* 写使能命令 */
#define CMD_READ 0x03   /* 读数据区命令 */
#define CMD_RDSR 0x05   /* 读状态寄存器命令 */
#define CMD_RDID 0x9F   /* 读器件ID命令 */
#define CMD_SE 0x20     /* 擦除扇区命令 */
#define CMD_BE 0xC7     /* 批量擦除命令 */
#define DUMMY_BYTE 0xA5 /* 哑命令，可以为任意值，用于读操作 */

#define WIP_FLAG 0x01 /* 状态寄存器中的正在编程标志（WIP) */

/* W25Q64BV 参数*/
#define SPI_FLASH_MAX_ADDR (8 * 1024 * 1024) /* 总容量大小 8M */
#define SPI_FLASH_SECTOR_SIZE (4 * 1024)     /* 单扇区大小 4K */
#define SPI_FLASH_PAGE_SIZE (256)            /* 单页面大小 256 Bytes */

/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/* Private user code ---------------------------------------------------------*/

SFLASH_T g_tSF;

static void spi_FlashWriteEnable(void);
static void spi_FlashWriteStatus(uint8_t _ucValue);
static void spi_FlashWaitForWriteEnd(void);
static uint8_t spi_FlashNeedErase(uint8_t * _ucpOldBuf, uint8_t * _ucpNewBuf, uint16_t _uiLen);
static uint8_t spi_FlashCmpData(uint32_t _uiSrcAddr, uint8_t * _ucpTar, uint32_t _uiSize);
static uint8_t spi_FlashAutoWritePage(uint32_t _uiWrAddr, uint8_t * _ucpSrc, uint16_t _usWrLen);
static uint8_t s_spiBuf[4 * 1024]; /* 用于写函数，先读出整个page，修改缓冲区后，再整个page回写 */

static void spi_FlashSetCS(uint8_t _level);

uint8_t g_spi_busy = 0; /* SPI 总线共享标志 */

/*
*********************************************************************************************************
*	函 数 名: bsp_SpiBusEnter
*	功能说明: 占用SPI总线
*	形    参: 无
*	返 回 值: 0 表示不忙  1表示忙
*********************************************************************************************************
*/
void bsp_SpiBusEnter(void)
{
    g_spi_busy = 1;
}

/*
*********************************************************************************************************
*	函 数 名: bsp_SpiBusExit
*	功能说明: 释放占用的SPI总线
*	形    参: 无
*	返 回 值: 0 表示不忙  1表示忙
*********************************************************************************************************
*/
void bsp_SpiBusExit(void)
{
    g_spi_busy = 0;
}

/*
*********************************************************************************************************
*	函 数 名: bsp_spi_swap
*	功能说明: 向SPI总线发送一个字节。  SCK上升沿采集数据, SCK空闲时为高电平
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
uint8_t bsp_spi_swap(uint8_t _ucByte)
{
    uint8_t read = 0xA5;

    HAL_SPI_TransmitReceive(&hspi1, &_ucByte, &read, 1, 10);
    return read;
}

#define bsp_spiRead1() bsp_spi_swap(DUMMY_BYTE)

/*
*********************************************************************************************************
*	函 数 名: spi_FlashSetCS(0)
*	功能说明: 设置CS。 用于运行中SPI共享。
*	形    参: 无
        返 回 值: 无
*********************************************************************************************************
*/
static void spi_FlashSetCS(uint8_t _level)
{
    if (_level == 0) {
        bsp_SpiBusEnter(); /* 占用SPI总线， 用于总线共享 */
        spi_FlashCS_0();
    } else {
        spi_FlashCS_1();
        bsp_SpiBusExit(); /* 释放SPI总线， 用于总线共享 */
    }
}

/*
*********************************************************************************************************
*	函 数 名: bsp_Initspi_Flash
*	功能说明: 初始化串行Flash硬件接口（配置STM32的SPI时钟、GPIO)
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_spi_FlashInit(void)
{
    spi_FlashReadInfo(); /* 自动识别芯片型号 */

    spi_FlashSetCS(0);       /* 软件方式，使能串行Flash片选 */
    bsp_spi_swap(CMD_DISWR); /* 发送禁止写入的命令,即使能软件写保护 */
    spi_FlashSetCS(1);       /* 软件方式，禁能串行Flash片选 */

    spi_FlashWaitForWriteEnd(); /* 等待串行Flash内部操作完成 */

    spi_FlashWriteStatus(0); /* 解除所有BLOCK的写保护 */
}

/*
*********************************************************************************************************
*	函 数 名: spi_FlashEraseSector
*	功能说明: 擦除指定的扇区
*	形    参:  _uiSectorAddr : 扇区地址
*	返 回 值: 无
*********************************************************************************************************
*/
void spi_FlashEraseSector(uint32_t _uiSectorAddr)
{
    spi_FlashWriteEnable(); /* 发送写使能命令 */

    /* 擦除扇区操作 */
    spi_FlashSetCS(0);                              /* 使能片选 */
    bsp_spi_swap(CMD_SE);                           /* 发送擦除命令 */
    bsp_spi_swap((_uiSectorAddr & 0xFF0000) >> 16); /* 发送扇区地址的高8bit */
    bsp_spi_swap((_uiSectorAddr & 0xFF00) >> 8);    /* 发送扇区地址中间8bit */
    bsp_spi_swap(_uiSectorAddr & 0xFF);             /* 发送扇区地址低8bit */
    spi_FlashSetCS(1);                              /* 禁能片选 */

    spi_FlashWaitForWriteEnd(); /* 等待串行Flash内部写操作完成 */
}

/*
*********************************************************************************************************
*	函 数 名: spi_FlashEraseChip
*	功能说明: 擦除整个芯片
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void spi_FlashEraseChip(void)
{
    spi_FlashWriteEnable(); /* 发送写使能命令 */

    /* 擦除扇区操作 */
    spi_FlashSetCS(0);    /* 使能片选 */
    bsp_spi_swap(CMD_BE); /* 发送整片擦除命令 */
    spi_FlashSetCS(1);    /* 禁能片选 */

    spi_FlashWaitForWriteEnd(); /* 等待串行Flash内部写操作完成 */
}

/*
*********************************************************************************************************
*	函 数 名: spi_FlashPageWrite
*	功能说明: 向一个page内写入若干字节。字节个数不能超出页面大小（4K)
*	形    参:  	_pBuf : 数据源缓冲区；
*				_uiWriteAddr ：目标区域首地址
*				_usSize ：数据个数，不能超过页面大小
*	返 回 值: 无
*********************************************************************************************************
*/
void spi_FlashPageWrite(uint8_t * _pBuf, uint32_t _uiWriteAddr, uint16_t _usSize)
{
    uint32_t i, j;

    if (g_tSF.ChipID == SST25VF016B_ID) {
        /* AAI指令要求传入的数据个数是偶数 */
        if ((_usSize < 2) && (_usSize % 2)) {
            return;
        }

        spi_FlashWriteEnable(); /* 发送写使能命令 */

        spi_FlashSetCS(0);                             /* 使能片选 */
        bsp_spi_swap(CMD_AAI);                         /* 发送AAI命令(地址自动增加编程) */
        bsp_spi_swap((_uiWriteAddr & 0xFF0000) >> 16); /* 发送扇区地址的高8bit */
        bsp_spi_swap((_uiWriteAddr & 0xFF00) >> 8);    /* 发送扇区地址中间8bit */
        bsp_spi_swap(_uiWriteAddr & 0xFF);             /* 发送扇区地址低8bit */
        bsp_spi_swap(*_pBuf++);                        /* 发送第1个数据 */
        bsp_spi_swap(*_pBuf++);                        /* 发送第2个数据 */
        spi_FlashSetCS(1);                             /* 禁能片选 */

        spi_FlashWaitForWriteEnd(); /* 等待串行Flash内部写操作完成 */

        _usSize -= 2; /* 计算剩余字节数 */

        for (i = 0; i < _usSize / 2; i++) {
            spi_FlashSetCS(0);          /* 使能片选 */
            bsp_spi_swap(CMD_AAI);      /* 发送AAI命令(地址自动增加编程) */
            bsp_spi_swap(*_pBuf++);     /* 发送数据 */
            bsp_spi_swap(*_pBuf++);     /* 发送数据 */
            spi_FlashSetCS(1);          /* 禁能片选 */
            spi_FlashWaitForWriteEnd(); /* 等待串行Flash内部写操作完成 */
        }

        /* 进入写保护状态 */
        spi_FlashSetCS(0);
        bsp_spi_swap(CMD_DISWR);
        spi_FlashSetCS(1);

        spi_FlashWaitForWriteEnd(); /* 等待串行Flash内部写操作完成 */
    } else                          /* for MX25L1606E 、 W25Q64BV */
    {
        for (j = 0; j < _usSize / 256; j++) {
            spi_FlashWriteEnable(); /* 发送写使能命令 */

            spi_FlashSetCS(0);                             /* 使能片选 */
            bsp_spi_swap(0x02);                            /* 发送AAI命令(地址自动增加编程) */
            bsp_spi_swap((_uiWriteAddr & 0xFF0000) >> 16); /* 发送扇区地址的高8bit */
            bsp_spi_swap((_uiWriteAddr & 0xFF00) >> 8);    /* 发送扇区地址中间8bit */
            bsp_spi_swap(_uiWriteAddr & 0xFF);             /* 发送扇区地址低8bit */

            for (i = 0; i < 256; i++) {
                bsp_spi_swap(*_pBuf++); /* 发送数据 */
            }

            spi_FlashSetCS(1); /* 禁止片选 */

            spi_FlashWaitForWriteEnd(); /* 等待串行Flash内部写操作完成 */

            _uiWriteAddr += 256;
        }

        /* 进入写保护状态 */
        spi_FlashSetCS(0);
        bsp_spi_swap(CMD_DISWR);
        spi_FlashSetCS(1);

        spi_FlashWaitForWriteEnd(); /* 等待串行Flash内部写操作完成 */
    }
}

/*
*********************************************************************************************************
*	函 数 名: spi_FlashReadBuffer
*	功能说明: 连续读取若干字节。字节个数不能超出芯片容量。
*	形    参:  	_pBuf : 数据源缓冲区；
*				_uiReadAddr ：首地址
*				_usSize ：数据个数, 可以大于PAGE_SIZE,但是不能超出芯片总容量
*	返 回 值: 读取长度
*********************************************************************************************************
*/
uint32_t spi_FlashReadBuffer(uint32_t _uiReadAddr, uint8_t * _pBuf, uint32_t _uiSize)
{
    uint32_t readCnt = 0;
    /* 如果读取的数据长度为0或者超出串行Flash地址空间，则直接返回 */
    if ((_uiSize == 0) || (_uiReadAddr + _uiSize) > g_tSF.TotalSize) {
        return 0;
    }

    /* 擦除扇区操作 */
    spi_FlashSetCS(0);                            /* 使能片选 */
    bsp_spi_swap(CMD_READ);                       /* 发送读命令 */
    bsp_spi_swap((_uiReadAddr & 0xFF0000) >> 16); /* 发送扇区地址的高8bit */
    bsp_spi_swap((_uiReadAddr & 0xFF00) >> 8);    /* 发送扇区地址中间8bit */
    bsp_spi_swap(_uiReadAddr & 0xFF);             /* 发送扇区地址低8bit */
    while (_uiSize--) {
        *_pBuf++ = bsp_spiRead1(); /* 读一个字节并存储到pBuf，读完后指针自加1 */
        ++readCnt;
    }
    spi_FlashSetCS(1); /* 禁能片选 */
    return readCnt;
}

/*
*********************************************************************************************************
*	函 数 名: spi_FlashCmpData
*	功能说明: 比较Flash的数据.
*	形    参:  	_ucpTar : 数据缓冲区
*				_uiSrcAddr ：Flash地址
*				_uiSize ：数据个数, 可以大于PAGE_SIZE,但是不能超出芯片总容量
*	返 回 值: 0 = 相等, 1 = 不等
*********************************************************************************************************
*/
static uint8_t spi_FlashCmpData(uint32_t _uiSrcAddr, uint8_t * _ucpTar, uint32_t _uiSize)
{
    uint8_t ucValue;

    /* 如果读取的数据长度为0或者超出串行Flash地址空间，则直接返回 */
    if ((_uiSrcAddr + _uiSize) > g_tSF.TotalSize) {
        return 1;
    }

    if (_uiSize == 0) {
        return 0;
    }

    spi_FlashSetCS(0);                           /* 使能片选 */
    bsp_spi_swap(CMD_READ);                      /* 发送读命令 */
    bsp_spi_swap((_uiSrcAddr & 0xFF0000) >> 16); /* 发送扇区地址的高8bit */
    bsp_spi_swap((_uiSrcAddr & 0xFF00) >> 8);    /* 发送扇区地址中间8bit */
    bsp_spi_swap(_uiSrcAddr & 0xFF);             /* 发送扇区地址低8bit */
    while (_uiSize--) {
        /* 读一个字节 */
        ucValue = bsp_spiRead1();
        if (*_ucpTar++ != ucValue) {
            spi_FlashSetCS(1);
            return 1;
        }
    }
    spi_FlashSetCS(1);
    return 0;
}

/*
*********************************************************************************************************
*	函 数 名: spi_FlashNeedErase
*	功能说明: 判断写PAGE前是否需要先擦除。
*	形    参:   _ucpOldBuf ： 旧数据
*			   _ucpNewBuf ： 新数据
*			   _uiLen ：数据个数，不能超过页面大小
*	返 回 值: 0 : 不需要擦除， 1 ：需要擦除
*********************************************************************************************************
*/
static uint8_t spi_FlashNeedErase(uint8_t * _ucpOldBuf, uint8_t * _ucpNewBuf, uint16_t _usLen)
{
    uint16_t i;
    uint8_t ucOld;

    /*
    算法第1步：old 求反, new 不变
          old    new
              1101   0101
    ~     1111
            = 0010   0101

    算法第2步: old 求反的结果与 new 位与
              0010   old
    &	  0101   new
             =0000

    算法第3步: 结果为0,则表示无需擦除. 否则表示需要擦除
    */

    for (i = 0; i < _usLen; i++) {
        ucOld = *_ucpOldBuf++;
        ucOld = ~ucOld;

        /* 注意错误的写法: if (ucOld & (*_ucpNewBuf++) != 0) */
        if ((ucOld & (*_ucpNewBuf++)) != 0) {
            return 1;
        }
    }
    return 0;
}

/*
*********************************************************************************************************
*	函 数 名: spi_FlashAutoWritePage
*	功能说明: 写1个PAGE并校验,如果不正确则再重写两次。本函数自动完成擦除操作。
*	形    参:  	_pBuf : 数据源缓冲区；
*				_uiWriteAddr ：目标区域首地址
*				_usSize ：数据个数，不能超过页面大小
*	返 回 值: 0 : 错误， 1 ： 成功
*********************************************************************************************************
*/
static uint8_t spi_FlashAutoWritePage(uint32_t _uiWrAddr, uint8_t * _ucpSrc, uint16_t _usWrLen)
{
    uint16_t i;
    uint16_t j;           /* 用于延时 */
    uint32_t uiFirstAddr; /* 扇区首址 */
    uint8_t ucNeedErase;  /* 1表示需要擦除 */
    uint8_t cRet;

    /* 长度为0时不继续操作,直接认为成功 */
    if (_usWrLen == 0) {
        return 1;
    }

    /* 如果偏移地址超过芯片容量则退出 */
    if (_uiWrAddr >= g_tSF.TotalSize) {
        return 0;
    }

    /* 如果数据长度大于扇区容量，则退出 */
    if (_usWrLen > g_tSF.PageSize) {
        return 0;
    }

    /* 如果FLASH中的数据没有变化,则不写FLASH */
    spi_FlashReadBuffer(_uiWrAddr, s_spiBuf, _usWrLen);
    if (memcmp(s_spiBuf, _ucpSrc, _usWrLen) == 0) {
        return 1;
    }

    /* 判断是否需要先擦除扇区 */
    /* 如果旧数据修改为新数据，所有位均是 1->0 或者 0->0, 则无需擦除,提高Flash寿命 */
    ucNeedErase = 0;
    if (spi_FlashNeedErase(s_spiBuf, _ucpSrc, _usWrLen)) {
        ucNeedErase = 1;
    }

    uiFirstAddr = _uiWrAddr & (~(g_tSF.PageSize - 1));

    if (_usWrLen == g_tSF.PageSize) /* 整个扇区都改写 */
    {
        for (i = 0; i < g_tSF.PageSize; i++) {
            s_spiBuf[i] = _ucpSrc[i];
        }
    } else /* 改写部分数据 */
    {
        /* 先将整个扇区的数据读出 */
        spi_FlashReadBuffer(uiFirstAddr, s_spiBuf, g_tSF.PageSize);

        /* 再用新数据覆盖 */
        i = _uiWrAddr & (g_tSF.PageSize - 1);
        memcpy(&s_spiBuf[i], _ucpSrc, _usWrLen);
    }

    /* 写完之后进行校验，如果不正确则重写，最多3次 */
    cRet = 0;
    for (i = 0; i < 3; i++) {

        /* 如果旧数据修改为新数据，所有位均是 1->0 或者 0->0, 则无需擦除,提高Flash寿命 */
        if (ucNeedErase == 1) {
            spi_FlashEraseSector(uiFirstAddr); /* 擦除1个扇区 */
        }

        /* 编程一个PAGE */
        spi_FlashPageWrite(s_spiBuf, uiFirstAddr, g_tSF.PageSize);

        if (spi_FlashCmpData(_uiWrAddr, _ucpSrc, _usWrLen) == 0) {
            cRet = 1;
            break;
        } else {
            if (spi_FlashCmpData(_uiWrAddr, _ucpSrc, _usWrLen) == 0) {
                cRet = 1;
                break;
            }

            /* 失败后延迟一段时间再重试 */
            for (j = 0; j < 10000; j++)
                ;
        }
    }

    return cRet;
}

/*
*********************************************************************************************************
*	函 数 名: spi_FlashWriteBuffer
*	功能说明: 写1个扇区并校验,如果不正确则再重写两次。本函数自动完成擦除操作。
*	形    参:  	_pBuf : 数据源缓冲区；
*				_uiWrAddr ：目标区域首地址
*				_usSize ：数据个数，不能超过页面大小
*	返 回 值: 1 : 成功， 0 ： 失败
*********************************************************************************************************
*/
uint16_t spi_FlashWriteBuffer(uint32_t _uiWriteAddr, uint8_t * _pBuf, uint16_t _usWriteSize)
{
    uint16_t NumOfPage = 0, NumOfSingle = 0, Addr = 0, count = 0, temp = 0, wroteCnt = 0;

    Addr = _uiWriteAddr % g_tSF.PageSize;
    count = g_tSF.PageSize - Addr;
    NumOfPage = _usWriteSize / g_tSF.PageSize;
    NumOfSingle = _usWriteSize % g_tSF.PageSize;

    if (Addr == 0) /* 起始地址是页面首地址  */
    {
        if (NumOfPage == 0) /* 数据长度小于页面大小 */
        {
            if (spi_FlashAutoWritePage(_uiWriteAddr, _pBuf, _usWriteSize) == 0) {
                return wroteCnt;
            }
            wroteCnt += _usWriteSize;
        } else /* 数据长度大于等于页面大小 */
        {
            while (NumOfPage--) {
                if (spi_FlashAutoWritePage(_uiWriteAddr, _pBuf, g_tSF.PageSize) == 0) {
                    return wroteCnt;
                }
                _uiWriteAddr += g_tSF.PageSize;
                _pBuf += g_tSF.PageSize;
                wroteCnt += g_tSF.PageSize;
            }
            if (spi_FlashAutoWritePage(_uiWriteAddr, _pBuf, NumOfSingle) == 0) {
                return wroteCnt;
            }
            wroteCnt += NumOfSingle;
        }
    } else /* 起始地址不是页面首地址  */
    {
        if (NumOfPage == 0) /* 数据长度小于页面大小 */
        {
            if (NumOfSingle > count) /* (_usWriteSize + _uiWriteAddr) > SPI_FLASH_PAGESIZE */
            {
                temp = NumOfSingle - count;

                if (spi_FlashAutoWritePage(_uiWriteAddr, _pBuf, count) == 0) {
                    return wroteCnt;
                }

                _uiWriteAddr += count;
                _pBuf += count;
                wroteCnt += count;

                if (spi_FlashAutoWritePage(_uiWriteAddr, _pBuf, temp) == 0) {
                    return wroteCnt;
                }
                wroteCnt += temp;
            } else {
                if (spi_FlashAutoWritePage(_uiWriteAddr, _pBuf, _usWriteSize) == 0) {
                    return wroteCnt;
                }
                wroteCnt += _usWriteSize;
            }
        } else /* 数据长度大于等于页面大小 */
        {
            _usWriteSize -= count;
            NumOfPage = _usWriteSize / g_tSF.PageSize;
            NumOfSingle = _usWriteSize % g_tSF.PageSize;

            if (spi_FlashAutoWritePage(_uiWriteAddr, _pBuf, count) == 0) {
                return wroteCnt;
            }

            _uiWriteAddr += count;
            _pBuf += count;
            wroteCnt += count;

            while (NumOfPage--) {
                if (spi_FlashAutoWritePage(_uiWriteAddr, _pBuf, g_tSF.PageSize) == 0) {
                    return wroteCnt;
                }
                _uiWriteAddr += g_tSF.PageSize;
                _pBuf += g_tSF.PageSize;
                wroteCnt += g_tSF.PageSize;
            }

            if (NumOfSingle != 0) {
                if (spi_FlashAutoWritePage(_uiWriteAddr, _pBuf, NumOfSingle) == 0) {
                    return wroteCnt;
                }
                wroteCnt += NumOfSingle;
            }
        }
    }
    return wroteCnt; /* 成功 */
}

/*
*********************************************************************************************************
*	函 数 名: spi_FlashReadID
*	功能说明: 读取器件ID
*	形    参:  无
*	返 回 值: 32bit的器件ID (最高8bit填0，有效ID位数为24bit）
*********************************************************************************************************
*/
uint32_t spi_FlashReadID(void)
{
    uint32_t uiID;
    uint8_t id1, id2, id3;

    spi_FlashSetCS(0);      /* 使能片选 */
    bsp_spi_swap(CMD_RDID); /* 发送读ID命令 */
    id1 = bsp_spiRead1();   /* 读ID的第1个字节 */
    id2 = bsp_spiRead1();   /* 读ID的第2个字节 */
    id3 = bsp_spiRead1();   /* 读ID的第3个字节 */
    spi_FlashSetCS(1);      /* 禁能片选 */

    uiID = ((uint32_t)id1 << 16) | ((uint32_t)id2 << 8) | id3;

    return uiID;
}

/*
*********************************************************************************************************
*	函 数 名: spi_FlashReadInfo
*	功能说明: 读取器件ID,并填充器件参数
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void spi_FlashReadInfo(void)
{
	uint8_t cnt=0;

    /* 自动识别串行Flash型号 */
	while (g_tSF.ChipID == 0 && ++cnt < 5){
        g_tSF.ChipID = spi_FlashReadID(); /* 芯片ID */
        vTaskDelay(500);
	}
        switch (g_tSF.ChipID) {
            case SST25VF016B_ID:
                strcpy(g_tSF.ChipName, "SST25VF016B");
                g_tSF.TotalSize = 2 * 1024 * 1024; /* 总容量 = 2M */
                g_tSF.PageSize = 4 * 1024;         /* 页面大小 = 4K */
                break;

            case MX25L1606E_ID:
                strcpy(g_tSF.ChipName, "MX25L1606E");
                g_tSF.TotalSize = 2 * 1024 * 1024; /* 总容量 = 2M */
                g_tSF.PageSize = 4 * 1024;         /* 页面大小 = 4K */
                break;

            case W25Q64BV_ID:
                strcpy(g_tSF.ChipName, "W25Q64BV");
                g_tSF.TotalSize = 8 * 1024 * 1024; /* 总容量 = 8M */
                g_tSF.PageSize = 4 * 1024;         /* 页面大小 = 4K */
                break;

            case W25Q64FW_ID:
                strcpy(g_tSF.ChipName, "W25Q64FW");
                g_tSF.TotalSize = 8 * 1024 * 1024; /* 总容量 = 8M */
                g_tSF.PageSize = 4 * 1024;         /* 页面大小 = 4K */
                break;

            default:
                strcpy(g_tSF.ChipName, "Unknow Flash");
                g_tSF.TotalSize = 2 * 1024 * 1024;
                g_tSF.PageSize = 4 * 1024;
                break;
        }

}

/*
*********************************************************************************************************
*	函 数 名: spi_FlashWriteEnable
*	功能说明: 向器件发送写使能命令
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
static void spi_FlashWriteEnable(void)
{
    spi_FlashSetCS(0);      /* 使能片选 */
    bsp_spi_swap(CMD_WREN); /* 发送命令 */
    spi_FlashSetCS(1);      /* 禁能片选 */
}

/*
*********************************************************************************************************
*	函 数 名: spi_FlashWriteStatus
*	功能说明: 写状态寄存器
*	形    参:  _ucValue : 状态寄存器的值
*	返 回 值: 无
*********************************************************************************************************
*/
static void spi_FlashWriteStatus(uint8_t _ucValue)
{

    if (g_tSF.ChipID == SST25VF016B_ID) {
        /* 第1步：先使能写状态寄存器 */
        spi_FlashSetCS(0);       /* 使能片选 */
        bsp_spi_swap(CMD_EWRSR); /* 发送命令， 允许写状态寄存器 */
        spi_FlashSetCS(1);       /* 禁能片选 */

        /* 第2步：再写状态寄存器 */
        spi_FlashSetCS(0);      /* 使能片选 */
        bsp_spi_swap(CMD_WRSR); /* 发送命令， 写状态寄存器 */
        bsp_spi_swap(_ucValue); /* 发送数据：状态寄存器的值 */
        spi_FlashSetCS(1);      /* 禁能片选 */
    } else {
        spi_FlashSetCS(0);      /* 使能片选 */
        bsp_spi_swap(CMD_WRSR); /* 发送命令， 写状态寄存器 */
        bsp_spi_swap(_ucValue); /* 发送数据：状态寄存器的值 */
        spi_FlashSetCS(1);      /* 禁能片选 */
    }
}

/*
*********************************************************************************************************
*	函 数 名: spi_FlashWaitForWriteEnd
*	功能说明: 采用循环查询的方式等待器件内部写操作完成
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
static void spi_FlashWaitForWriteEnd(void)
{
    spi_FlashSetCS(0);      /* 使能片选 */
    bsp_spi_swap(CMD_RDSR); /* 发送命令， 读状态寄存器 */
    while ((bsp_spiRead1() & WIP_FLAG) == SET)
        ;              /* 判断状态寄存器的忙标志位 */
    spi_FlashSetCS(1); /* 禁能片选 */
}

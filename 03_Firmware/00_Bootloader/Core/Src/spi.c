/******************************************************************************
 *
 *
 * All Rights Reserved.
 *
 * @file spi.c
 *
 * @par dependencies
 * - spi.h
 *
 * @author  Ethan-Hang |   |        / Ethan-Hang (SPI2 port)
 *
 * @brief SPI2 port for the W25Q64 NOR flash used by the OTA staging path.
 *
 *        Wiring (must match APP CubeMX config in 01_APP/Core/Src/spi.c):
 *          PB10 — SPI2_SCK   (AF5)
 *          PB14 — SPI2_MISO  (AF5)
 *          PB15 — SPI2_MOSI  (AF5)
 *          PB13 — software CS, GPIO output
 *
 *        Clock / format mirrors hspi2 in 01_APP: Master, 2-line full-duplex,
 *        8-bit MSB-first, CPOL=Low, CPHA=1Edge (SPI Mode 0), prescaler=2
 *        (APB1 = 42 MHz → 21 MHz SCK, well below W25Q64's 80 MHz limit).
 *
 * @version V2.0 2026-05-14  Migrated from SPI1/PA4-7 to SPI2/PB10/13/14/15
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/
/* Includes ------------------------------------------------------------------*/
#include "spi.h"
/* Private function prototypes -----------------------------------------------*/
u8 SPI2_ReadWriteByte(u8 WriteData);
/* extern variables ----------------------------------------------------------*/
extern volatile uint32_t SysTickUptime;

/**
 * CS line setup — drive PB13 high (deasserted) as GPIO output before
 * enabling the SPI peripheral so the slave doesn't latch garbage during
 * SPI controller init.
 **/
static void CS_IO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Pin = F_CS_Pin;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(F_CS_GPIO_Port, &GPIO_InitStructure);
}

/**
 * @brief Bring up SPI2 + W25Q64 CS line.
 *
 * @param[in]  : None.
 * @param[out] : None.
 * @return     : None.
 * */
void SPI2_Init(void)
{
    /**
     * Enable GPIOB and SPI2 clocks. SPI2 hangs on APB1 (unlike SPI1 on APB2),
     * so use RCC_APB1PeriphClockCmd here.
     **/
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);

    CS_IO_Init();
    /* Deassert CS before configuring the bus to avoid spurious clocks
       landing on the slave during AF reassignment. */
    GPIO_WriteBit(F_CS_GPIO_Port, F_CS_Pin, (BitAction)1);

    /**
     * SCK/MISO/MOSI on PB10/PB14/PB15 set to AF5 for SPI2. PB13 stays a plain
     * GPIO (we drive CS in software). The AF for SPI2 on F411 is AF5 across
     * all three pins.
     **/
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_PinAFConfig(GPIOB, GPIO_PinSource10, GPIO_AF_SPI2);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource14, GPIO_AF_SPI2);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource15, GPIO_AF_SPI2);

    RCC_APB1PeriphResetCmd(RCC_APB1Periph_SPI2, ENABLE);
    RCC_APB1PeriphResetCmd(RCC_APB1Periph_SPI2, DISABLE);

    SPI_InitTypeDef SPI_InitStructure;
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_Init(SPI2, &SPI_InitStructure);

    SPI_Cmd(SPI2, ENABLE);

    /* Send a dummy byte to flush the shift register and verify the bus. */
    SPI2_ReadWriteByte(0xff);
}

/**
 * @brief Send one byte and read one byte (blocking).
 *
 * @param[in]  WriteData : byte to clock out.
 * @param[out] : None.
 * @return     : byte clocked in from MISO during the same SCK burst.
 * */
u8 SPI2_ReadWriteByte(u8 WriteData)
{
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET)
        ;
    SPI_I2S_SendData(SPI2, WriteData);

    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET)
        ;
    return SPI_I2S_ReceiveData(SPI2);
}

/**
 * @brief Streaming write with timeout (TX bytes consumed, RX bytes drained
 *        and discarded so the shift register stays in sync).
 *
 * @param[in]  WriteData : pointer to source buffer.
 * @param[in]  dataSize  : bytes to write.
 * @param[in]  timeout   : max wait in SysTickUptime ms.
 * @param[out] : None.
 * @return     : 1 success, 0 timeout.
 * */
u8 SPI2_WriteByte(u8 *WriteData, u16 dataSize, u32 timeout)
{
    u32 time = timeout;
    u32 current_time = SysTickUptime;
    u16 txsize = dataSize;
    u16 rxsize = dataSize;
    u8 *pTxBuffPtr = WriteData;
    u8 txflow = 1u;
    while ((txsize > 0) || (rxsize > 0))
    {
        if ((SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) != RESET) &&
            (txsize > 0) && (txflow == 1))
        {
            SPI_I2S_SendData(SPI2, *pTxBuffPtr);
            pTxBuffPtr += sizeof(u8);
            txsize--;
            txflow = 0;
        }
        if ((SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) != RESET) &&
            (rxsize > 0) && (txflow == 0))
        {
            SPI_I2S_ReceiveData(SPI2);
            rxsize--;
            txflow = 1;
        }
        if ((SysTickUptime - current_time) >= time)
        {
            SPI_I2S_ClearFlag(SPI2, SPI_I2S_FLAG_OVR);
            return 0;
        }
    }
    SPI_I2S_ClearFlag(SPI2, SPI_I2S_FLAG_OVR);
    return 1;
}

/**
 * @brief Streaming read; transmits the caller's buffer as dummy bytes and
 *        overwrites it with received bytes in place.
 *
 * @param[in,out] ReadData : on entry contains dummy bytes (caller filled
 *                           0xFF in sfud_port for read paths); on success
 *                           holds the received bytes.
 * @param[in]     dataSize : bytes to read.
 * @param[in]     timeout  : max wait in SysTickUptime ms.
 *
 * @return  1 success, 0 timeout.
 * */
u8 SPI2_ReadByte(u8 *ReadData, u16 dataSize, u32 timeout)
{
    u32 time = timeout;
    u32 current_time = SysTickUptime;
    u16 txsize = dataSize;
    u16 rxsize = dataSize;
    u8 *pTxBuffPtr = ReadData;
    u8 *pRxBuffPtr = ReadData;
    u8 txflow = 1u;
    while ((txsize > 0) || (rxsize > 0))
    {
        if ((SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) != RESET) &&
            (txsize > 0) && (txflow == 1))
        {
            SPI_I2S_SendData(SPI2, *pTxBuffPtr);
            pTxBuffPtr += sizeof(u8);
            txsize--;
            txflow = 0;
        }
        if ((SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) != RESET) &&
            (rxsize > 0) && (txflow == 0))
        {
            *pRxBuffPtr = SPI_I2S_ReceiveData(SPI2);
            pRxBuffPtr += sizeof(u8);
            rxsize--;
            txflow = 1;
        }
        if ((SysTickUptime - current_time) >= time)
        {
            return 0;
        }
    }
    return 1;
}

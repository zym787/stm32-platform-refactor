/******************************************************************************
 *  
 *
 * All Rights Reserved.
 *
 * @file Spi.h
 *
 * @par dependencies
 * - Spi.h
 *
 * @author  Ethan-Hang |   |       
 *
 * @brief Functions related to reading and writing in the chip's flash area.
 *
 * Processing flow:
 *
 * call directly.
 *
 * @version V1.0 2024-09-13
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __SPI_H
#define __SPI_H
/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* W25Q64 sits on SPI2 (matches APP-side wiring documented in 01_APP CLAUDE.md):
   SCK=PB10, MISO=PB14, MOSI=PB15, CS=PB13 (software, GPIO). */
#define F_CS_Pin       GPIO_Pin_13
#define F_CS_GPIO_Port GPIOB
/* Exported functions ------------------------------------------------------- */

void SPI2_Init(void);
u8   SPI2_WriteByte(u8 *WriteData, u16 dataSize, u32 timeout);
u8   SPI2_ReadByte(u8 *ReadData, u16 dataSize, u32 timeout);
#endif /* __FLASH_H */

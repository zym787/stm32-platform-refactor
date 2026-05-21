/******************************************************************************
 * @file ec_bsp_aht21_reg.h
 * 
 * @author Ethan-Hang
 * 
 * @brief Provide the address of AHT21's registers.
 * 
 * Processing flow:
 * 
 * @version V1.0
 * 
 * @note 1 tab == 4 spaces!
 * 
 *****************************************************************************/
#ifndef __EC_BSP_AHT21_REG_H__
#define __EC_BSP_AHT21_REG_H__

#define AHT21_IIC_ADDR	 		                       0x38
#define AHT21_REG_WRITE_ADDR                           0x70
#define AHT21_REG_READ_ADDR                            0x71

#define AHT21_IDLE		 		                       0x00
#define AHT21_BUSY				                       0x01

#define AHT21_REG_POINTER_AC 	                       0xAC
#define AHT21_REG_MEASURE_CMD_ARGS1 				   0x33
#define AHT21_REG_MEASURE_CMD_ARGS2 				   0x00

#define AHT21_REG_CAL_1B                               0x1B
#define AHT21_REG_CAL_1C                               0x1C
#define AHT21_REG_CAL_1E                               0x1E

#endif //__EC_BSP_AHT21_REG_H__

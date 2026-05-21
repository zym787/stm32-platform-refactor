/******************************************************************************
 * @file
 *
 * @par dependencies
 *
 * @author Ethan-Hang
 *
 * @brief
 *
 * Processing flow:
 *
 *
 * @version V1.0 2026--
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "mpu.h"
#include "main.h"

//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define WATCH_ADDRESS 0x20000000U
//******************************** Defines **********************************//

//******************************* Declaring *********************************//

//******************************* Declaring *********************************//

//******************************* Functions *********************************//
void MPU_Config(void)
{
    HAL_MPU_Disable();

    MPU_Region_InitTypeDef MPU_InitStruct;
    MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
    MPU_InitStruct.BaseAddress      = (uint32_t)WATCH_ADDRESS;
    MPU_InitStruct.Size             = MPU_REGION_SIZE_32B;
    MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
    MPU_InitStruct.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;
    MPU_InitStruct.IsCacheable      = MPU_ACCESS_CACHEABLE;
    MPU_InitStruct.IsShareable      = MPU_ACCESS_NOT_SHAREABLE;
    MPU_InitStruct.Number           = MPU_REGION_NUMBER0;
    MPU_InitStruct.TypeExtField     = MPU_TEX_LEVEL0;
    MPU_InitStruct.SubRegionDisable = 0x00;
    MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;

    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}
//******************************* Functions *********************************//
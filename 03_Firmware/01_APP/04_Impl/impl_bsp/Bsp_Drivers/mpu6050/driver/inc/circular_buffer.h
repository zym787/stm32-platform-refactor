/******************************************************************************
 * @file circular_buffer.h
 *
 * @par dependencies
 * - stdint
 * - stdlib
 *
 * @author Ethan-Hang
 *
 * @brief Declare circular buffer utilities for MPUXXXX DMA packet storage.
 *
 * Processing flow:
 * Provide ring-buffer structure and initialization interface.
 * @version V1.0 2026-1-6
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#ifndef __CIRCULAR_BUFFER_H__
#define __CIRCULAR_BUFFER_H__

//******************************** Includes *********************************//
#include <stdint.h>
#include <stdlib.h>
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define MPUXXXX_DATA_PACKET_SIZE                         14

typedef struct circular_buffer
{
    uint8_t  *                  buffer;
    uint8_t                   capacity;
    uint8_t                      rflag;
    uint8_t                      wflag;

    uint8_t  * (*pf_get_rbuffer_addr)(struct circular_buffer const  *  const );
    uint8_t  * (*pf_get_wbuffer_addr)(struct circular_buffer const  *  const );
    void       (*pf_data_writed     )(struct circular_buffer        *  const );
    void       (*pf_data_readed     )(struct circular_buffer        *  const );
} circular_buffer_t;

//******************************** Defines **********************************//

//******************************* Declaring *********************************//
void               circular_buffer_init        (circular_buffer_t * const p_buffer, uint8_t size);

/**
 * @brief   Get a pointer to the singleton circular buffer instance.
 *          External modules must call this instead of using extern.
 *
 * @return  Pointer to the circular buffer instance.
 *
 * */
circular_buffer_t * circular_buffer_get_instance(void);

//******************************* Declaring *********************************//

#endif // end of __CIRCULAR_BUFFER_H__

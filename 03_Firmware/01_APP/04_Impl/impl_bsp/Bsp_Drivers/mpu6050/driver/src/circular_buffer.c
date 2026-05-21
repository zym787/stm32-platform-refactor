/******************************************************************************
 * @file circular_buffer.c
 *
 * @par dependencies
 * - circular_buffer.h
 *
 * @author Ethan-Hang
 *
 * @brief   Circular buffer implementation for
 *          MPU6050 DMA data buffering.
 *          Provides a fixed-capacity ring buffer
 *          with separate read/write pointers and
 *          function-pointer-based interface.
 *
 * Processing flow:
 *   1. circular_buffer_init() allocates the buffer
 *      via pvPortMalloc and binds function pointers.
 *   2. Writer calls pf_get_wbuffer_addr() to get
 *      the current write slot address, then calls
 *      pf_data_writed() to advance the write flag.
 *   3. Reader calls pf_get_rbuffer_addr() to get
 *      the current read slot address, then calls
 *      pf_data_readed() to advance the read flag.
 *
 * @version V1.0 2026-1-6
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "osal_heap.h"
#include "circular_buffer.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
static circular_buffer_t g_circular_buffer;

//******************************** Defines **********************************//

//******************************* Declaring *********************************//
/**
 * @brief   Get a pointer to the singleton circular buffer instance.
 *          External modules must use this instead of extern.
 *
 * @return  Pointer to the circular buffer instance.
 *
 * */
circular_buffer_t * circular_buffer_get_instance(void)
{
    return &g_circular_buffer;
}


/**
 * @brief   Get the address of the current read slot
 *          in the circular buffer.
 * 
 * @param[in] p_buffer : Pointer to the circular
 *                       buffer instance.
 * 
 * @param[out] : None
 * 
 * @return  Pointer to the current read slot start
 *          address.
 * 
 * */
static uint8_t *(get_rbuffer_addr)(circular_buffer_t const * const  p_buffer)
{
    return (uint8_t *)(p_buffer->buffer + 
                                 (p_buffer->rflag * MPUXXXX_DATA_PACKET_SIZE));
}

/**
 * @brief   Get the address of the current write
 *          slot in the circular buffer.
 * 
 * @param[in] p_buffer : Pointer to the circular
 *                       buffer instance.
 * 
 * @param[out] : None
 * 
 * @return  Pointer to the current write slot start
 *          address.
 * 
 * */
static uint8_t *(get_wbuffer_addr)(circular_buffer_t const * const  p_buffer)
{
    return (uint8_t *)(p_buffer->buffer + 
                                 (p_buffer->wflag * MPUXXXX_DATA_PACKET_SIZE));
}

/**
 * @brief   Advance the write flag by one slot.
 *          Wraps around when the end of the buffer
 *          is reached (modulo capacity).
 * 
 * @param[in] p_buffer : Pointer to the circular
 *                       buffer instance.
 * 
 * @param[out] : None
 * 
 * @return  None
 * 
 * */
static void     (data_writed     )(circular_buffer_t * const p_buffer)
{
    p_buffer->wflag = (p_buffer->wflag + 1) % p_buffer->capacity;
}

/**
 * @brief   Advance the read flag by one slot.
 *          Wraps around when the end of the buffer
 *          is reached (modulo capacity).
 * 
 * @param[in] p_buffer : Pointer to the circular
 *                       buffer instance.
 * 
 * @param[out] : None
 * 
 * @return  None
 * 
 * */
static void     (data_readed     )(circular_buffer_t * const p_buffer)
{
    p_buffer->rflag = (p_buffer->rflag + 1) % p_buffer->capacity;
}

/**
 * @brief   Initialize the circular buffer.
 *          Sets capacity, resets read/write flags,
 *          allocates the backing store via
 *          pvPortMalloc, and binds all function
 *          pointers. Returns without action if
 *          p_buffer is NULL, size is 0, or the
 *          allocation fails.
 * 
 * @param[in] p_buffer : Pointer to the circular
 *                       buffer instance to init.
 * @param[in] size     : Number of slots to allocate.
 *                       Each slot is
 *                       MPUXXXX_DATA_PACKET_SIZE
 *                       bytes.
 * 
 * @param[out] : None
 * 
 * @return  None
 * 
 * */
void circular_buffer_init(circular_buffer_t * const p_buffer, uint8_t size)
{
    if (NULL == p_buffer || size == 0)
    {
        return;
    }

    p_buffer->capacity = size;
    p_buffer->rflag    = 0;
    p_buffer->wflag    = 0;

    p_buffer->buffer = (uint8_t *)osal_heap_malloc(size * MPUXXXX_DATA_PACKET_SIZE);
    if (NULL == p_buffer->buffer)
    {
        return;
    }

    p_buffer->pf_data_readed         =         data_readed;
    p_buffer->pf_data_writed         =         data_writed;
    p_buffer->pf_get_rbuffer_addr    =    get_rbuffer_addr;
    p_buffer->pf_get_wbuffer_addr    =    get_wbuffer_addr;

}
//******************************* Declaring *********************************//


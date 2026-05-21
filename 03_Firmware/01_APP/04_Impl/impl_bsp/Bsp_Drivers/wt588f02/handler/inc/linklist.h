/******************************************************************************
 * @file linklist.h
 *
 * @par dependencies
 *
 * @author Ethan-Hang
 *
 * @brief Define priority linked-list data structures for WT588 voice requests.
 *
 * Processing flow:
 * Provide list container types and constructor API for voice queue handling.
 * @version V1.0 2026-4-6
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/
#pragma once
#ifndef __LINKLIST_H__
#define __LINKLIST_H__

//******************************** Includes *********************************//
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "Debug.h"

//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define WT588_MAX_PRIORITY         (15)
// #define WT588_MAX_ITEM             (20)

//******************************** Defines **********************************//

//******************************* Declaring *********************************//
typedef enum
{
    LIST_OK               = 0,        /* Operation successful                */
    LIST_ERROR            = 1,        /* General error                       */
    LIST_ERRORTIMEOUT     = 2,        /* Timeout error                       */
    LIST_ERRORRESOURCE    = 3,        /* Resource unavailable                */
    LIST_ERRORPARAMETER   = 4,        /* Invalid parameter                   */
    LIST_ERRORNOMEMORY    = 5,        /* Out of memory                       */
    LIST_RESERVED         = 0xFF,     /* list Reserved                       */
} list_status_t;

typedef struct 
{
    void *( *pf_list_malloc )(size_t );
    void  ( *pf_list_free   )(void * );
} list_malloc_interface_t;

typedef struct list_voice_node
{
    uint8_t                     volume;
    uint8_t                   priority;
    uint8_t                volume_addr;

    struct list_voice_node    *   next;
    struct list_voice_node    *   prev;
} list_voice_node_t;

typedef struct list_handler list_handler_t;
struct list_handler
{
    list_malloc_interface_t *list_malloc_interface;

    uint8_t node_count_by_priority[WT588_MAX_PRIORITY];
    list_voice_node_t priority_list_heads[WT588_MAX_PRIORITY];

    uint8_t current_priority;
    uint8_t highest_priority;

    list_status_t (*pf_list_add_node)(list_handler_t *, list_voice_node_t *);
    list_status_t (*pf_list_del_node)(list_handler_t *, list_voice_node_t *);
    list_status_t (*pf_list_sort    )(list_handler_t *, uint8_t            );

    list_voice_node_t *(*pf_get_first_node)(list_handler_t *);
    
    bool (*pf_list_is_empty)(list_handler_t *);
};

//******************************* Declaring *********************************//

//******************************* Functions *********************************//
list_status_t list_handler_construct(list_handler_t   *         list_instance,
                            list_malloc_interface_t   * list_malloc_interface);

//******************************* Functions *********************************//
#endif /* __LINKLIST_H__ */

/******************************************************************************
 * @file linklist.c
 *
 * @par dependencies
 *
 * @author Ethan-Hang
 *
 * @brief Implement priority linked-list add/delete/sort/query operations.
 *
 * Processing flow:
 * Maintain per-priority doubly linked lists and merge-sort by volume address.
 * @version V1.0 2026-4-7
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "linklist.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//

//******************************** Defines **********************************//

//******************************* Declaring *********************************//

/**
 * @brief Split a doubly linked list into two halves.
 *
 * @param[in] head : Head node of the list.
 *
 * @param[out] : None
 *
 * @return Pointer to the head of the second half.
 *
 * */
static list_voice_node_t *list_split_middle(list_voice_node_t *head)
{
    list_voice_node_t *slow = head;
    list_voice_node_t *fast = head;

    while (NULL != fast->next && NULL != fast->next->next)
    {
        slow = slow->next;
        fast = fast->next->next;
    }

    list_voice_node_t *second = slow->next;
    slow->next = NULL;
    if (NULL != second)
    {
        second->prev = NULL;
    }

    return second;
}

/**
 * @brief Merge two sorted lists by volume_addr (ascending).
 *
 * @param[in] left  : Head of the first sorted list.
 * @param[in] right : Head of the second sorted list.
 *
 * @param[out] : None
 *
 * @return Head of the merged sorted list.
 *
 * */
static list_voice_node_t *list_merge_sorted(list_voice_node_t *left,
                                            list_voice_node_t *right)
{
    list_voice_node_t dummy = {0};
    list_voice_node_t *tail = &dummy;

    while (NULL != left && NULL != right)
    {
        if (left->volume_addr <= right->volume_addr)
        {
            list_voice_node_t *next = left->next;
            tail->next = left;
            left->prev = tail;
            tail = left;
            left = next;
        }
        else
        {
            list_voice_node_t *next = right->next;
            tail->next = right;
            right->prev = tail;
            tail = right;
            right = next;
        }
    }

    while (NULL != left)
    {
        list_voice_node_t *next = left->next;
        tail->next = left;
        left->prev = tail;
        tail = left;
        left = next;
    }

    while (NULL != right)
    {
        list_voice_node_t *next = right->next;
        tail->next = right;
        right->prev = tail;
        tail = right;
        right = next;
    }

    tail->next = NULL;

    if (NULL != dummy.next)
    {
        dummy.next->prev = NULL;
    }

    return dummy.next;
}

/**
 * @brief Sort a doubly linked list by merge sort.
 *
 * @param[in] head : Head of the list.
 *
 * @param[out] : None
 *
 * @return Head of the sorted list.
 *
 * */
static list_voice_node_t *list_merge_sort(list_voice_node_t *head)
{
    if (NULL == head || NULL == head->next)
    {
        return head;
    }

    list_voice_node_t *second = list_split_middle(head);
    list_voice_node_t *left = list_merge_sort(head);
    list_voice_node_t *right = list_merge_sort(second);

    return list_merge_sorted(left, right);
}

//******************************* Declaring *********************************//

//******************************* Functions *********************************//
/**
 * @brief Add a voice node to priority linked list
 *
 * Creates deep copy of input node, inserts at tail of corresponding priority list,
 * and updates highest priority if necessary.
 *
 * @param[in] handler_inst : Pointer to list handler instance
 * @param[in] node         : Pointer to source node data
 *
 * @return list_status_t LIST_OK if successful, error code otherwise
 *
 * */
list_status_t (list_add_node)(list_handler_t *handler_inst,
                           list_voice_node_t *node)
{
    list_status_t ret = LIST_OK;
    /************ 1.Checking input parameters ************/
    if (NULL == handler_inst || NULL == node)
    {
        DEBUG_OUT(e, LIST_ERR_LOG_TAG, 
                    "list add node input error parameter");
        ret = LIST_ERRORPARAMETER;
        return ret;
    }

    /************* 2.Checking the Resources **************/
    if (node->priority >= WT588_MAX_PRIORITY             ||
        0              ==     node->priority)
    {
        DEBUG_OUT(e, LIST_ERR_LOG_TAG, 
                     "list add node input error priority");
        ret = LIST_ERRORPARAMETER;
        return ret;
    }
    if (NULL == handler_inst->list_malloc_interface              || 
        NULL == handler_inst->list_malloc_interface->pf_list_malloc)
    {
        DEBUG_OUT(e, LIST_ERR_LOG_TAG, 
                   "list add node malloc interface error");
        ret = LIST_ERRORRESOURCE;
        return ret;
    }

    /*************** 3. Find the tail node ***************/
    uint8_t index = node->priority;
    list_voice_node_t *tail = &(handler_inst->priority_list_heads[index]);
    while (tail->next != NULL)
    {
        tail = tail->next;
    }
    
    /********* 4. Create the new node(deep copy) *********/
    list_voice_node_t *new_node = (list_voice_node_t *)handler_inst->\
              list_malloc_interface->pf_list_malloc(sizeof(list_voice_node_t));
    if (NULL == new_node)
    {
        DEBUG_OUT(e, LIST_ERR_LOG_TAG, 
                   "list add node malloc new node failed");
        ret = LIST_ERRORNOMEMORY;
        return ret;
    }
    new_node->volume             =            node->volume;
    new_node->priority           =          node->priority;
    new_node->volume_addr        =       node->volume_addr;

    /*********** 5. Update the amount of node ************/
    tail->next = new_node;
    new_node->next = NULL;
    new_node->prev = tail;
    handler_inst->node_count_by_priority[index]++;

    /****** 6. Update highest priority if necessary ******/
    if (new_node->priority < handler_inst->highest_priority)
    {
        handler_inst->highest_priority = new_node->priority;
    }

    return ret;
}

/**
 * @brief Delete a voice node from priority linked list
 *
 * Searches for matching node pointer in priority list, removes it,
 * frees memory, and updates highest priority if list becomes empty.
 *
 * @param[in] handler_inst : Pointer to list handler instance
 * @param[in] node         : Pointer to node to delete (must match stored pointer)
 *
 * @return list_status_t LIST_OK if successful, error code otherwise
 *
 * */
list_status_t (list_del_node)(list_handler_t *handler_inst,
                           list_voice_node_t *node)
{
    list_status_t ret = LIST_OK;
    /************ 1.Checking input parameters ************/
    if (NULL == handler_inst || NULL == node)
    {
        DEBUG_OUT(e, LIST_ERR_LOG_TAG, 
                    "list del node input error parameter");
        ret = LIST_ERRORPARAMETER;
        return ret;
    }

    /************* 2.Checking the Resources **************/
    if (node->priority >= WT588_MAX_PRIORITY             ||
        0              ==     node->priority)
    {
        DEBUG_OUT(e, LIST_ERR_LOG_TAG, 
                     "list add node input error priority");
        ret = LIST_ERRORPARAMETER;
        return ret;
    }
    if (NULL == handler_inst->list_malloc_interface             ||
        NULL == handler_inst->list_malloc_interface->pf_list_free)
    {
        DEBUG_OUT(e, LIST_ERR_LOG_TAG, 
                   "list del node malloc interface error");
        ret = LIST_ERRORRESOURCE;
        return ret;
    }

    /*************** 3. Find the del node ****************/
    uint8_t index = node->priority;
    list_voice_node_t *cur_node =  &(handler_inst->priority_list_heads[index]);
    cur_node = cur_node->next;
    while (cur_node)
    {
        if (cur_node == node)
        {
            cur_node->prev->next = cur_node->next;
            // except tail node
            if (NULL != cur_node->next)
            {
                cur_node->next->prev = cur_node->prev;
            }
            handler_inst->node_count_by_priority[index]--;
            handler_inst->list_malloc_interface->pf_list_free(cur_node);
            cur_node = NULL;

            // if no node left in this priority, update highest priority
            handler_inst->highest_priority = WT588_MAX_PRIORITY;
            // update highest priority if has the node
            for (uint8_t i = 0; i < WT588_MAX_PRIORITY; i++)
            {
                if (handler_inst->node_count_by_priority[i] > 0)
                {
                    handler_inst->highest_priority = i;
                    break;
                }
            }

            return ret;
        }
        cur_node = cur_node->next;
    }

    // no valid node found in list if code runs to here
    DEBUG_OUT(e, LIST_ERR_LOG_TAG, 
                "list del node error, node not found in list");
    ret = LIST_ERRORPARAMETER;
    return ret;
}

/**
 * @brief Sort nodes within a specific priority level
 *
 * Performs merge sort on nodes of given priority by volume_addr (ascending).
 * Only sorts when more than one node exists in the priority level.
 *
 * @param[in] handler_inst : Pointer to list handler instance
 * @param[in] priority     : Priority level to sort (0-14)
 *
 * @return list_status_t LIST_OK if successful, error code otherwise
 *
 * */
list_status_t (list_sort    )(list_handler_t *handler_inst, uint8_t priority)
{
    list_status_t ret = LIST_OK;
    /************ 1.Checking input parameters ************/
    if (NULL == handler_inst)
    {
        DEBUG_OUT(e, LIST_ERR_LOG_TAG, 
                        "list sort input error parameter");
        ret = LIST_ERRORPARAMETER;
        return ret;
    }

    /************* 2.Checking the Resources **************/
    if (priority >= WT588_MAX_PRIORITY                   ||
        0        ==     priority)
    {
        DEBUG_OUT(e, LIST_ERR_LOG_TAG, 
                         "list sort input error priority");
        ret = LIST_ERRORPARAMETER;
        return ret;
    }

    /********* 3.Check whether sorting is needed *********/
    if (handler_inst->node_count_by_priority[priority] <= 1)
    {
        DEBUG_OUT(d, LIST_LOG_TAG, 
                     "list sort warning, no need to sort");
        return ret;
    }

    /*************** 4. Merge sort by volume_addr *******/
    list_voice_node_t *head = &(handler_inst->priority_list_heads[priority]);
    list_voice_node_t *sorted_head = list_merge_sort(head->next);

    head->next = sorted_head;
    if (NULL != sorted_head)
    {
        sorted_head->prev = head;
    }

    DEBUG_OUT(d, LIST_LOG_TAG, "list sort success, priority: %u", priority);

    return ret;
}

/**
 * @brief Get the first node from highest priority list
 *
 * Retrieves the head node of the highest non-empty priority list.
 *
 * @param[in] handler_inst : Pointer to list handler instance
 *
 * @return list_voice_node_t* Pointer to first node, NULL if list empty or error
 *
 * */
list_voice_node_t *(get_first_node)(list_handler_t *handler_inst)
{
    /************ 1.Checking input parameters ************/
    if (NULL == handler_inst)
    {
        DEBUG_OUT(e, LIST_ERR_LOG_TAG, 
                    "get first node input error parameter");
        return NULL;
    }

    uint8_t highest_priority = handler_inst->highest_priority;
    if (highest_priority >= WT588_MAX_PRIORITY)
    {
        DEBUG_OUT(e, LIST_ERR_LOG_TAG, 
                    "get first node error, no valid node in list");
        return NULL;
    }

    list_voice_node_t *p_head = \
                    (handler_inst->priority_list_heads[highest_priority]).next;
    if (NULL == p_head)
    {
        DEBUG_OUT(e, LIST_ERR_LOG_TAG, 
                    "get first node error, no valid node in list");
    }

    return p_head;
}

/**
 * @brief Check if all priority lists are empty
 *
 * Iterates through all priority levels to determine if any nodes exist.
 *
 * @param[in] handler_inst : Pointer to list handler instance
 *
 * @return bool true if all lists empty, false otherwise
 *
 * */
bool (list_is_empty)(list_handler_t *handler_inst)
{
    /************ 1.Checking input parameters ************/
    if (NULL == handler_inst)
    {
        DEBUG_OUT(e, LIST_ERR_LOG_TAG, 
                    "list is empty input error parameter");
        return true;
    }

    for (uint8_t i = 0; i < WT588_MAX_PRIORITY; i++)
    {
        if (handler_inst->node_count_by_priority[i] > 0)
        {
            return false;
        }
    }

    return true;
}

/**
 * @brief Construct list handler instance
 *
 * Initializes list structures, zeroes counters, and mounts memory interfaces.
 *
 * @param[in] list_instance         : Pointer to list handler instance
 * @param[in] list_malloc_interface : Pointer to memory allocation interface
 *
 * @return list_status_t LIST_OK if successful, error code otherwise
 *
 * */
list_status_t list_handler_construct(list_handler_t   *          list_instance,
                            list_malloc_interface_t   *  list_malloc_interface)
{
    list_status_t ret = LIST_OK;

    /************ 1.Checking input parameters ************/
    if (NULL == list_instance        ||
        NULL == list_malloc_interface )
    {
        DEBUG_OUT(e, LIST_ERR_LOG_TAG,
           "list handler construct input error parameter");
        ret = LIST_ERRORPARAMETER;
        return ret;
    }

    /************* 2.Checking the Resources **************/
    if (NULL == list_malloc_interface->pf_list_malloc    ||
        NULL == list_malloc_interface->pf_list_free)
    {
        DEBUG_OUT(e, LIST_ERR_LOG_TAG,
          "list handler construct malloc interface error");
        ret = LIST_ERRORRESOURCE;
        return ret;
    }

    /************ 3.Initialize the Resources *************/
    memset(list_instance->node_count_by_priority, 0, \
                     sizeof(uint8_t) * WT588_MAX_PRIORITY);
    memset(list_instance->priority_list_heads,    0, \
           sizeof(list_voice_node_t) * WT588_MAX_PRIORITY);
    list_instance->current_priority  =  WT588_MAX_PRIORITY;
    list_instance->highest_priority  =  WT588_MAX_PRIORITY;

    /**************** 4.Mount interfaces *****************/
    // 3.1 mount external interfaces
    list_instance->list_malloc_interface =
                                     list_malloc_interface;

    // 3.2 mount internal interfaces
    list_instance->pf_list_add_node  =       list_add_node;
    list_instance->pf_list_del_node  =       list_del_node;
    list_instance->pf_list_sort      =           list_sort;
    list_instance->pf_get_first_node =      get_first_node;
    list_instance->pf_list_is_empty  =       list_is_empty;

    return ret;
}
//******************************* Functions *********************************//

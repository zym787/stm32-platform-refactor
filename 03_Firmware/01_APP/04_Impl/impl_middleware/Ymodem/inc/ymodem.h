
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _YMODEM_H_
#define _YMODEM_H_

/* Includes ------------------------------------------------------------------*/
#include "board_types.h"
#include "stdint.h"
/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
#define PACKET_SEQNO_INDEX      (1)
#define PACKET_SEQNO_COMP_INDEX (2)

#define PACKET_HEADER           (3)
#define PACKET_TRAILER          (2)
#define PACKET_OVERHEAD         (PACKET_HEADER + PACKET_TRAILER)
#define PACKET_SIZE             (128)
#define PACKET_1K_SIZE          (1024)

#define FILE_NAME_LENGTH        (256)
#define FILE_SIZE_LENGTH        (16)

#define SOH                     (0x01) /* start of 128-byte data packet */
#define STX                     (0x02) /* start of 1024-byte data packet */
#define EOT                     (0x04) /* end of transmission ???????? */
#define ACK                     (0x06) /* acknowledge ???*/
#define NAK                     (0x15) /* negative acknowledge ????? */
#define CA                                                                     \
    (0x18) /* two of these in succession aborts transfer ?????????????????? */
#define CRC16       (0x43) /* 'C' == 0x43, request 16-bit CRC ????CRC */

#define ABORT1      (0x41) /* 'A' == 0x41, abort by user */
#define ABORT2      (0x61) /* 'a' == 0x61, abort by user */

#define NAK_TIMEOUT (2000) //??????????
#define MAX_ERRORS  (3)    //??????????

#define IS_AF(c)            ((c >= 'A') && (c <= 'F'))
#define IS_af(c)            ((c >= 'a') && (c <= 'f'))
#define IS_09(c)            ((c >= '0') && (c <= '9'))
#define ISVALIDHEX(c)       IS_AF(c) || IS_af(c) || IS_09(c)
#define ISVALIDDEC(c)       IS_09(c)
#define CONVERTDEC(c)       (c - '0')

#define CONVERTHEX_alpha(c) (IS_AF(c) ? (c - 'A' + 10) : (c - 'a' + 10))
#define CONVERTHEX(c)       (IS_09(c) ? (c - '0') : CONVERTHEX_alpha(c))

/* Exported macro ------------------------------------------------------------*/

/* Receive state machine types -----------------------------------------------*/
typedef enum
{
    YMODEM_RX_STATE_FILE_INFO = 0, /* Processing filename packet */
    YMODEM_RX_STATE_FILE_DATA = 1  /* Processing file data packet */
} Ymodem_RxState_t;

typedef struct
{
    UINT8_t         *packet_data;
    UINT8_t          file_size[FILE_SIZE_LENGTH];
    UINT8_t         *file_ptr;
    UINT8_t         *buf_ptr;
    INT32_t          i;
    INT32_t          packet_length;
    INT32_t          session_done;
    INT32_t          file_done;
    INT32_t          packets_received;
    INT32_t          errors;
    INT32_t          session_begin;
    INT32_t          size;           /* Total file size from filename packet */
    INT32_t          bytes_received; /* Bytes received so far */
    INT32_t          eot_seen;       /* 1 = first EOT NAK'd, waiting for the
                                        second per Ymodem batch spec */
    Ymodem_RxState_t state;
    UINT8_t (*buf)[1030];
} Ymodem_RxContext_t;

/* State handler return values
 * ------------------------------------------------*/
typedef enum
{
    YMODEM_RX_HANDLER_ERROR    = -1, /* Error occurred, abort transfer */
    YMODEM_RX_HANDLER_CONTINUE = 0,  /* Continue processing */
    YMODEM_RX_HANDLER_DONE     = 1   /* State processing complete */
} Ymodem_RxHandlerStatus_t;

/* Packet receive return values
 * ------------------------------------------------*/
typedef enum
{
    YMODEM_PKT_SUCCESS = 0,  /* Packet received successfully */
    YMODEM_PKT_TIMEOUT = -1, /* Timeout or packet error */
    YMODEM_PKT_ABORT   = 1   /* Abort by user */
} Ymodem_PacketStatus_t;

/* Transfer receive return values
 * -----------------------------------------------*/
typedef enum
{
    YMODEM_RX_ABORTED     = 0,  /* Abort by sender (normal end) */
    YMODEM_RX_SIZE_ERR    = -1, /* Image size exceeds Flash size */
    YMODEM_RX_TIMEOUT_ERR = -4, /* Max errors reached, timeout */
    YMODEM_RX_FLASH_ERR   = -2, /* Flash programming error */
    YMODEM_RX_USER_ABORT  = -3  /* User abort (Ctrl+C) */
} Ymodem_ReceiveStatus_t;

/* Exported functions ------------------------------------------------------- */
INT32_t  Ymodem_Receive(UINT8_t (*)[1030]);

#endif /* _YMODEM_H_ */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

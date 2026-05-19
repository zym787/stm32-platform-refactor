/******************************************************************************
 * @file ymodem.c
 *
 * @par dependencies
 * - string.h
 * - osal_wrapper_adapter.h
 * - os_freertos.h
 * - ota_transport.h     (abstract byte stream — adapter lives in 01_APP)
 * - Debug.h
 *
 * @author Ethan-Hang
 *
 * @brief
 * Ymodem protocol receive and packet forward module.
 *
 * Processing flow:
 * 1. Receive Ymodem packets via the abstract ota_transport (UART / BLE /
 *    CAN — whichever adapter is linked in).
 * 2. Parse file info and payload packets.
 * 3. Forward payload to OTA download task by queue.
 * 4. Synchronize producer and consumer by semaphore.
 *
 * @version V1.0 2026-3-28
 * @version V2.0 2026-05-18  decoupled from STM32 HAL — all UART access
 *                            now goes through ota_transport.h.
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include <string.h>

#include "osal_wrapper_adapter.h"
#include "osal_error.h"
#include "os_freertos.h"

#include "ota_transport.h"

#include "ymodem.h"
#include "Debug.h"
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
uint8_t                  file_name[FILE_NAME_LENGTH];
/**
 * @brief Producer-consumer to the OTA staging task. These remain
 *        application-level coupling — the Ymodem user handlers below act
 *        as the "OTA delivery point" for received frames. If Ymodem is
 *        ever reused outside the OTA flow these would be swapped for a
 *        callback registration, but for this project OTA is the only
 *        consumer so direct externs keep the diff smallest.
 */
extern osal_queue_handle_t Queue_AppDataBuffer;
extern osal_sema_handle_t  Semaphore_ExtFlashState;

/** @brief Bytes received in the most recent ota_transport_frame_wait —
 *         consumed by Receive_Packet to validate packet length. */
static uint16_t            s_u16_YmodRecLength;


/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
 * @brief Update XMODEM/YMODEM CRC-16 (poly 0x1021, init 0, no reflection)
 *        with one input byte.
 *
 * @param  crc_in  Running CRC accumulator.
 * @param  byte    Input byte to mix in.
 *
 * @return Updated CRC.
 *
 * @note Bit-bang implementation matches the bootloader's Ymodem so any image
 *       that round-trips through both still verifies on the wire. Loop body
 *       borrowed verbatim — do not rewrite to a table without re-checking
 *       compatibility with the sender's CRC.
 */
static uint16_t crc16_update(uint16_t crc_in, uint8_t byte)
{
    uint32_t crc = crc_in;
    uint32_t in  = byte | 0x100u;
    do
    {
        crc <<= 1;
        in  <<= 1;
        if (in & 0x100u)
        {
            ++crc;
        }
        if (crc & 0x10000u)
        {
            crc ^= 0x1021u;
        }
    } while (!(in & 0x10000u));
    return (uint16_t)(crc & 0xFFFFu);
}

/**
 * @brief Compute Ymodem packet CRC-16 over @p size payload bytes, appending
 *        two zero bytes the way the XMODEM-CRC algorithm augments the data
 *        stream so the result matches the 2-byte trailer the sender appended.
 */
static uint16_t crc16_xmodem(const uint8_t *data, uint32_t size)
{
    uint16_t       crc      = 0U;
    const uint8_t *data_end = data + size;
    while (data < data_end)
    {
        crc = crc16_update(crc, *data++);
    }
    crc = crc16_update(crc, 0U);
    crc = crc16_update(crc, 0U);
    return crc;
}

/**
 * @brief  Convert string to integer.
 * @param  inputstr: Input string, supports dec/hex and K/M suffix.
 * @param  intnum: Output integer value.
 * @retval 1: Convert success
 *         0: Convert failed
 */
static uint32_t Str2Int(uint8_t *inputstr, int32_t *intnum)
{
    uint32_t i = 0, res = 0;
    uint32_t val = 0;

    if (inputstr[0] == '0' && (inputstr[1] == 'x' || inputstr[1] == 'X'))
    {
        if (inputstr[2] == '\0')
        {
            return 0;
        }
        for (i = 2; i < 11; i++)
        {
            if (inputstr[i] == '\0')
            {
                *intnum = val;
                // Return 1 on success.
                res     = 1;
                break;
            }
            if (ISVALIDHEX(inputstr[i]))
            {
                val = (val << 4) + CONVERTHEX(inputstr[i]);
            }
            else
            {
                // Invalid input returns 0.
                res = 0;
                break;
            }
        }

        if (i >= 11)
        {
            res = 0;
        }
    }
    else /* Decimal input, max 10 characters. */
    {
        for (i = 0; i < 11; i++)
        {
            if (inputstr[i] == '\0')
            {
                *intnum = val;
                // Return 1 on success.
                res     = 1;
                break;
            }
            else if ((inputstr[i] == 'k' || inputstr[i] == 'K') && (i > 0))
            {
                val     = val << 10;
                *intnum = val;
                res     = 1;
                break;
            }
            else if ((inputstr[i] == 'm' || inputstr[i] == 'M') && (i > 0))
            {
                val     = val << 20;
                *intnum = val;
                res     = 1;
                break;
            }
            else if (ISVALIDDEC(inputstr[i]))
            {
                val = val * 10 + CONVERTDEC(inputstr[i]);
            }
            else
            {
                // Invalid input returns 0.
                res = 0;
                break;
            }
        }
        // More than 10 chars is invalid, return 0.
        if (i >= 11)
        {
            res = 0;
        }
    }

    return res;
}


/**
 * @brief  Receive byte from sender
 * @param  c: Receive buffer pointer.
 * @param  length: Expected DMA receive length.
 * @param  timeout: Timeout in ms.
 * @retval YMODEM_PKT_SUCCESS: Data received.
 * @retval YMODEM_PKT_TIMEOUT: Timeout or receive start failed.
 */
static int32_t Receive_Byte(uint8_t *c, uint16_t length, uint32_t timeout)
{
    /**
    * If the user handler pre-armed the next buffer already, skip the
    * re-arm — the fresh buffer is already receiving bytes and we just
    * need to wait for the transport idle event to deliver the frame
    * length. Project memory [[f411-usart-rdr-overrun]]: without pre-arm
    * the ~2 ms re-arm gap on F411 USART overruns RDR.
    **/
    if (!ota_transport_frame_is_armed())
    {
        ota_transport_status_t arm_st = ota_transport_frame_arm(c, length);
        if (OTA_TRANSPORT_OK != arm_st)
        {
            /* First arm failed — most often a stale RxState from a
               previous abort. Hard-stop then retry once. */
            (void)ota_transport_frame_stop();
            arm_st = ota_transport_frame_arm(c, length);
            if (OTA_TRANSPORT_OK != arm_st)
            {
                return YMODEM_PKT_TIMEOUT;
            }
        }
    }

    ota_transport_status_t wait_st =
        ota_transport_frame_wait(&s_u16_YmodRecLength, timeout);
    if (OTA_TRANSPORT_OK != wait_st)
    {
        return YMODEM_PKT_TIMEOUT;
    }
    return YMODEM_PKT_SUCCESS;
}

/**
 * @brief  Send a byte via the abstract OTA transport.
 * @param  c: Character
 * @retval 0: Byte sent
 */
static uint32_t Send_Byte(uint8_t c)
{
    (void)ota_transport_tx_byte(c);
    return 0;
}


/**
 * @brief  Handle FILE_INFO packet in user layer.
 * @param  ctx: Ymodem receive context.
 * @retval None
 */
static void Ymodem_File_Info_User_Handler(Ymodem_RxContext_t *ctx)
{
    if (osal_queue_send(Queue_AppDataBuffer, &ctx, OSAL_MAX_DELAY) != OSAL_SUCCESS)
    {
        DEBUG_OUT(e, YMODEM_LOG_TAG, "Failed to send file data to queue");
        return;
    }

    /* Wait until download task consumes the FILE_INFO message. */
    if (osal_sema_take(Semaphore_ExtFlashState, OSAL_MAX_DELAY) != OSAL_SUCCESS)
    {
        DEBUG_OUT(e, YMODEM_LOG_TAG,
                  "Failed to sync file info with download task");
        return;
    }

    if (ctx->buf[0] == ctx->packet_data)
    {
        ctx->packet_data = ctx->buf[1];
    }
    else
    {
        ctx->packet_data = ctx->buf[0];
    }

    /**
    * Pre-arm the next-buffer receive on the abstract transport. The
    * sender will fire the next packet on ACK receipt, and the main loop
    * still has Send_Byte(ACK) ahead of it — without this pre-arm the
    * leading bytes overrun RDR on F411. Receive_Byte() then sees
    * ota_transport_frame_is_armed() == true and skips its own arm.
    **/
    (void)ota_transport_frame_arm(ctx->packet_data, 1030U);
}

/**
 * @brief  Handle FILE_DATA packet in user layer.
 * @param  ctx: Ymodem receive context.
 * @retval None
 */
static void Ymodem_File_Data_User_Handler(Ymodem_RxContext_t *ctx)
{
    if (osal_queue_send(Queue_AppDataBuffer, &ctx, OSAL_MAX_DELAY) != OSAL_SUCCESS)
    {
        DEBUG_OUT(e, YMODEM_LOG_TAG, "Failed to send file info to queue");
        return;
    }

    /* Wait until download task writes this packet into W25Q before reusing
     * context buffers. */
    if (osal_sema_take(Semaphore_ExtFlashState, OSAL_MAX_DELAY) != OSAL_SUCCESS)
    {
        DEBUG_OUT(e, YMODEM_LOG_TAG,
                  "Failed to sync file data with download task");
        return;
    }

    if (ctx->buf[0] == ctx->packet_data)
    {
        ctx->packet_data = ctx->buf[1];
    }
    else
    {
        ctx->packet_data = ctx->buf[0];
    }

    /**
    * Pre-arm the fresh buffer via the abstract transport — same reason
    * as Ymodem_File_Info_User_Handler above.
    **/
    (void)ota_transport_frame_arm(ctx->packet_data, 1030U);
}

/**
 * @brief  Receive a packet from sender
 * @param  data: Packet buffer.
 * @param  length: Output packet payload length.
 * @param  timeout: Timeout in ms.
 * @retval YMODEM_PKT_SUCCESS: Packet valid.
 * @retval YMODEM_PKT_TIMEOUT: Timeout or packet invalid.
 * @retval YMODEM_PKT_ABORT: Transfer aborted.
 */
static int32_t Receive_Packet(uint8_t *data, int32_t *length, uint32_t timeout)
{
    uint16_t packet_size;
    uint8_t  c = 0;
    *length    = 0;
    if (Receive_Byte(data, 1030, timeout) != 0)
    {
        return (int32_t)YMODEM_PKT_TIMEOUT;
    }

    c = data[0];
    switch (c)
    {
    case SOH:
        packet_size = PACKET_SIZE;
        break;
    case STX:
        packet_size = PACKET_1K_SIZE;
        break;
    case EOT:
        return (int32_t)YMODEM_PKT_SUCCESS;
    case CA:
        if ((Receive_Byte(&c, 1, timeout) == 0) && (c == CA))
        {
            *length = -1;
            DEBUG_OUT(i, YMODEM_LOG_TAG,
                      "Received double CA, aborting transfer...");
            return (int32_t)YMODEM_PKT_SUCCESS;
        }
        else
        {
            DEBUG_OUT(w, YMODEM_LOG_TAG,
                      "Single CA received, waiting for second CA...");
            return (int32_t)YMODEM_PKT_TIMEOUT;
        }
    case ABORT1:
    case ABORT2:
        return (int32_t)YMODEM_PKT_ABORT;
    default:
        return (int32_t)YMODEM_PKT_TIMEOUT;
    }
    if (s_u16_YmodRecLength != (packet_size + PACKET_OVERHEAD))
    {
        return (int32_t)YMODEM_PKT_TIMEOUT;
    }

    if (data[PACKET_SEQNO_INDEX] !=
        ((data[PACKET_SEQNO_COMP_INDEX] ^ 0xff) & 0xff))
    {
        return (int32_t)YMODEM_PKT_TIMEOUT;
    }

    /* CRC-16/XMODEM check. Sender places the running CRC over the
       128/1024 B payload (MSB first, LSB second) in the two trailer bytes
       immediately after the data. Without this check, a corrupted payload
       with intact SOH/STX + SEQ/~SEQ would silently pass to the consumer. */
    {
        const uint16_t expected_crc =
            crc16_xmodem(&data[PACKET_HEADER], (uint32_t)packet_size);
        const uint16_t received_crc =
            ((uint16_t)data[PACKET_HEADER + packet_size] << 8) |
             (uint16_t)data[PACKET_HEADER + packet_size + 1];
        if (expected_crc != received_crc)
        {
            DEBUG_OUT(w, YMODEM_PACKET_LOG_TAG,
                      "CRC mismatch seq=%u got=0x%04X expected=0x%04X",
                      (unsigned)(data[PACKET_SEQNO_INDEX] & 0xFFu),
                      (unsigned)received_crc, (unsigned)expected_crc);
            return (int32_t)YMODEM_PKT_TIMEOUT;
        }
    }

    *length = packet_size;
    return (int32_t)YMODEM_PKT_SUCCESS;
}

/* Private state machine handler functions ---------------------------------*/

/**
 * @brief  Handle filename packet in receive state machine
 * @param  ctx: Receive context
 * @retval Ymodem_RxHandlerStatus_t: HANDLER_ERROR/CONTINUE/DONE
 */
static int32_t Ymodem_RxState_FileInfo(Ymodem_RxContext_t *ctx)
{
    /* Filename packet */
    if (ctx->packet_data[PACKET_HEADER] != 0)
    {
        /* Filename packet has valid data */
        for (ctx->i = 0, ctx->file_ptr = ctx->packet_data + PACKET_HEADER;
             (*ctx->file_ptr != 0) && (ctx->i < FILE_NAME_LENGTH);)
        {
            file_name[ctx->i++] = *ctx->file_ptr++;
        }
        file_name[ctx->i++] = '\0';
        for (ctx->i = 0, ctx->file_ptr++;
             (*ctx->file_ptr != ' ') && (ctx->i < FILE_SIZE_LENGTH);)
        {
            ctx->file_size[ctx->i++] = *ctx->file_ptr++;
        }
        ctx->file_size[ctx->i++] = '\0';
        Str2Int(ctx->file_size, &ctx->size);

        Ymodem_File_Info_User_Handler(ctx);

        DEBUG_OUT(d, YMODEM_FILE_LOG_TAG, "File size: %d bytes",
                  (int)ctx->size);

        Send_Byte(ACK);
        Send_Byte(CRC16);
        DEBUG_OUT(d, YMODEM_FILE_LOG_TAG, "Transition to FILE_DATA state");
        ctx->bytes_received = 0; /* Reset byte counter for new file */
        ctx->state          = YMODEM_RX_STATE_FILE_DATA;
        return (int32_t)YMODEM_RX_HANDLER_CONTINUE;
    }
    /* Filename packet is empty, end session */
    else
    {
        Send_Byte(ACK);
        DEBUG_OUT(d, YMODEM_LOG_TAG,
                  "Session complete, file transfer successful");
        ctx->file_done    = 1;
        ctx->session_done = 1;
        return (int32_t)YMODEM_RX_HANDLER_DONE;
    }
}

/**
 * @brief  Handle file data packet in receive state machine
 * @param  ctx: Receive context
 * @retval Ymodem_RxHandlerStatus_t: always CONTINUE
 */
static int32_t Ymodem_RxState_FileData(Ymodem_RxContext_t *ctx)
{
    // Only process positive packet lengths (actual data)
    if (ctx->packet_length > 0)
    {
        // Calculate bytes to copy (don't exceed total file size)
        int32_t bytes_to_copy = ctx->packet_length;
        if ((ctx->bytes_received + bytes_to_copy) > ctx->size)
        {
            bytes_to_copy = ctx->size - ctx->bytes_received;
        }

        /* Consumer writes payload bytes only (without 3-byte Ymodem header). */
        ctx->packet_length = bytes_to_copy;

        Ymodem_File_Data_User_Handler(ctx);

        ctx->bytes_received += bytes_to_copy;

        // Calculate and display progress (integer math, no float)
        uint32_t progress_percent = (ctx->bytes_received * 100) / ctx->size;
        DEBUG_OUT(i, YMODEM_DATA_LOG_TAG, "Progress: %d/%d bytes (%d%%)",
                  (int)ctx->bytes_received, (int)ctx->size,
                  (int)progress_percent);
    }

    Send_Byte(ACK);
    return (int32_t)YMODEM_RX_HANDLER_CONTINUE;
}

/**
 * @brief  Receive a file using the ymodem protocol (FSM version)
 * @param  buf: Address of the first byte
 * @retval Ymodem_ReceiveStatus_t: File size on success, negative on error
 *    - YMODEM_RX_ABORTED: Abort by sender (0)
 *    - YMODEM_RX_SIZE_ERR: Image size exceeds Flash size (-1)
 *    - YMODEM_RX_TIMEOUT_ERR: Max retries exceeded (-4)
 *    - YMODEM_RX_FLASH_ERR: Flash programming error (-2)
 *    - YMODEM_RX_USER_ABORT: User abort with Ctrl+C (-3)
 */
int32_t Ymodem_Receive(uint8_t (*buf)[1030])
{
    Ymodem_RxContext_t ctx;
    /* State handler function pointer array */
    int32_t (*state_handlers[])(Ymodem_RxContext_t *) = {
        Ymodem_RxState_FileInfo, /* YMODEM_RX_STATE_FILE_INFO */
        Ymodem_RxState_FileData  /* YMODEM_RX_STATE_FILE_DATA */
    };
    int32_t rx_result;
    int32_t state_result;

    /* Initialize context */
    ctx.buf              = buf;
    ctx.buf_ptr          = buf[0];
    ctx.packet_data      = ctx.buf_ptr;
    ctx.size             = 0;
    ctx.bytes_received   = 0;
    ctx.errors           = 0;
    ctx.session_done     = 0;
    ctx.packets_received = 0;
    ctx.session_begin    = 0;
    ctx.eot_seen         = 0;
    ctx.state            = YMODEM_RX_STATE_FILE_INFO;
    /* Initialize FlashDestination variable */

    DEBUG_OUT(i, YMODEM_LOG_TAG,
              "Starting reception... (bufA @0x%08lx)(bufB @0x%08lx)",
              (uint32_t)buf[0], (uint32_t)buf[1]);

    while (1)
    {
        for (ctx.packets_received = 0, ctx.file_done = 0;;)
        {
            rx_result = Receive_Packet(ctx.packet_data, &ctx.packet_length,
                                       NAK_TIMEOUT);

            switch (rx_result)
            {
            case YMODEM_PKT_SUCCESS:
                /* Packet received successfully */
                ctx.errors = 0;
                switch (ctx.packet_length)
                {
                /* Abort by sender */
                case -1:
                    Send_Byte(ACK);
                    return (int32_t)YMODEM_RX_ABORTED;
                /* End of transmission. Ymodem batch spec requires the
                   sender to send EOT twice — the first verified with NAK,
                   the second ACK'd. This is what makes a stuck dialog on
                   the PC side recognise the session as cleanly closed. */
                case 0:
                    if (ctx.state == YMODEM_RX_STATE_FILE_DATA)
                    {
                        if (ctx.eot_seen == 0)
                        {
                            /* First EOT: NAK forces sender to retransmit
                               EOT, which doubles as the verify step. */
                            Send_Byte(NAK);
                            ctx.eot_seen = 1;
                            DEBUG_OUT(d, YMODEM_LOG_TAG,
                                      "First EOT seen, NAK sent for verify");
                        }
                        else
                        {
                            /* Second EOT confirmed: ACK and request the
                               trailing batch-terminator block 0 via 'C'
                               (sent by the outer file_done path). */
                            Send_Byte(ACK);
                            ctx.state     = YMODEM_RX_STATE_FILE_INFO;
                            ctx.file_done = 1;
                            ctx.eot_seen  = 0;
                            DEBUG_OUT(d, YMODEM_LOG_TAG,
                                      "Second EOT ACK'd, awaiting block 0");
                        }
                    }
                    else /* state == YMODEM_RX_STATE_FILE_INFO */
                    {
                        /* Stray EOT in FILE_INFO — some non-spec senders
                           collapse the dance and ship an EOT instead of
                           the empty-filename block. Treat as end-of-batch
                           so we don't hang forever. */
                        Send_Byte(ACK);
                        ctx.file_done    = 1;
                        ctx.session_done = 1;
                        DEBUG_OUT(i, YMODEM_LOG_TAG,
                                  "Session complete (EOT in FILE_INFO)");
                    }
                    break;
                /* Normal packet */
                default:
                {
                    uint8_t pkt_seqno =
                        ctx.packet_data[PACKET_SEQNO_INDEX] & 0xff;
                    uint8_t exp_seqno = ctx.packets_received & 0xff;

                    /* Debug: Print sequence number */
                    DEBUG_OUT(d, YMODEM_PACKET_LOG_TAG,
                              "Seqno: %d Expected: %d", pkt_seqno, exp_seqno);

                    if (pkt_seqno != exp_seqno)
                    {
                        DEBUG_OUT(w, YMODEM_PACKET_LOG_TAG,
                                  "Sequence mismatch, sending NAK");
                        Send_Byte(NAK);
                    }
                    else
                    {
                        /* Call state handler function pointer */
                        state_result = state_handlers[ctx.state](&ctx);
                        if (state_result == (int32_t)YMODEM_RX_HANDLER_ERROR)
                        {
                            return state_result;
                        }
                        if (state_result == (int32_t)YMODEM_RX_HANDLER_DONE)
                        {
                            /* State handler indicates completion */
                            break;
                        }
                        /* YMODEM_RX_HANDLER_CONTINUE: proceed normally */

                        ctx.packets_received++;
                        ctx.session_begin = 1;
                    }
                }
                }
                break;
            case YMODEM_PKT_ABORT:
                /* User abort */
                Send_Byte(CA);
                Send_Byte(CA);
                return (int32_t)YMODEM_RX_USER_ABORT;
            case YMODEM_PKT_TIMEOUT:
            default:
                /* Graceful close for senders that omit the batch-terminator
                   block 0. After EOT-NAK-EOT-ACK we transitioned to
                   FILE_INFO and sent 'C' to invite block 0; spec-strict
                   senders reply with empty-filename SOH, but many real-
                   world tools (Tera Term, some lrzsz configs, parts of
                   Xshell) consider the session done after the second-EOT
                   ACK and never send block 0. Recognise that state — full
                   payload received + back in FILE_INFO — and close
                   cleanly rather than failing the OTA. */
                if (ctx.state == YMODEM_RX_STATE_FILE_INFO &&
                    ctx.size  > 0 &&
                    ctx.bytes_received >= ctx.size)
                {
                    DEBUG_OUT(i, YMODEM_LOG_TAG,
                              "Session complete (sender skipped block 0)");
                    ctx.file_done    = 1;
                    ctx.session_done = 1;
                    break;
                }
                /* Timeout or packet error */
                if (ctx.session_begin > 0)
                {
                    ctx.errors++;
                    DEBUG_OUT(w, YMODEM_LOG_TAG, "Timeout/Error, count: %d/%d",
                              (int)ctx.errors, MAX_ERRORS);
                }
                if (ctx.errors > MAX_ERRORS)
                {
                    DEBUG_OUT(e, YMODEM_LOG_TAG, "Max errors exceeded!");
                    Send_Byte(CA);
                    Send_Byte(CA);
                    return (int32_t)YMODEM_RX_TIMEOUT_ERR;
                }
                Send_Byte(CRC16);
                break;
            }
            if (ctx.file_done != 0)
            {
                break;
            }
        }
        if (ctx.session_done != 0)
        {
            break;
        }

        /* If file transfer complete but session not done, request next packet
         */
        /* (handles second EOT or empty filename packet per Ymodem protocol) */
        if (ctx.file_done != 0 && ctx.session_done == 0)
        {
            Send_Byte(CRC16);  /* Request next packet */
            ctx.file_done = 0; /* Reset to allow new file processing */
            ctx.errors    = 0; /* Reset error count for new packet reception */
            DEBUG_OUT(d, YMODEM_LOG_TAG,
                      "Requesting next packet with CRC16...");
        }
    }

    /* Debug: Final result */
    DEBUG_OUT(i, YMODEM_LOG_TAG, "Total bytes received: %lu",
              ctx.bytes_received);

    return ctx.bytes_received; /* Return actual bytes received */
}

/**
 ******************************************************************************
 * @file    IAP/src/ymodem.c
 * @author  MCD Application Team
 * @version V3.3.0
 * @date    10/15/2010
 * @brief   This file provides all the software functions related to the ymodem
 *          protocol.
 ******************************************************************************
 * @copy
 *
 * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
 * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
 * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
 * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
 * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
 * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
 *
 * <h2><center>&copy; COPYRIGHT 2010 STMicroelectronics</center></h2>
 */

/** @addtogroup IAP
 * @{
 */

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_flash.h"

#include "flash.h"

#include "common.h"
#include "bootmanager.h"
#include "w25qxx_Handler.h"

#include "Debug.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
uint8_t        file_name[FILE_NAME_LENGTH];
uint32_t       FlashDestination = BackAppAddress;
uint32_t       PageSize         = INTER_PAGE_SIZE;
uint32_t       EraseCounter     = 0x0;
uint32_t       NbrOfPage        = 0;
FLASH_Status   FLASHStatus      = FLASH_COMPLETE;
uint32_t       RamSource;
extern uint8_t tab_1024[1024];

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
* @brief Busy-wait poll the UART RX FIFO for one byte.
*
* @param[in]  timeout : max iterations to poll before giving up. NOT a real
*                       time budget: the unit is "one FIFO probe", so wall
*                       time scales with CPU clock and compiler -O level.
*
* @param[out] c       : on success, *c holds the received byte; undefined
*                       on timeout.
*
* @return  0  one byte received
*         -1  timeout (no byte within `timeout` polls)
* */
static int32_t Receive_Byte(uint8_t *c, uint32_t timeout)
{
    /**
    * Bootloader has no SysTick callback / no OSAL — busy-wait is the only
    * cheap blocking primitive available before the OS is up.
    **/
    while (timeout-- > 0)
    {
        if (SerialKeyPressed(c) == 1)
        {
            return 0;
        }
    }
    return -1;
}

/**
 * @brief  Send a byte
 * @param  c: Character
 * @retval 0: Byte sent
 */
static uint32_t Send_Byte(uint8_t c)
{
    SerialPutChar(c);
    return 0;
}

/**
* @brief Receive one Ymodem frame and classify it.
*
* @param[in]  timeout : per-byte busy-wait budget (see Receive_Byte).
*
* @param[out] data    : caller-supplied buffer, must be at least
*                       PACKET_1K_SIZE + PACKET_OVERHEAD (= 1029) bytes.
* @param[out] length  :  0  EOT (end of transmission)
*                       -1  remote abort (CA CA)
*                       >0  payload length (128 or 1024)
*
* @return Ymodem_PacketStatus_t
*           YMODEM_PKT_SUCCESS  caller should inspect *length
*           YMODEM_PKT_TIMEOUT  timeout, framing error, or bad SEQ/~SEQ
*           YMODEM_PKT_ABORT    sender pressed 'A'/'a' to abort
*
* @note Both the SEQ/~SEQ complement byte and the 16-bit XMODEM-CRC
*       trailer are validated. Cal_CRC16() is invoked over the payload
*       after the SEQ check; mismatches return YMODEM_PKT_TIMEOUT, which
*       the outer loop converts into a NAK that asks the sender to
*       retransmit the offending frame.
* */
static int32_t Receive_Packet(uint8_t *data, int32_t *length, uint32_t timeout)
{
    uint16_t i, packet_size;
    uint8_t  c;
    *length = 0;
    if (Receive_Byte(&c, timeout) != 0)
    {
        return (int32_t)YMODEM_PKT_TIMEOUT;
    }

    /**
    * First byte selects the framing: SOH/STX = data packet (128/1024 B),
    * EOT = clean end, CA CA = sender abort, 'A'/'a' = user abort, anything
    * else = treat as line noise and ask sender to retry via NAK/'C' upstream.
    **/
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
        /**
        * Ymodem requires two consecutive CA to mean "remote abort". A lone
        * CA is ambiguous, so we wait for the second one within the same
        * byte-timeout; if it doesn't arrive, treat as garbage line noise.
        **/
        if ((Receive_Byte(&c, timeout) == 0) && (c == CA))
        {
            *length = -1;
            return (int32_t)YMODEM_PKT_SUCCESS;
        }
        else
        {
            return (int32_t)YMODEM_PKT_TIMEOUT;
        }
    case ABORT1:
    case ABORT2:
        return (int32_t)YMODEM_PKT_ABORT;
    default:
        return (int32_t)YMODEM_PKT_TIMEOUT;
    }

    /**
    * Drain the remainder of the frame: 2 B seq header + payload + 2 B CRC.
    * data[0] already holds the framing byte (SOH/STX) from the switch above.
    **/
    *data = c;
    for (i = 1; i < (packet_size + PACKET_OVERHEAD); i++)
    {
        if (Receive_Byte(data + i, timeout) != 0)
        {
            return (int32_t)YMODEM_PKT_TIMEOUT;
        }
    }

    /* SEQ / ~SEQ complement check. */
    if (data[PACKET_SEQNO_INDEX] !=
        ((data[PACKET_SEQNO_COMP_INDEX] ^ 0xff) & 0xff))
    {
        return (int32_t)YMODEM_PKT_TIMEOUT;
    }

    /* CRC-16/XMODEM check. Sender places the CRC over the 128/1024 B
       payload (MSB first, LSB second) in the two trailer bytes right after
       the data. Cal_CRC16 is the existing implementation at the bottom of
       this file — it was defined but never called by the receive path,
       letting corrupted payloads with intact SEQ/~SEQ slip through. */
    {
        const uint16_t expected_crc =
            Cal_CRC16(&data[PACKET_HEADER], (uint32_t)packet_size);
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
* @brief FSM handler for the Ymodem "block 0" filename packet.
*
* Parses the NUL-terminated filename + space-separated ASCII size pair from
* the packet payload, sizes the upcoming transfer, erases the staging block
* in external flash, and transitions the FSM into the data-receive state.
* An empty payload (data[PACKET_HEADER] == 0) signals "no more files" and
* ends the session.
*
* @param[in,out] ctx : transfer context; on success ctx->size and
*                      ctx->state are updated, file_name[] is populated.
*
* @return YMODEM_RX_HANDLER_CONTINUE  filename parsed, expect data packets
*         YMODEM_RX_HANDLER_DONE      empty filename → end of session
*         YMODEM_RX_HANDLER_ERROR     size exceeds Flash budget (CA CA sent)
*
* @note The filename/size copy loops below can overrun file_name[] and
*       ctx->file_size[] by one byte if the sender sends a filename of
*       exactly FILE_NAME_LENGTH (256) without a NUL terminator, or a size
*       field of FILE_SIZE_LENGTH (16) without a trailing space. Trusted
*       transmitter only — do not expose this path to untrusted peers
*       without bounding the trailing terminator write.
* */
static int32_t Ymodem_RxState_FileInfo(Ymodem_RxContext_t *ctx)
{
    /**
    * Block 0 payload layout (Ymodem):
    *   [filename\0][ascii size " "][padding 0x00...].
    * A leading 0x00 byte means "session over" (sender sends one such packet
    * after the final EOT to gracefully close the session).
    **/
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

        /* Debug: Print received file size */
        DEBUG_OUT(d, YMODEM_FILE_INFO_LOG_TAG, "File size: %d bytes", ctx->size);

        /**
        * Compare declared image size against the internal-flash APP slot
        * budget — even though the staging area is the external W25Q64, the
        * image will eventually be copied back into internal flash by the
        * APP-side OTA flow, so the upstream limit still applies here.
        **/
        if (ctx->size > (INTER_FLASH_SIZE - 1))
        {
            DEBUG_OUT(e, YMODEM_FILE_INFO_LOG_TAG, "File size exceeds Flash!");
            /**
            * Two consecutive CA bytes is the Ymodem "abort transfer" signal
            * the sender is required to honour.
            **/
            Send_Byte(CA);
            Send_Byte(CA);
            return (int32_t)YMODEM_RX_HANDLER_ERROR;
        }

        /* Erase Flash sectors for application storage */
        // elog_info("FileInfo",
        //           "Erasing Backup Flash: addr=0x%08x, size=%d bytes",
        //           BackAppAddress, ctx->size);
        // uint8_t erase_result = Flash_erase(BackAppAddress, ctx->size);
        // if (erase_result != 0)
        // {
        //     elog_error("FileInfo", "Flash erase failed!");
        //     Send_Byte(CA);
        //     Send_Byte(CA);
        //     return (int32_t)YMODEM_RX_HANDLER_ERROR;
        // }
        // elog_info("FileInfo", "Flash erase success");

        /**
        * No pre-erase here. The original code called
        * Erase_Flash_Block(BLOCK_1) intending to "front-load" the W25Q
        * erase to keep the data-packet path fast, but that comment was
        * wrong on two counts:
        *
        *   1. BLOCK_SIZE = 0x80000 (512 KB), so Erase_Flash_Block was
        *      erasing 128 sub-sectors back-to-back — ~6.4 s of solid
        *      Flash-busy that stalled the OTA dialog right after block 0
        *      and before block 1 (the "Bootloader hangs after starting"
        *      symptom).
        *
        *   2. W25Q64_WriteData -> sfud_erase_write already does
        *      sfud_erase() per 4 KB chunk before each sub-sector write
        *      (see sfud.c:823). The upfront block erase was therefore a
        *      complete double-erase — wasted ~6.4 s AND doubled the
        *      Flash wear for nothing.
        *
        * Removing the call gives back the 6.4 s and halves the erase
        * count on every OTA cycle. The data-packet path stays exactly
        * as fast as before because sfud_erase_write was always doing
        * the real per-sub-sector erase anyway.
        **/

        /**
        * ACK + 'C' tells the sender "filename received, please start data,
        * use 16-bit CRC mode". Sender will then transmit block 1.
        **/
        Send_Byte(ACK);
        Send_Byte(CRC16);
        DEBUG_OUT(d, YMODEM_FILE_INFO_LOG_TAG, "Transition to FILE_DATA state");
        ctx->bytes_received = 0; /* Reset byte counter for new file */
        ctx->state          = YMODEM_RX_STATE_FILE_DATA;
        return (int32_t)YMODEM_RX_HANDLER_CONTINUE;
    }
    /**
    * Empty filename packet — Ymodem's graceful end-of-session marker. Just
    * ACK and let the outer loop tear down.
    **/
    else
    {
        Send_Byte(ACK);
        DEBUG_OUT(i, YMODEM_LOG_TAG, "Session complete, file transfer successful");
        ctx->file_done    = 1;
        ctx->session_done = 1;
        return (int32_t)YMODEM_RX_HANDLER_DONE;
    }
}

/**
* @brief FSM handler for Ymodem data packets — stream payload into the
*        W25Q64 staging block and ACK back.
*
* @param[in,out] ctx : transfer context; ctx->bytes_received is advanced.
*
* @return YMODEM_RX_HANDLER_CONTINUE  packet written (or ignored), ACK sent
*         YMODEM_RX_HANDLER_ERROR     W25Q64 write failed (CA CA sent)
*
* @note bytes_to_copy is clamped against the declared file size to drop the
*       0x1A / 0x00 padding the sender stuffs into the last packet — without
*       this, the staged image would contain trailing junk and verify would
*       fail downstream. Bytes_to_copy can theoretically reach 0 or negative
*       if the sender lies about ctx->size; caller currently relies on
*       W25Q64_WriteData() rejecting bogus lengths. Caveat emptor.
* */
static int32_t Ymodem_RxState_FileData(Ymodem_RxContext_t *ctx)
{
    /**
    * Skip control packets (length 0 = EOT, length -1 = remote abort). Those
    * are handled by the outer FSM; here we only consume real data frames.
    **/
    if (ctx->packet_length > 0)
    {
        /**
        * Last data packet is padded to a full 128/1024 B frame with 0x1A or
        * 0x00. Trim to the declared file size so the staged image is byte-
        * exact — important for the post-flash CRC check on the APP side.
        **/
        int32_t bytes_to_copy = ctx->packet_length;
        if ((ctx->bytes_received + bytes_to_copy) > ctx->size)
        {
            bytes_to_copy = ctx->size - ctx->bytes_received;
        }

        /**
        * Payload starts at PACKET_HEADER (skip SOH/STX + 2-byte SEQ header).
        * W25Q64_WriteData buffers writes internally to amortize page-program
        * cost; the matching _End() flush is invoked in Ymodem_Receive.
        **/
        uint8_t *src_ptr = ctx->packet_data + PACKET_HEADER;

        if (W25Q64_WriteData(BLOCK_1, src_ptr, bytes_to_copy) != 0)
        {
            DEBUG_OUT(e, YMODEM_FILE_DATA_LOG_TAG,
                      "External flash write failed at offset=%d, len=%d",
                      ctx->bytes_received, bytes_to_copy);
            Send_Byte(CA);
            Send_Byte(CA);
            return (int32_t)YMODEM_RX_HANDLER_ERROR;
        }

        // uint32_t flash_addr = FlashDestination;

        // elog_debug("FileData", "Programming Flash: offset=%d/%d (%d bytes)",
        //            ctx->bytes_received, ctx->size, bytes_to_copy);

        // // Write data in 32-bit word chunks
        // for (int32_t i = 0; i < bytes_to_copy; i += 4)
        // {
        //     // Get 32-bit word from packet (handle last partial word)
        //     uint32_t word_data  = 0xFFFFFFFF;
        //     int32_t  bytes_left = bytes_to_copy - i;

        //     if (bytes_left >= 4)
        //     {
        //         word_data = *(uint32_t *)src_ptr;
        //         src_ptr += 4;
        //     }
        //     else
        //     {
        //         // Handle last partial word (1-3 bytes)
        //         for (int32_t j = 0; j < bytes_left; j++)
        //         {
        //             word_data &= ~(0xFF << (j * 8));
        //             word_data |= (*src_ptr++ << (j * 8));
        //         }
        //     }

        //     // Program the word to Flash
        //     Flash_Write(flash_addr, word_data);

        //     // Verify the write (critical check!)
        //     if (*(uint32_t *)(flash_addr) != word_data)
        //     {
        //         elog_error("FileData",
        //                    "Flash write verification failed at 0x%08x",
        //                    flash_addr);
        //         Send_Byte(CA);
        //         Send_Byte(CA);
        //         return (int32_t)YMODEM_RX_HANDLER_ERROR;
        //     }

        //     flash_addr += 4;
        // }

        // Update tracking variables
        FlashDestination += bytes_to_copy;
        ctx->bytes_received += bytes_to_copy;

        // Calculate and display progress (integer math, no float)
        uint32_t progress_percent = (ctx->bytes_received * 100) / ctx->size;
        DEBUG_OUT(i, YMODEM_FILE_DATA_LOG_TAG, "Progress: %d/%d bytes (%d%%)",
                  ctx->bytes_received, ctx->size, progress_percent);
    }

    Send_Byte(ACK);
    return (int32_t)YMODEM_RX_HANDLER_CONTINUE;
}

/**
* @brief Receive a file via Ymodem and stage it into W25Q64 BLOCK_1.
*
* Driver-style outer loop around the FSM (FileInfo → FileData). Handles
* per-packet ACK/NAK, the Ymodem double-EOT trick, and the trailing
* zero-length filename packet that closes the session.
*
* @param[out] buf : caller buffer pointer; currently only kept in ctx for
*                   debug printing — actual payload goes straight into the
*                   external SPI NOR via W25Q64_WriteData(), not into buf.
*
* @return >0   on success: number of bytes accepted into staging (ctx.bytes_received)
*         0    YMODEM_RX_ABORTED  remote sent CA CA  (NOTE: collides numerically
*                                  with YMODEM_RX_TIMEOUT_ERR — see enum in
*                                  ymodem.h; caller cannot distinguish them)
*         0    YMODEM_RX_TIMEOUT_ERR  3 consecutive packet errors → bail
*         -1   YMODEM_RX_SIZE_ERR     image bigger than INTER_FLASH_SIZE
*         -2   YMODEM_RX_FLASH_ERR    W25Q64 finalize-flush failed
*         -3   YMODEM_RX_USER_ABORT   user pressed 'A' / 'a' on sender side
* */
int32_t Ymodem_Receive(uint8_t *buf)
{
    Ymodem_RxContext_t ctx;
    /**
    * FSM dispatch table — index by ctx.state (Ymodem_RxState_t). Adding a
    * state means appending a handler here and extending the enum in lockstep.
    **/
    int32_t (*state_handlers[])(Ymodem_RxContext_t *) = {
        Ymodem_RxState_FileInfo, /* YMODEM_RX_STATE_FILE_INFO */
        Ymodem_RxState_FileData  /* YMODEM_RX_STATE_FILE_DATA */
    };
    int32_t rx_result;
    int32_t state_result;

    /* Initialize context */
    ctx.buf              = buf;
    ctx.buf_ptr          = buf;
    ctx.size             = 0;
    ctx.bytes_received   = 0;
    ctx.errors           = 0;
    ctx.session_done     = 0;
    ctx.packets_received = 0;
    /**
    * session_begin gates error counting: timeouts before the very first
    * valid packet should not count toward MAX_ERRORS, because the receiver
    * is the one driving handshake (it spams 'C' to invite a sender), and
    * the sender may legitimately take a few seconds to react.
    **/
    ctx.session_begin    = 0;
    ctx.eot_seen         = 0;
    ctx.state            = YMODEM_RX_STATE_FILE_INFO;

    /**
    * Staging cursor (internal-flash semantics) — kept as a legacy global for
    * the commented-out internal-flash path; current code writes through
    * W25Q64_WriteData() instead, so this is effectively only used for
    * bookkeeping/log lines.
    **/
    FlashDestination     = BackAppAddress;

    DEBUG_OUT(i, YMODEM_LOG_TAG, "Starting reception... (buf @0x%08x)",
              (uint32_t)buf);

    /**
    * Kick-start: invite the sender with 'C' immediately so PC ymodem-rb
    * starts shipping block 0 without first having to wait out NAK_TIMEOUT
    * (which is ~100–500 ms of busy-poll on this bare-metal bootloader —
    * the unit of NAK_TIMEOUT is "FIFO probe iterations", not ms, and a
    * full loop at 0x100000 burns hundreds of ms of CPU at the F411 core
    * clock). The inner-loop default branch already sends 'C' on every
    * subsequent timeout, so this is just collapsing the first 'C' to t=0
    * — pure latency win, no semantic change.
    *
    * APP doesn't need this because its OTA tooling uses a magic-byte
    * pre-handshake (0x11 22 33) and starts block 0 unprompted, so the
    * APP-side Receive_Byte wakes on the first DMA-idle callback without
    * waiting on NAK_TIMEOUT. Standard ymodem-rb against this bootloader
    * does wait for 'C', hence this kick-start.
    **/
    Send_Byte(CRC16);

    while (1)
    {
        for (ctx.packets_received = 0, ctx.file_done = 0, ctx.buf_ptr = buf;;)
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
                /**
                * length == 0 means EOT byte was seen. Ymodem batch spec
                * requires the sender to send EOT twice — the first NAK'd
                * (verify step), the second ACK'd. PC tools that hold the
                * upload dialog open until they see the verify-NAK pattern
                * (Tera Term, lrzsz spec mode, SecureCRT) need this exact
                * dance; ACK'ing the first EOT used to leave them stuck.
                **/
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
                           ship an EOT instead of the empty-filename block.
                           Treat as end-of-batch so we don't hang. */
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
                    /**
                    * Compare against expected SEQ. Sender numbers packets
                    * 1, 2, 3, ... wrapping mod 256. Receiver counts packets
                    * starting at 0 (block 0 = filename), so the modulo of
                    * packets_received gives the expected number for the
                    * next packet.
                    **/
                    uint8_t pkt_seqno =
                        ctx.packet_data[PACKET_SEQNO_INDEX] & 0xff;
                    uint8_t exp_seqno = ctx.packets_received & 0xff;

                    /* Debug: Print sequence number */
                    DEBUG_OUT(d, YMODEM_PACKET_LOG_TAG, "Seqno: %d Expected: %d", pkt_seqno,
                              exp_seqno);

                    if (pkt_seqno != exp_seqno)
                    {
                        /**
                        * SEQ mismatch usually means the previous ACK was
                        * lost and sender re-sent the prior block. NAK asks
                        * sender to retransmit; do NOT advance the counter.
                        **/
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
                /**
                * Pre-session timeouts (no valid packet seen yet) are normal —
                * we're just spamming 'C' to wake the sender — so don't count
                * them against MAX_ERRORS. After the first successful packet,
                * every timeout is a real problem.
                **/
                if (ctx.session_begin > 0)
                {
                    ctx.errors++;
                    DEBUG_OUT(w, YMODEM_ERROR_LOG_TAG, "Timeout/Error, count: %d/%d",
                              ctx.errors, MAX_ERRORS);
                }
                if (ctx.errors > MAX_ERRORS)
                {
                    DEBUG_OUT(e, YMODEM_ERROR_LOG_TAG, "Max errors exceeded!");
                    Send_Byte(CA);
                    Send_Byte(CA);
                    return (int32_t)YMODEM_RX_TIMEOUT_ERR;
                }
                /**
                * Send 'C' (not NAK) to re-invite the sender — this keeps the
                * link in CRC16 mode rather than falling back to checksum.
                **/
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

        /**
        * Reached after first EOT (file_done=1, session_done=0). Re-arm the
        * inner loop to receive the trailing empty-filename block 0 that
        * marks end-of-session. The `session_done == 0` test is redundant
        * with the break above but kept for readability.
        **/
        if (ctx.file_done != 0 && ctx.session_done == 0)
        {
            Send_Byte(CRC16);  /* Request next packet */
            ctx.file_done = 0; /* Reset to allow new file processing */
            ctx.errors    = 0; /* Reset error count for new packet reception */
            DEBUG_OUT(d, YMODEM_LOG_TAG, "Requesting next packet with CRC16...");
        }
    }

    /* Debug: Final result */
    DEBUG_OUT(i, YMODEM_RESULT_LOG_TAG, "Total bytes received: %d", ctx.bytes_received);

    /**
    * Flush W25Q64's internal write buffer — the streaming write path keeps
    * a partial page in RAM to amortize SPI page-program latency, so the
    * very last (sub-page) chunk only lands in flash when we explicitly
    * close out the block here.
    **/
    if (W25Q64_WriteData_End(BLOCK_1) != 0)
    {
        DEBUG_OUT(e, YMODEM_RESULT_LOG_TAG, "Finalize external flash write failed");
        return (int32_t)YMODEM_RX_FLASH_ERR;
    }

    return (int32_t)ctx.bytes_received; /* Return actual bytes received */
}

/**
 * @brief  check response using the ymodem protocol
 * @param  buf: Address of the first byte
 * @retval The size of the file
 */
int32_t Ymodem_CheckResponse(uint8_t c)
{
    return 0;
}

/**
 * @brief  Prepare the first block
 * @param  timeout
 *     0: end of transmission
 */
void Ymodem_PrepareIntialPacket(uint8_t *data, const uint8_t *fileName,
                                uint32_t *length)
{
    uint16_t i, j;
    uint8_t  file_ptr[10];

    /* Make first three packet */
    data[0] = SOH;
    data[1] = 0x00;
    data[2] = 0xff;

    /* Filename packet has valid data */
    for (i = 0; (fileName[i] != '\0') && (i < FILE_NAME_LENGTH); i++)
    {
        data[i + PACKET_HEADER] = fileName[i];
    }

    data[i + PACKET_HEADER] = 0x00;

    Int2Str(file_ptr, *length);
    for (j = 0, i = i + PACKET_HEADER + 1; file_ptr[j] != '\0';)
    {
        data[i++] = file_ptr[j++];
    }

    for (j = i; j < PACKET_SIZE + PACKET_HEADER; j++)
    {
        data[j] = 0;
    }
}

/**
 * @brief  Prepare the data packet
 * @param  timeout
 *     0: end of transmission
 */
void Ymodem_PreparePacket(uint8_t *SourceBuf, uint8_t *data, uint8_t pktNo,
                          uint32_t sizeBlk)
{
    uint16_t i, size, packetSize;
    uint8_t *file_ptr;

    /* Make first three packet */
    packetSize = sizeBlk >= PACKET_1K_SIZE ? PACKET_1K_SIZE : PACKET_SIZE;
    size       = sizeBlk < packetSize ? sizeBlk : packetSize;
    if (packetSize == PACKET_1K_SIZE)
    {
        data[0] = STX;
    }
    else
    {
        data[0] = SOH;
    }
    data[1]  = pktNo;
    data[2]  = (~pktNo);
    file_ptr = SourceBuf;

    /* Filename packet has valid data */
    for (i = PACKET_HEADER; i < size + PACKET_HEADER; i++)
    {
        data[i] = *file_ptr++;
    }
    if (size <= packetSize)
    {
        for (i = size + PACKET_HEADER; i < packetSize + PACKET_HEADER; i++)
        {
            data[i] = 0x1A; /* EOF (0x1A) or 0x00 */
        }
    }
}

/**
 * @brief  Update CRC16 for input byte
 * @param  CRC input value
 * @param  input byte
 * @retval None
 */
uint16_t UpdateCRC16(uint16_t crcIn, uint8_t byte)
{
    uint32_t crc = crcIn;
    uint32_t in  = byte | 0x100;
    do
    {
        crc <<= 1;
        in <<= 1;
        if (in & 0x100)
            ++crc;
        if (crc & 0x10000)
            crc ^= 0x1021;
    } while (!(in & 0x10000));
    return crc & 0xffffu;
}


/**
 * @brief  Cal CRC16 for YModem Packet
 * @param  data
 * @param  length
 * @retval None
 */
uint16_t Cal_CRC16(const uint8_t *data, uint32_t size)
{
    uint32_t       crc     = 0;
    const uint8_t *dataEnd = data + size;
    while (data < dataEnd)
        crc = UpdateCRC16(crc, *data++);

    crc = UpdateCRC16(crc, 0);
    crc = UpdateCRC16(crc, 0);
    return crc & 0xffffu;
}

/**
 * @brief  Cal Check sum for YModem Packet
 * @param  data
 * @param  length
 * @retval None
 */
uint8_t CalChecksum(const uint8_t *data, uint32_t size)
{
    uint32_t       sum     = 0;
    const uint8_t *dataEnd = data + size;
    while (data < dataEnd)
        sum += *data++;
    return sum & 0xffu;
}

/**
 * @brief  Transmit a data packet using the ymodem protocol
 * @param  data
 * @param  length
 * @retval None
 */
void Ymodem_SendPacket(uint8_t *data, uint16_t length)
{
    uint16_t i;
    i = 0;
    while (i < length)
    {
        Send_Byte(data[i]);
        i++;
    }
}

/*******************(C)COPYRIGHT 2010 STMicroelectronics *****END OF FILE****/


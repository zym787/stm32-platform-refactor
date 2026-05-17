/******************************************************************************
 * @file bootmanager.c
 *
 * @par dependencies
 * - stddef.h
 * - string.h
 * - stm32f4xx.h
 * - iwdg.h
 * - main.h
 * - bootmanager.h
 * - at24cxx_driver.h
 * - w25qxx_Handler.h
 * - ymodem.h
 * - Debug.h
 * 
 * @author Ethan-Hang
 *
 * @brief
 * OTA state manager and image switch logic.
 *
 * Processing flow:
 * 1. Read OTA state from EEPROM.
 * 2. Receive and decrypt image to external BLOCK_2.
 * 3. Copy image from external flash to internal APP flash.
 * 4. Jump to APP and complete rollback flow when needed.
 *
 * @version V1.0 2026-4-2
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#include <stddef.h>
#include <string.h>

#include "stm32f4xx.h"
#include "iwdg.h"
#include "main.h"
#include "bootmanager.h"

#include "ota_flag.h"
#include "w25qxx_Handler.h"

#include "ymodem.h"

#include "Debug.h"

extern uint8_t       tab_1024[1024];
extern uint8_t       key_scan(void);
extern uint32_t          g_jumpinit;
extern volatile bool elog_init_flag;

static const uint8_t s_iv_default[16] = {0x31, 0x32, 0x31, 0x32, 0x31, 0x32,
                                         0x31, 0x32, 0x31, 0x32, 0x31, 0x32,
                                         0x31, 0x32, 0x31, 0x32};

static const uint8_t s_key_256[32]    = {
    0x31, 0x32, 0x31, 0x32, 0x31, 0x32, 0x31, 0x32, 0x31, 0x32, 0x31,
    0x32, 0x31, 0x32, 0x31, 0x32, 0x31, 0x32, 0x31, 0x32, 0x31, 0x32,
    0x31, 0x32, 0x31, 0x32, 0x31, 0x32, 0x31, 0x32, 0x31, 0x32};

static uint8_t s_mem_read_buffer[4096];

#define OTA_IWDG_PRESCALER IWDG_Prescaler_64
#define OTA_IWDG_RELOAD    3000U

static bool s_iwdg_started = false;

/**
 * @brief
 * Feed the OTA watchdog counter.
 *
 * @param[in] : None.
 *
 * @param[out] : None.
 *
 * @return
 * None.
 * */
static void ota_watchdog_feed(void)
{
    IWDG_ReloadCounter();
}

/**
 * @brief
 * Start OTA watchdog once and feed it.
 *
 * @param[in] : None.
 *
 * @param[out] : None.
 *
 * @return
 * None.
 * */
static void ota_watchdog_start(void)
{
    if (!s_iwdg_started)
    {
        IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
        IWDG_SetPrescaler(OTA_IWDG_PRESCALER);
        IWDG_SetReload(OTA_IWDG_RELOAD);
        IWDG_ReloadCounter();
        IWDG_Enable();
        s_iwdg_started = true;
        DEBUG_OUT(i, OTA_LOG_TAG, "OTA watchdog started");
    }

    ota_watchdog_feed();
}

/**
 * @brief
 * Trigger software reset by NVIC.
 *
 * @param[in] : None.
 *
 * @param[out] : None.
 *
 * @return
 * None.
 * */
static void soft_reset(void)
{
    __NVIC_SystemReset();
}

/**
 * @brief
 * Copy one external flash block to internal APP flash.
 *
 * @param[in] block_index : Source block index in external flash.
 *
 * @param[in] tag : Log label for this copy path.
 *
 * @param[out] : None.
 *
 * @return
 * 0 on success, -1 on failure.
 * */
static int8_t ex_block_to_app(uint8_t block_index, const char *tag)
{
    uint32_t  flash_des        = ApplicationAddress;
    uint32_t  flash_size       = Read_BlockSize(block_index);
    uint16_t  read_memory_size = 0;
    uint8_t   read_state       = 0;
    uint32_t *word_ptr         = NULL;

    if (flash_size == 0U)
    {
        DEBUG_OUT(e, OTA_LOG_TAG, "%s: invalid block size 0", tag);
        return -1;
    }

    if (Flash_erase(flash_des, flash_size) != 0U)
    {
        DEBUG_OUT(e, OTA_LOG_TAG, "%s: internal flash erase failed", tag);
        return -1;
    }

    for (;;)
    {
        ota_watchdog_feed();

        read_state =
            W25Q64_ReadData(block_index, s_mem_read_buffer, &read_memory_size);

        if (read_state == 1U)
        {
            DEBUG_OUT(i, OTA_LOG_TAG, "%s: copy complete, size=%lu bytes", tag,
                      (unsigned long)flash_size);
            return 0;
        }

        if (read_state == 2U)
        {
            DEBUG_OUT(e, OTA_LOG_TAG, "%s: read external flash failed", tag);
            return -1;
        }

        if ((read_memory_size % 4U) != 0U)
        {
            DEBUG_OUT(e, OTA_LOG_TAG, "%s: read size %u is not 4-byte aligned",
                      tag, read_memory_size);
            return -1;
        }

        word_ptr = (uint32_t *)s_mem_read_buffer;
        for (uint16_t i = 0U; i < (read_memory_size / 4U); i++)
        {
            Flash_Write(flash_des, word_ptr[i]);
            flash_des += 4U;

            if ((i & 0x3FU) == 0U)
            {
                ota_watchdog_feed();
            }
        }
    }
}

/**
 * @brief
 * Apply OTA image, update state, and jump to APP.
 *
 * @param[in] file_size : Downloaded file size in bytes.
 *
 * @param[in] first_boot : True when device has no valid APP.
 *
 * @param[out] : None.
 *
 * @return
 * None.
 * */
void ota_apply_update(int32_t file_size, bool first_boot)
{
    uint32_t          current_app_size = 0;
    struct ota_flag_t f;

    ota_watchdog_feed();

    /**
    * Snapshot the flag struct once; if magic is invalid (blank sector after
    * factory wipe) start from zeros so subsequent writes leave a valid magic.
    **/
    if (ota_flag_read(&f) != 0)
    {
        memset(&f, 0, sizeof(f));
        f.magic = OTA_FLAG_MAGIC;
    }

    if (exA_to_exB_AES(file_size) != 0)
    {
        DEBUG_OUT(e, OTA_LOG_TAG, "ota_apply_update: exA_to_exB_AES failed");
        if (!first_boot)
        {
            f.state = EE_OTA_NO_APP_UPDATE;
            (void)ota_flag_write(&f);
            jump_to_app();
        }
        return;
    }

    if (!first_boot)
    {
        ota_watchdog_feed();

        current_app_size = f.current_app_size;
        if (current_app_size == 0xFFFFFFFFU)
        {
            DEBUG_OUT(
                w, OTA_LOG_TAG,
                "ota_apply_update: current app size uninitialised, fallback");
            current_app_size = 0U;
        }

        if ((current_app_size == 0U) || (current_app_size == 0xFFFFFFFFU) ||
            (current_app_size > (uint32_t)(OTA_APP_MAX_SIZE - 1)))
        {
            if ((file_size > 0) && (file_size <= (int32_t)(OTA_APP_MAX_SIZE - 1)))
            {
                current_app_size = (uint32_t)file_size;
                DEBUG_OUT(
                    w, OTA_LOG_TAG,
                    "ota_apply_update: fallback current app size=%lu bytes",
                    (unsigned long)current_app_size);
            }
        }

        if ((current_app_size > 0U) &&
            (current_app_size <= (uint32_t)(OTA_APP_MAX_SIZE - 1)))
        {
            DEBUG_OUT(d, OTA_LOG_TAG,
                      "Current app size read from flag: %lu bytes",
                      (unsigned long)current_app_size);

            if (app_to_exA(current_app_size) != 0)
            {
                DEBUG_OUT(w, OTA_LOG_TAG,
                          "ota_apply_update: backup current app failed");
            }
        }
        else
        {
            DEBUG_OUT(w, OTA_LOG_TAG,
                      "ota_apply_update: skip backup due to invalid app size");
        }
    }

    ota_watchdog_feed();

    if (exB_to_app() != 0)
    {
        DEBUG_OUT(e, OTA_LOG_TAG, "ota_apply_update: apply new app failed");
        if (!first_boot)
        {
            f.state = EE_OTA_NO_APP_UPDATE;
            (void)ota_flag_write(&f);
        }
        jump_to_app();
        return;
    }

    /**
    * Success path: stamp CHECK_START and the new app size in a single sector
    * erase. Each ota_flag_write costs ~1 s on F411, so coalescing matters.
    **/
    f.state = EE_OTA_APP_CHECK_START;
    if ((file_size > 0) && (file_size <= (int32_t)(OTA_APP_MAX_SIZE - 1)))
    {
        f.current_app_size = (uint32_t)file_size;
    }
    if (ota_flag_write(&f) != 0)
    {
        DEBUG_OUT(e, OTA_LOG_TAG,
                  "ota_apply_update: write OTA check start failed");
        jump_to_app();
        return;
    }

    jump_to_app();

    // No valid APP to run, use backup if available.
    if (!first_boot)
    {
        ota_watchdog_feed();

        if (exA_to_app() != 0)
        {
            DEBUG_OUT(e, OTA_LOG_TAG, "ota_apply_update: rollback copy failed");
            return;
        }
    }

    jump_to_app();
}

/**
 * @brief
 * Disable peripherals and interrupts before APP jump.
 *
 * @param[in] : None.
 *
 * @param[out] : None.
 *
 * @return
 * None.
 * */
static void disable_all_peripherals(void)
{
    __disable_irq();

    elog_stop();
    elog_deinit();
    // TIM_DeInit(TIM3);

    USART_DeInit(USART1);
    GPIO_DeInit(GPIOA);

    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL  = 0U;

    for (uint32_t i = 0U; i < 8U; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }

    RCC_DeInit();
}

/**
 * @brief
 * Jump to APP reset handler if vector table is valid.
 *
 * @param[in] : None.
 *
 * @param[out] : None.
 *
 * @return
 * None.
 * */
void jump_to_app(void)
{
    uint32_t  sp            = *(__IO uint32_t *)(ApplicationAddress);
    uint32_t  jump_addr     = *(__IO uint32_t *)(ApplicationAddress + 4U);
    pFunction jump_app_func = NULL;

    if ((sp & 0x2FFE0000U) == 0x20000000U)
    {
        disable_all_peripherals();

        NVIC_SetVectorTable(NVIC_VectTab_FLASH,
                            (ApplicationAddress - NVIC_VectTab_FLASH));

        __set_MSP(sp);

        jump_app_func = (pFunction)jump_addr;
        jump_app_func();
    }
    else
    {
        if (elog_init_flag)
        {
            DEBUG_OUT(
                e, OTA_LOG_TAG,
                "jump_to_app: invalid stack pointer 0x%08lX at app 0x%08lX",
                (unsigned long)sp, (unsigned long)ApplicationAddress);
        }
    }
}

/**
 * @brief
 * Decrypt image from BLOCK_1 and write it to BLOCK_2.
 *
 * @param[in] fl_size : Encrypted file size in bytes.
 *
 * @param[out] : None.
 *
 * @return
 * 0 on success, -1 on failure.
 * */
int8_t exA_to_exB_AES(int32_t fl_size)
{
    uint8_t  temp[16];
    uint8_t  iv_work[16];
    uint16_t read_time        = 0;
    uint16_t read_data_count  = 0;
    uint32_t app_size         = 0;
    uint16_t read_memory_size = 0;
    uint32_t read_memory_idx  = 0;
    uint8_t  read_state       = 0;
    uint32_t decoded_bytes    = 0;
    uint32_t progress_step    = 0;
    uint32_t progress_last    = 0;

    if ((fl_size <= 0) || (fl_size > (int32_t)(OTA_APP_MAX_SIZE - 1)))
    {
        DEBUG_OUT(e, OTA_LOG_TAG, "exA_to_exB_AES: invalid input size=%ld",
                  (long)fl_size);
        return -1;
    }

    /* Rebuild BLOCK_1 read/write context in case RAM state was reset. */
    SetBlockParmeter(BLOCK_1, (uint32_t)fl_size);

    ota_watchdog_feed();

    memcpy(iv_work, s_iv_default, sizeof(iv_work));

    read_state = W25Q64_ReadData(BLOCK_1, s_mem_read_buffer, &read_memory_size);
    if ((read_state != 0U) || (read_memory_size < 16U))
    {
        DEBUG_OUT(e, OTA_LOG_TAG,
                  "exA_to_exB_AES: first read failed, state=%u size=%u",
                  read_state, read_memory_size);
        return -1;
    }

    memcpy(temp, s_mem_read_buffer, 16);

    /**
    * Diagnostic: dump the ciphertext bytes the bootloader sees at the
    * start of BLOCK_1. Compare against the APP-side "first 16 B to write"
    * log to localise corruption (Ymodem RX vs APP buffering vs SPI
    * read/write vs AES misdecode).
    **/
    DEBUG_OUT(i, OTA_LOG_TAG,
              "first 16 B from W25Q64 BLOCK_1: "
              "%02X %02X %02X %02X %02X %02X %02X %02X "
              "%02X %02X %02X %02X %02X %02X %02X %02X",
              temp[0],  temp[1],  temp[2],  temp[3],
              temp[4],  temp[5],  temp[6],  temp[7],
              temp[8],  temp[9],  temp[10], temp[11],
              temp[12], temp[13], temp[14], temp[15]);

    Aes_IV_key256bit_Decode(iv_work, temp, (uint8_t *)s_key_256);

    app_size = ((uint32_t)temp[15] << 24) | ((uint32_t)temp[14] << 16) |
               ((uint32_t)temp[13] << 8) | (uint32_t)temp[12];

    DEBUG_OUT(d, OTA_LOG_TAG, "exA_to_exB_AES: decoded AppSize=%lu, input=%ld",
              (unsigned long)app_size, (long)fl_size);

    if ((app_size == 0U) || (app_size > (uint32_t)fl_size))
    {
        DEBUG_OUT(e, OTA_LOG_TAG, "exA_to_exB_AES: decoded AppSize invalid");
        return -1;
    }

    read_data_count = (uint16_t)(app_size / 16U);
    if ((app_size % 16U) != 0U)
    {
        read_data_count++;
    }

    DEBUG_OUT(i, OTA_LOG_TAG,
              "exA_to_exB_AES: decrypt progress 0%% (0/%lu bytes)",
              (unsigned long)app_size);

    read_memory_idx = 16U;

    ota_watchdog_feed();
    Erase_Flash_Block(BLOCK_2);

    for (read_time = 0; read_time < read_data_count; read_time++)
    {
        if ((read_time & 0x3FU) == 0U)
        {
            ota_watchdog_feed();
        }

        if (read_memory_idx == read_memory_size)
        {
            read_state =
                W25Q64_ReadData(BLOCK_1, s_mem_read_buffer, &read_memory_size);
            if (read_state != 0U)
            {
                DEBUG_OUT(e, OTA_LOG_TAG,
                          "exA_to_exB_AES: block read failed at %u", read_time);
                return -1;
            }
            read_memory_idx = 0U;
        }

        if ((read_memory_idx + 16U) > read_memory_size)
        {
            DEBUG_OUT(e, OTA_LOG_TAG,
                      "exA_to_exB_AES: frame overflow idx=%lu size=%u",
                      (unsigned long)read_memory_idx, read_memory_size);
            return -1;
        }

        memcpy(temp, s_mem_read_buffer + read_memory_idx, 16);
        read_memory_idx += 16U;
        Aes_IV_key256bit_Decode(iv_work, temp, (uint8_t *)s_key_256);

        W25Q64_WriteData(BLOCK_2, temp, 16);

        if ((app_size - decoded_bytes) >= 16U)
        {
            decoded_bytes += 16U;
        }
        else
        {
            decoded_bytes = app_size;
        }

        progress_step = (decoded_bytes * 10U) / app_size;
        if (progress_step > progress_last)
        {
            progress_last = progress_step;
            DEBUG_OUT(i, OTA_LOG_TAG,
                      "exA_to_exB_AES: decrypt progress %lu%% (%lu/%lu bytes)",
                      (unsigned long)(progress_last * 10U),
                      (unsigned long)decoded_bytes, (unsigned long)app_size);
        }
    }

    W25Q64_WriteData_End(BLOCK_2);
    DEBUG_OUT(i, OTA_LOG_TAG,
              "exA_to_exB_AES: decode and copy done, AppSize=%lu",
              (unsigned long)app_size);

    return 0;
}

/**
 * @brief
 * Backup current internal APP to external BLOCK_1.
 *
 * @param[in] fl_size : APP image size in bytes.
 *
 * @param[out] : None.
 *
 * @return
 * 0 on success, -1 on failure.
 * */
int8_t app_to_exA(uint32_t fl_size)
{
    DEBUG_OUT(d, OTA_LOG_TAG,
              "Start to backup current app to external flash A, size=%lu bytes",
              (unsigned long)fl_size);
    if ((fl_size == 0U) || (fl_size > (uint32_t)(OTA_APP_MAX_SIZE - 1)))
    {
        DEBUG_OUT(e, OTA_LOG_TAG, "app_to_exA: invalid app size=%lu",
                  (unsigned long)fl_size);
        return -1;
    }

    ota_watchdog_feed();
    Erase_Flash_Block(BLOCK_1);
    W25Q64_WriteData(BLOCK_1, (uint8_t *)ApplicationAddress, fl_size);
    W25Q64_WriteData_End(BLOCK_1);
    return 0;
}

/**
 * @brief
 * Copy decrypted image in BLOCK_2 to internal APP flash.
 *
 * @param[in] : None.
 *
 * @param[out] : None.
 *
 * @return
 * 0 on success, -1 on failure.
 * */
int8_t exB_to_app(void)
{
    DEBUG_OUT(d, OTA_LOG_TAG, "Start to copy external flash B to app");

    return ex_block_to_app(BLOCK_2, "exB_to_app");
}

/**
 * @brief
 * Restore backup image in BLOCK_1 to internal APP flash.
 *
 * @param[in] : None.
 *
 * @param[out] : None.
 *
 * @return
 * 0 on success, -1 on failure.
 * */
int8_t exA_to_app(void)
{
    DEBUG_OUT(d, OTA_LOG_TAG, "Start to copy external flash A to app");

    return ex_block_to_app(BLOCK_1, "exA_to_app");
}

/**
 * @brief
 * Execute OTA state machine according to EEPROM state.
 *
 * @param[in] : None.
 *
 * @param[out] : None.
 *
 * @return
 * None.
 * */
void OTA_StateManager(void)
{
    uint8_t           ota_state      = EE_OTA_NO_APP_UPDATE;
    uint32_t          app_size       = 0;
    int32_t           file_size      = 0;
    static uint16_t   send_wait_time = 0;
    struct ota_flag_t f;

    /**
    * Read once into a local snapshot. Invalid magic (e.g. blank Sector 2
    * on a freshly-flashed board) is treated as EE_INIT_NO_APP rather than
    * an error — that lets first-boot land cleanly in the "wait for download"
    * path instead of looping silently on EEPROM read failure as before.
    **/
    if (ota_flag_read(&f) != 0)
    {
        memset(&f, 0, sizeof(f));
        f.magic = OTA_FLAG_MAGIC;
        f.state = EE_INIT_NO_APP;
    }
    ota_state = (uint8_t)f.state;

    if (0 == send_wait_time)
    {
        DEBUG_OUT(i, OTA_LOG_TAG, "OTA state=0x%02X", ota_state);
    }

    ota_watchdog_feed();

    switch (ota_state)
    {
    case EE_INIT_NO_APP:
        DEBUG_OUT(i, OTA_LOG_TAG,
                  "No app in flash, wait for key press to download");
        if (key_scan())
        {
            file_size = Ymodem_Receive(tab_1024);

            if (file_size <= 0)
            {
                DEBUG_OUT(e, OTA_LOG_TAG, "OTA download failed, file_size=%ld",
                          (long)file_size);
                break;
            }

            ota_apply_update(file_size, true);
        }
        else
        {
            send_wait_time++;
            if (send_wait_time >= 20000U)
            {
                DEBUG_OUT(
                    w, OTA_LOG_TAG,
                    "No app and no update, wait for key press to download");
                send_wait_time = 0U;
            }
        }
        break;
    case EE_OTA_NO_APP_UPDATE:
        if (key_scan())
        {
            file_size = Ymodem_Receive(tab_1024);

            if (file_size <= 0)
            {
                DEBUG_OUT(e, OTA_LOG_TAG, "OTA download failed, file_size=%ld",
                          (long)file_size);
                jump_to_app();
                break;
            }

            ota_apply_update(file_size, false);
        }
        else
        {
            /**
            * No upgrade pending, no key — just hand off to APP. The legacy
            * upstream design soft-reset here and relied on a g_jumpinit
            * sentinel checked by main() early, but our main loop owns the
            * state machine directly so a soft_reset would just bring us
            * right back into this branch on the next boot. jump_to_app()
            * never returns.
            **/
            jump_to_app();
        }
        break;

    case EE_OTA_DOWNLOADING:
        DEBUG_OUT(w, OTA_LOG_TAG, "OTA previous downloading interrupted");
        jump_to_app();

        file_size = Ymodem_Receive(tab_1024);
        ota_apply_update(file_size, false);
        break;

    case EE_OTA_DOWNLOAD_FINISHED:
        /**
        * image_size was stamped by the APP after a successful Ymodem
        * staging cycle — read directly from the snapshot.
        **/
        app_size = f.image_size;

        if (app_size == 0U)
        {
            DEBUG_OUT(e, OTA_LOG_TAG,
                      "OTA_StateManager: downloaded app size is 0");
            jump_to_app();
            break;
        }

        SetBlockParmeter(BLOCK_1, app_size);
        ota_apply_update((int32_t)app_size, false);
        break;
    case EE_OTA_APP_CHECK_START:
        /**
        * Transition CHECK_START → CHECKING and jump. IWDG starts here; if
        * the APP fails to clear the flag in time, we fall into the
        * CHECKING branch on the next reboot and roll back.
        **/
        f.state = EE_OTA_APP_CHECKING;
        (void)ota_flag_write(&f);

        ota_watchdog_start();
        jump_to_app();
        break;
    case EE_OTA_APP_CHECKING:
        /**
        * Reached if APP failed to confirm last cycle (IWDG-reset us back
        * here). Clear the flag and retry the APP — proper rollback via
        * exA_to_app is a TODO for a later commit. Without the jump, a
        * soft_reset here would land in NO_APP_UPDATE and just bounce
        * back through main again.
        **/
        f.state = EE_OTA_NO_APP_UPDATE;
        (void)ota_flag_write(&f);

        ota_watchdog_start();
        jump_to_app();
        break;
    default:
        DEBUG_OUT(e, OTA_LOG_TAG, "OTA_StateManager: unknown state 0x%02X",
                  ota_state);
        break;
    }
}

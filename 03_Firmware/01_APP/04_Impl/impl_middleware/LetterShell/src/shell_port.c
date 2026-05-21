/**
 * @file shell_port.c
 * @author Letter (NevermindZZT@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2019-02-22
 * 
 * @copyright (c) 2019 Letter
 * 
 */

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "shell.h"
// #include "serial.h"
#include "stm32f4xx_hal.h"
#include "usart.h"
// #include "cevent.h"
#include "log.h"
#include "SEGGER_RTT.h"
#include "bsp_wrapper_temp_humi.h"

Shell shell;
char shellBuffer[512];

static SemaphoreHandle_t shellMutex;

/**
 * @brief 用户shell写
 * 
 * @param data 数据
 * @param len 数据长度
 * 
 * @return short 实际写入的数据长度
 */
short userShellWrite(char *data, unsigned short len)
{
    // serialTransmit(&debugSerial, (uint8_t *)data, len, 0x1FF);
	  HAL_UART_Transmit(&huart1, (uint8_t *)data, len, 0xFFFFFFFF);
        // SEGGER_RTT_WriteDownBuffer(0, data, len);
    return len;
}


/**
 * @brief 用户shell读
 * 
 * @param data 数据
 * @param len 数据长度
 * 
 * @return short 实际读取到
 */
short userShellRead(char *data, unsigned short len)
{
	  // return serialReceive(&debugSerial, (uint8_t *)data, len, 0);
	  if(HAL_UART_Receive(&huart1, (uint8_t *)data, len, 0xFFFF) != HAL_OK)
        // if(SEGGER_RTT_ReadUpBuffer(0, data, len) == 0)
		{
				return 0;
		}else{
				return 1;
		}  
}

/**
 * @brief 用户shell上锁
 * 
 * @param shell shell
 * 
 * @return int 0
 */
int userShellLock(Shell *shell)
{
    xSemaphoreTakeRecursive(shellMutex, portMAX_DELAY);
    return 0;
}

/**
 * @brief 用户shell解锁
 * 
 * @param shell shell
 * 
 * @return int 0
 */
int userShellUnlock(Shell *shell)
{
    xSemaphoreGiveRecursive(shellMutex);
    return 0;
}

/**
 * @brief 用户shell初始化
 * 
 */
void userShellInit(void)
{
    shellMutex = xSemaphoreCreateMutex();

    shell.write = userShellWrite;
    shell.read = userShellRead;
    shell.lock = userShellLock;
    shell.unlock = userShellUnlock;
    shellInit(&shell, shellBuffer, 512);
//    if (xTaskCreate(shellTask, "shell", 256, &shell, 5, NULL) != pdPASS)
//    {
//        logError("shell task creat failed");
//    }
}
// CEVENT_EXPORT(EVENT_INIT_STAGE2, userShellInit);

void shellTest(int a, int b, int c)
{
	shellPrint(&shell, "This is test\r\n");
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC),
					test, shellTest, This is test);

/* ---- AHT21 shell command ---- */

/**
 * @brief shell AHT21回调：将温湿度结果打印到shell
 */
static void shell_aht21_callback(float *temperature, float *humidity)
{
    shellPrint(&shell, "AHT21: temperature = %.2f C, humidity = %.2f %%\r\n",
               *temperature, *humidity);
}

/**
 * @brief shell命令函数：触发一次AHT21温湿度读取
 *        用法: aht21_read
 */
void shell_aht21_read(void)
{
    temp_humi_read_all_async(shell_aht21_callback, 500);
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC),
                 aht21_read, shell_aht21_read, Read AHT21 temperature and humidity);


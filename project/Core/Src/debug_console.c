#include "debug_console.h"
#include "usart.h"
#include <string.h>

/* redirect printf to USART1 */
int _write(int file, char *ptr, int len)
{
    (void)file;
    HAL_UART_Transmit(&huart1, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
}

void DebugConsole_Init(void)
{
    LogSensorHeader();
}

void LogSensorHeader(void)
{
    printf("\r\n========================================\r\n");
    printf("    Smart Hood Sensor Monitor\r\n");
    printf("========================================\r\n");
    printf("Temp(C)  Hum(%%)   VOC(raw) VOC_idx PM2.5  PM10   Cur(A)  Power(W)\r\n");
    printf("-------- -------- -------- ------- ------ ------ ------- --------\r\n");
}

void LogSensorData(float temp, float hum, uint16_t voc_raw, uint16_t voc_idx,
                   uint16_t pm2_5, uint16_t pm10, float current, float power)
{
    printf("%7.1f  %7.1f  %7u  %6u  %5u  %5u  %6.2f  %7.1f\r\n",
           temp, hum, voc_raw, voc_idx, pm2_5, pm10, current, power);
}

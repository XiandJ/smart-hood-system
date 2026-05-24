#include "app/comm_task.h"
#include "usart.h"

/*
 * CommTask — OpenMV communication framework
 *
 * Protocol: command byte + optional payload via USART1 (921600bps)
 * Currently a stub; extend with actual OpenMV protocol when ready.
 */

#define COMM_RX_BUF_SIZE  16

static uint8_t rx_buf[COMM_RX_BUF_SIZE];

void CommTask_Init(void) {
    /* Start DMA receive for OpenMV commands (placeholder) */
    /* HAL_UART_Receive_DMA(&huart1, rx_buf, COMM_RX_BUF_SIZE); */
}

void CommTask_Update(void) {
    /* Process incoming OpenMV commands here */
    /* e.g. parse rx_buf, set motor speed or query sensor data */
}

#include "drivers/pms7003.h"
#include "config.h"
#include "usart.h"

#define PMS_HEADER1      0x42
#define PMS_HEADER2      0x4D
#define PMS_DMA_BUF_SIZE 64

typedef enum {
    PMS_STATE_IDLE,
    PMS_STATE_HDR1,
    PMS_STATE_DATA
} pms_state_t;

static uint8_t  rx_dma_buf[PMS_DMA_BUF_SIZE];
static uint8_t  rx_frame[PMS7003_FRAME_LEN];
static volatile uint8_t  rx_pos;
static volatile pms_state_t state = PMS_STATE_IDLE;
static volatile uint16_t frame_len;

static pms7003_data_t current_data;
static volatile uint8_t  data_ready = 0;

static uint16_t pms_make_word(uint8_t hi, uint8_t lo) {
    return ((uint16_t)hi << 8) | lo;
}

static void pms_parse_byte(uint8_t byte) {
    switch (state) {
    case PMS_STATE_IDLE:
        if (byte == PMS_HEADER1) {
            rx_frame[0] = byte;
            rx_pos = 1;
            state = PMS_STATE_HDR1;
        }
        break;
    case PMS_STATE_HDR1:
        if (byte == PMS_HEADER2) {
            rx_frame[1] = byte;
            rx_pos = 2;
            state = PMS_STATE_DATA;
        } else {
            state = PMS_STATE_IDLE;
        }
        break;
    case PMS_STATE_DATA:
        rx_frame[rx_pos++] = byte;
        if (rx_pos >= 4) {
            frame_len = pms_make_word(rx_frame[2], rx_frame[3]);
        }
        if (rx_pos >= 4 + frame_len + 2) {
            /* full frame received, verify checksum */
            uint16_t sum = 0;
            for (uint16_t i = 0; i < rx_pos - 2; i++) {
                sum += rx_frame[i];
            }
            uint16_t chk = pms_make_word(rx_frame[rx_pos - 2], rx_frame[rx_pos - 1]);
            if (sum == chk && frame_len >= 28) {
                current_data.pm1_0_cf1  = pms_make_word(rx_frame[4],  rx_frame[5]);
                current_data.pm2_5_cf1  = pms_make_word(rx_frame[6],  rx_frame[7]);
                current_data.pm10_cf1   = pms_make_word(rx_frame[8],  rx_frame[9]);
                current_data.pm1_0_atm  = pms_make_word(rx_frame[10], rx_frame[11]);
                current_data.pm2_5_atm  = pms_make_word(rx_frame[12], rx_frame[13]);
                current_data.pm10_atm   = pms_make_word(rx_frame[14], rx_frame[15]);
                current_data.cnt_0_3um  = pms_make_word(rx_frame[16], rx_frame[17]);
                current_data.cnt_0_5um  = pms_make_word(rx_frame[18], rx_frame[19]);
                current_data.cnt_1_0um  = pms_make_word(rx_frame[20], rx_frame[21]);
                current_data.cnt_2_5um  = pms_make_word(rx_frame[22], rx_frame[23]);
                current_data.cnt_5_0um  = pms_make_word(rx_frame[24], rx_frame[25]);
                current_data.cnt_10_0um = pms_make_word(rx_frame[26], rx_frame[27]);
                current_data.version     = rx_frame[28];
                current_data.error_code  = rx_frame[29];
                current_data.checksum_ok = 1;
                data_ready = 1;
            }
            state = PMS_STATE_IDLE;
        }
        break;
    }
}

/* HAL UART Rx Event Callback - triggered by IDLE interrupt on STM32H5 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
    if (huart != &huart3) return;
    /* feed each received byte to the frame parser */
    for (uint16_t i = 0; i < Size; i++) {
        pms_parse_byte(rx_dma_buf[i]);
    }
    /* restart DMA reception */
    HAL_UARTEx_ReceiveToIdle_DMA(&huart3, rx_dma_buf, PMS_DMA_BUF_SIZE);
}

int8_t PMS7003_Init(void) {
    state = PMS_STATE_IDLE;
    rx_pos = 0;
    data_ready = 0;
    HAL_UARTEx_ReceiveToIdle_DMA(&huart3, rx_dma_buf, PMS_DMA_BUF_SIZE);
    return 0;
}

int8_t PMS7003_GetData(pms7003_data_t *data) {
    if (!data_ready) return -1;
    __disable_irq();
    *data = current_data;
    data_ready = 0;
    __enable_irq();
    return 0;
}

uint8_t PMS7003_IsDataReady(void) {
    return data_ready;
}

#include "main.h"
#include "sensor_hub.h"
#include "ai_engine.h"
#include "motor_control.h"
#include "openmv_bridge.h"
#include "system_manager.h"

/* HAL句柄 */
I2C_HandleTypeDef   hi2c1;
UART_HandleTypeDef  huart1;   /* OpenMV */
UART_HandleTypeDef  huart2;   /* Debug / ESP32 */
UART_HandleTypeDef  huart3;   /* PMS7003 */
ADC_HandleTypeDef   hadc1;
TIM_HandleTypeDef   htim1;
IWDG_HandleTypeDef  hiwdg;

/* 全局系统上下文 */
SystemContext_t   g_sys;
DecisionContext_t g_decision;
MotorController_t g_motor;

/* 看门狗刷新计数器 */
static volatile uint32_t iwdg_reload_flag = 0;

int main(void)
{
    /* HAL初始化 */
    HAL_Init();
    SystemClock_Config();

    /* 外设初始化 */
    GPIO_Init();
    I2C1_Init();
    UART1_Init();
    UART3_Init();
    ADC1_Init();
    TIM1_PWM_Init();
    IWDG_Init();

    /* 使能DWT周期计数器 (用于AI推理计时) */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    /* 系统管理器初始化 (包含所有子系统) */
    SystemManager_Init();

    /* 通知OpenMV开始工作 */
    OpenMV_SendCommand(OMV_CMD_HEARTBEAT, NULL, 0);

    /* ============================================
     * 主循环
     * 架构: 协作式调度, 无RTOS
     * 周期: 系统管理 10ms, 传感器/AI 1s
     * ============================================ */
    while (1)
    {
        /* 喂狗 */
        HAL_IWDG_Refresh(&hiwdg);

        /* 系统管理任务 (10ms周期, 内部包含状态机、传感器采样、AI推理) */
        SystemManager_Run();

        /* 电机斜坡更新 (在SystemManager_Run中已调用Motor_Update) */

        /* PMS7003 UART超时检测 */
        if (HAL_GetTick() - g_pms7003.last_rx_tick > 3000) {
            g_pms7003.state = PMS_PARSE_IDLE;
            g_pms7003.idx = 0;
        }

        /* OpenMV心跳检测 */
        if (HAL_GetTick() - g_openmv.last_rx_tick > 3000) {
            g_openmv.connected = 0;
        }

        /* 低功耗: 待机状态可适当延长循环间隔 */
        if (g_sys.state == SYS_STANDBY || g_sys.state == SYS_OFF) {
            HAL_Delay(50); /* 待机50ms周期 */
        }
    }
}

/* ============================================================
 * 系统时钟配置
 * HSE 25MHz → PLL → 250MHz SYSCLK
 * ============================================================ */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM       = 5;
    RCC_OscInitStruct.PLL.PLLN       = 100;
    RCC_OscInitStruct.PLL.PLLP       = 2;
    RCC_OscInitStruct.PLL.PLLQ       = 2;
    RCC_OscInitStruct.PLL.PLLR       = 2;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
        Error_Handler();

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
                                     | RCC_CLOCKTYPE_PCLK3;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
        Error_Handler();
}

/* ============================================================
 * GPIO初始化
 * ============================================================ */
void GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* PA0: TIM2_CH1 PWM (自动配置为AF) */
    /* PA1: ADC1_IN1 (自动配置为模拟) */
    /* PA2: Motor Enable */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin   = GPIO_PIN_2;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PA3/4/5: 按键输入 */
    GPIO_InitStruct.Pin  = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PA6: WS2812B Data */
    GPIO_InitStruct.Pin  = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PC13: 板载LED */
    GPIO_InitStruct.Pin  = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

/* ============================================================
 * I2C1初始化 (SHT40 + SGP40 + SSD1306)
 * ============================================================ */
void I2C1_Init(void)
{
    hi2c1.Instance             = I2C1;
    hi2c1.Init.Timing          = 0x10707DBC; /* 400kHz @250MHz SYSCLK */
    hi2c1.Init.OwnAddress1     = 0;
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_ENABLE;

    if (HAL_I2C_Init(&hi2c1) != HAL_OK)
        Error_Handler();

    HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE);
}

/* ============================================================
 * UART1初始化 (OpenMV通信, 921600bps)
 * ============================================================ */
void UART1_Init(void)
{
    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = OPENMV_UART_BAUDRATE;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart1) != HAL_OK)
        Error_Handler();
}

/* ============================================================
 * UART3初始化 (PMS7003, 9600bps)
 * ============================================================ */
void UART3_Init(void)
{
    huart3.Instance          = USART3;
    huart3.Init.BaudRate     = PMS7003_BAUDRATE;
    huart3.Init.WordLength   = UART_WORDLENGTH_8B;
    huart3.Init.StopBits     = UART_STOPBITS_1;
    huart3.Init.Parity       = UART_PARITY_NONE;
    huart3.Init.Mode         = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart3) != HAL_OK)
        Error_Handler();
}

/* ============================================================
 * ADC1初始化 (霍尔电流传感器)
 * ============================================================ */
void ADC1_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    hadc1.Instance                   = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_ASYNC_DIV2;
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.GainCompensation      = 0;
    hadc1.Init.ScanConvMode          = ADC_SCAN_DISABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    hadc1.Init.LowPowerAutoWait      = DISABLE;
    hadc1.Init.ContinuousConvMode    = DISABLE;
    hadc1.Init.NbrOfConversion       = 1;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
    hadc1.Init.Overrun               = ADC_OVR_DATA_OVERWRITTEN;

    if (HAL_ADC_Init(&hadc1) != HAL_OK)
        Error_Handler();

    sConfig.Channel      = ADC_CHANNEL_1;
    sConfig.Rank         = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_24CYCLES_5;

    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
        Error_Handler();
}

/* ============================================================
 * TIM1 PWM初始化 (电机驱动, 20kHz)
 * ============================================================ */
void TIM1_PWM_Init(void)
{
    TIM_OC_InitTypeDef sConfigOC = {0};

    htim1.Instance               = TIM1;
    htim1.Init.Prescaler         = 0;
    htim1.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim1.Init.Period            = MOTOR_PWM_MAX_DUTY - 1; /* 1000 ticks */
    htim1.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
        Error_Handler();

    sConfigOC.OCMode     = TIM_OCMODE_PWM1;
    sConfigOC.Pulse      = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_ENABLE;

    if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
        Error_Handler();
}

/* ============================================================
 * IWDG独立看门狗
 * ============================================================ */
void IWDG_Init(void)
{
    hiwdg.Instance       = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_64;
    hiwdg.Init.Window    = 4095;
    hiwdg.Init.Reload    = 4095; /* ~4s timeout @32kHz LSI */

    if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
        Error_Handler();
}

/* ============================================================
 * UART RX中断处理
 * ============================================================ */
void USART1_IRQHandler(void)
{
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE)) {
        uint8_t byte = (uint8_t)(huart1.Instance->RDR & 0xFF);
        OpenMV_UART_RxCallback(byte);
    }
}

void USART3_IRQHandler(void)
{
    if (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_RXNE)) {
        uint8_t byte = (uint8_t)(huart3.Instance->RDR & 0xFF);
        PMS7003_UART_IRQHandler(byte);
    }
}

/* ============================================================
 * 错误处理
 * ============================================================ */
void Error_Handler(void)
{
    __disable_irq();
    while (1) {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        for (volatile uint32_t i = 0; i < 1000000; i++);
    }
}

/* ============================================================
 * HardFault处理
 * ============================================================ */
void HardFault_Handler(void)
{
    __disable_irq();
    /* 紧急停止电机 */
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);

    while (1) {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        for (volatile uint32_t i = 0; i < 500000; i++);
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t* file, uint32_t line)
{
    (void)file;
    (void)line;
    __disable_irq();
    while (1);
}
#endif

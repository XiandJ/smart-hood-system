/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.c
  * Description        : FreeRTOS applicative file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app_freertos.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app/sensor_hub.h"
#include "app/system_manager.h"
#include "app/ai_engine.h"
#include "app/comm_task.h"
#include "drivers/motor.h"
#include "debug_console.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
static sensor_data_t g_sensor_data;
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 128 * 4
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void SensorTask(void *argument);
void MotorTask(void *argument);
void SystemTask(void *argument);
void AiTask(void *argument);
void CommTask(void *argument);
/* USER CODE END FunctionPrototypes */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  osThreadId_t sensorTaskHandle;
  const osThreadAttr_t sensorTask_attr = {
    .name = "SensorTask",
    .priority = osPriorityHigh,
    .stack_size = 512 * 4
  };
  sensorTaskHandle = osThreadNew(SensorTask, NULL, &sensorTask_attr);

  osThreadId_t motorTaskHandle;
  const osThreadAttr_t motorTask_attr = {
    .name = "MotorTask",
    .priority = osPriorityAboveNormal,
    .stack_size = 256 * 4
  };
  motorTaskHandle = osThreadNew(MotorTask, NULL, &motorTask_attr);

  osThreadId_t systemTaskHandle;
  const osThreadAttr_t systemTask_attr = {
    .name = "SystemTask",
    .priority = osPriorityLow,
    .stack_size = 256 * 4
  };
  systemTaskHandle = osThreadNew(SystemTask, NULL, &systemTask_attr);

  osThreadId_t aiTaskHandle;
  const osThreadAttr_t aiTask_attr = {
    .name = "AiTask",
    .priority = osPriorityAboveNormal,
    .stack_size = 2048 * 4
  };
  aiTaskHandle = osThreadNew(AiTask, NULL, &aiTask_attr);

  osThreadId_t commTaskHandle;
  const osThreadAttr_t commTask_attr = {
    .name = "CommTask",
    .priority = osPriorityNormal,
    .stack_size = 512 * 4
  };
  commTaskHandle = osThreadNew(CommTask, NULL, &commTask_attr);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}
/* USER CODE BEGIN Header_StartDefaultTask */
/**
* @brief Function implementing the defaultTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN defaultTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1000);
  }
  /* USER CODE END defaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void SensorTask(void *argument) {
  SensorHub_Init();
  DebugConsole_Init();

  for (;;) {
    SensorHub_Update(&g_sensor_data);
    LogSensorData(g_sensor_data.temperature,
                  g_sensor_data.humidity,
                  g_sensor_data.voc_raw,
                  g_sensor_data.voc_index,
                  g_sensor_data.pm2_5,
                  g_sensor_data.pm10,
                  g_sensor_data.current,
                  g_sensor_data.power);
    osDelay(1000);
  }
}

void MotorTask(void *argument) {
  Motor_Init();

  for (;;) {
    Motor_Update();
    osDelay(10);  /* 100Hz: ramp + overcurrent check */
  }
}

void SystemTask(void *argument) {
  for (;;) {
    SystemManager_Update();
    osDelay(10);  /* 100Hz: buttons + state machine */
  }
}

void AiTask(void *argument) {
  AI_Init();

  for (;;) {
    uint8_t rec = AI_Evaluate(&g_sensor_data);
    SystemManager_SetAIMotorLevel(rec);
    osDelay(1000);  /* 1Hz: rule evaluation */
  }
}

void CommTask(void *argument) {
  CommTask_Init();

  for (;;) {
    CommTask_Update();
    osDelay(100);
  }
}
/* USER CODE END Application */


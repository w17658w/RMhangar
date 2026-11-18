/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "servo.h"
#include "referee.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* 开舱触发阶段：比赛中 */
#define HANGAR_OPEN_STAGE        REFEREE_STAGE_IN_BATTLE
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
osThreadId JudgeTaskHandle;
osThreadId VtxCmdTaskHandle;
osThreadId ControlTaskHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void const * argument);
void StartTask02(void const * argument);
void StartTask03(void const * argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
  /* place for user code */
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

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

  /* Create the thread(s) */
  /* definition and creation of JudgeTask */
  osThreadDef(JudgeTask, StartDefaultTask, osPriorityHigh, 0, 256);
  JudgeTaskHandle = osThreadCreate(osThread(JudgeTask), NULL);

  /* definition and creation of VtxCmdTask */
  osThreadDef(VtxCmdTask, StartTask02, osPriorityHigh, 0, 256);
  VtxCmdTaskHandle = osThreadCreate(osThread(VtxCmdTask), NULL);

  /* definition and creation of ControlTask */
  osThreadDef(ControlTask, StartTask03, osPriorityNormal, 0, 516);
  ControlTaskHandle = osThreadCreate(osThread(ControlTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the JudgeTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const * argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartTask02 */
/**
* @brief Function implementing the VtxCmdTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask02 */
void StartTask02(void const * argument)
{
  /* USER CODE BEGIN StartTask02 */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartTask02 */
}

/* USER CODE BEGIN Header_StartTask03 */
/**
* @brief Function implementing the ControlTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask03 */
void StartTask03(void const * argument)
{
  /* USER CODE BEGIN StartTask03 */
  servo_target_t s_door[SERVO_CH_NUM];
  uint8_t        s_door_is_open = 0u;  /* 舱门当前状态：0 = 关严，1 = 全开 */

  s_door[0].id  = SERVO_CH_DOOR_L;
  s_door[0].pos = HANGAR_DOOR_CLOSE_POS;
  s_door[1].id  = SERVO_CH_DOOR_R;
  s_door[1].pos = HANGAR_DOOR_CLOSE_POS;

  /* Infinite loop */
  for(;;)
  {
    Servo_Update();  /* 推进舵机插值，非阻塞 */

    if (!REFEREE_HAS_FRAME())
    {
      osDelay(10);
      continue;
    }

    uint8_t stage     = Referee_GetStage();
    uint8_t want_open;

    if (stage == HANGAR_OPEN_STAGE)
    {
      want_open = 1u;  /* 比赛中，开门 */
    }
    else if (stage == REFEREE_STAGE_SETTLEMENT)
    {
      want_open = 0u;  /* 结算，关门 */
    }
    else
    {
      want_open = s_door_is_open;  /* 其余阶段保持现状 */
    }

    /* 只有期望状态变了才下发，避免每轮重复改占空比 */
    if (want_open != s_door_is_open)
    {
      s_door_is_open = want_open;
      s_door[0].pos = want_open ? HANGAR_DOOR_OPEN_POS : HANGAR_DOOR_CLOSE_POS;
      s_door[1].pos = want_open ? HANGAR_DOOR_OPEN_POS : HANGAR_DOOR_CLOSE_POS;
      Servo_MoveMulti(s_door, SERVO_CH_NUM, HANGAR_DOOR_TIME_MS);
    }

    osDelay(10);
  }
/* USER CODE END StartTask03 */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

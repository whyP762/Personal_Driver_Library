/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "main.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "oled.h"
#include "esp8266.h"
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

/* USER CODE BEGIN PV */

 
typedef struct
{
  uint8_t rx_char;
  uint16_t index;
  uint8_t buffer[256];
  uint8_t Buf_flag;
} RxBufferType;

RxBufferType rx_buffer = {0};

uint8_t rx_char_uart2 = 0;

ESP8266_Config* esp8266_config;

SystemState system_state = STATE_INIT;

uint8_t key1_press = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */

void esp8266_rx_callback(ESP8266_Config* esp8266_config,uint8_t* data)
{
    uint8_t command = 0;
    if(esp8266_config != NULL)
    {
        if(sscanf(data,"servant+command+%d",&command) == 1)
        {
            if (command == 1)
            {
              HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
            }
        }
    }
}

int main(void)
{

  /* USER CODE BEGIN 1 */
  esp8266_config = ESP8266_Init(ESP8266_MODE_AP, &huart1, &htim1, "ESP8266", "12345678");
  /* USER CODE END 1 */
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */
  HAL_UART_Receive_IT(&huart1, &rx_buffer.rx_char, 1);
  HAL_UART_Receive_IT(&huart2, &rx_char_uart2, 1);
  Excute_User_Callback_Rx(esp8266_config, esp8266_rx_callback);
  OLED_Init();
  OLED_Clear();
  OLED_DisplayTurn(0);
  OLED_ColorTurn(0);
  /* USER CODE END 2 */
  // HAL_TIM_Base_Start_IT(&htim1);
  // printf("global_config.type:%d,esp8266_config->status:%d\n", esp8266_config->global_config.type, esp8266_config->status);
    
  uint8_t buf[256];
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  system_state = Get_ESP8266_SystemState(esp8266_config);
  while (1)
  {
    /* USER CODE END WHILE */
    if (key1_press)
    {
        sprintf(buf,"servant+command+1");
        Send_msg(esp8266_config, buf);
        key1_press = 0;
    }
    
    Get_ESP8266_SystemState(esp8266_config);
    Excute_Esp8266(esp8266_config);
    if(Get_ESP8266_SystemState(esp8266_config) != system_state)
    {
      sprintf(buf, "TEST: %d\n", Get_ESP8266_SystemState(esp8266_config));
      OLED_ShowString(0,0,buf,16,1);
      OLED_Refresh();
    }
    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    if(rx_buffer.rx_char == '\n' || rx_buffer.rx_char == '\r')
    {
      // rx_buffer.buffer[rx_buffer.index - 1] = '\n';  
      rx_buffer.buffer[rx_buffer.index] = '\0'; 
      rx_buffer.index = 0;  
      // HAL_UART_Transmit(&huart2, rx_buffer.buffer, strlen(rx_buffer.buffer), HAL_MAX_DELAY);
      rx_buffer.Buf_flag = 1;
      // printf("Received: %s\n", rx_buffer.buffer);
      UART_Handle_RX(esp8266_config, rx_buffer.buffer);
    }
    else
    {
      rx_buffer.buffer[rx_buffer.index++] = rx_buffer.rx_char;
      if (rx_buffer.index >= sizeof(rx_buffer.buffer))
      {
        rx_buffer.index = 0; 
      }
    }
    HAL_UART_Receive_IT(&huart1, &rx_buffer.rx_char, 1);
  }
  if (huart->Instance == USART2)
  {
    HAL_UART_Transmit(&huart2, &rx_char_uart2, 1, HAL_MAX_DELAY);
//    HAL_UART_Transmit(&huart1, &rx_char_uart2, 1, HAL_MAX_DELAY);
    HAL_UART_Receive_IT(&huart2, &rx_char_uart2, 1);
  }
  

}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  static uint32_t tick = 0;
   if (htim->Instance == TIM1)
   {
      tick++;
      Excute_ESP8266_TIMEOUT_CHECK(esp8266_config,2);
      if(tick % 250 == 0)
      {
        HAL_GPIO_TogglePin(LED_GPIO_Port,LED_Pin);

        tick = 0;
      }

   }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  // static uint32_t count = 0;
  if(GPIO_Pin == KEY1_Pin)
  {
    // printf("KEY1 pressed\n");
    key1_press = 1;
  }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

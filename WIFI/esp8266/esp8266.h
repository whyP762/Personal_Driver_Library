#pragma once
#include "main.h"
#include "usart.h"
#define ESP8266_TIMEOUT_TICK_SEC 3
#define ESP8266_RETRY_TIMERS 5

typedef struct ESP8266_Config ESP8266_Config;


typedef enum{
    ESP8266_MODE_STA = 1,
    ESP8266_MODE_AP,
    ESP8266_MODE_STA_AP
}ESP8266_Mode;


typedef void (*ESP8266_Callback)(ESP8266_Config* esp8266_config,uint8_t* data);


ESP8266_Config* ESP8266_Init(ESP8266_Mode mode,UART_HandleTypeDef *huart,TIM_HandleTypeDef *htim, const char* ssid, const char* password);
void Excute_Esp8266(ESP8266_Config* esp8266_config);
void UART_Handle_RX(ESP8266_Config* esp8266_config,uint8_t* data);
void Excute_ESP8266_TIMEOUT_CHECK(ESP8266_Config* esp8266_config,uint8_t pulse);
void Send_msg(ESP8266_Config* esp8266_config, const char* msg);
void Excute_User_Callback_Rx(ESP8266_Config* esp8266_config,ESP8266_Callback callback);

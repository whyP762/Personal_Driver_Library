#pragma once
#include "main.h"
#include "usart.h"
#define ESP8266_TIMEOUT_TICK_SEC 3
#define ESP8266_RETRY_TIMERS 5

typedef struct ESP8266_Config ESP8266_Config;

typedef enum {
    STATE_INIT,          // 初始化状态
    STATE_AT,            // AT指令测试
    STATE_CIPMUX_CONFIG,  // 基础配置（CIPMUX+CWMODE）
    STATE_CWMODE_CONFIG,  // CWMODE配置
    STATE_AP_CONFIG,     // AP配置（CIPAP+CWSAP）
    STATE_CREATE_WIFI_CONFIG, // 创建WIFI配置
    STATE_CWJAP_CONFIG,    // CWJAP配置
    STATE_TCP_SERVER,    // 启动TCP服务器
    STATE_READY,         // 就绪状态
    STATE_CLIENT_CONNECTING,   // 连接中状态
    STATE_CLIENT_CONNECTED, // 客户端连接
    STATE_RECIVE,           // 接收状态
    STATE_SENDING,       // 数据发送中
    STATE_ERROR,         // 错误状态
    STATE_TIMEOUT         // 超时状态
} SystemState;


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
SystemState Get_ESP8266_SystemState(ESP8266_Config* esp8266_config);

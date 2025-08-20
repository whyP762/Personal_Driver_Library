#include "esp8266.h"

typedef enum
{
    No_Client = 0,
    Exist_Client,
}client_id_T;

typedef enum
{
    False = 0,
    True = 1
}Change_Status_Flag_T;

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


struct ESP8266_Config
{
    uint8_t data[256];
    uint8_t ssid[32];
    uint8_t password[32];
    uint8_t AP_ip[32];
    Change_Status_Flag_T change_Status_flag;
    SystemState last_systemstate;              //上一状态
    SystemState systemstate;                  // 系统状态
    uint32_t tick;
    uint8_t retry_count;
    uint8_t current_client_id;
    client_id_T client_id[4];
    UART_HandleTypeDef *huart;                 // 串口句柄
    TIM_HandleTypeDef *htim;                   // 定时器句柄
    ESP8266_Mode mode;                         // 工作模式(AP,STA,AP+STA)
    ESP8266_Callback esp8266_rx_callback; // 接收回调函数
};

typedef struct {
    SystemState current;
    SystemState next;
    void (*handler)(ESP8266_Config*);
} StateTransition;


static void Excute_ESP8266_Init(ESP8266_Config* esp8266_config);
static void Excute_ESP8266_AT(ESP8266_Config* esp8266_config);
static void Excute_ESP8266_CIPMUX(ESP8266_Config* esp8266_config);
static void Excute_ESP8266_CWMODE(ESP8266_Config* esp8266_config);
static void Excute_ESP8266_CIPAP(ESP8266_Config* esp8266_config);
static void Excute_ESP8266_CWSAP(ESP8266_Config* esp8266_config);
static void Excute_ESP8266_READY(ESP8266_Config* esp8266_config);
static void Excute_TCP_ERROR(ESP8266_Config* esp8266_config);
static void handoff_ESP8266(ESP8266_Config* esp8266_config,SystemState new_state);
static void Excute_ESP8266_CIP_CONFIG(ESP8266_Config* esp8266_config);
static void Excute_ESP8266_CLIENT_CONNECTED(ESP8266_Config* esp8266_config);
static void Excute_ESP8266_Timeout(ESP8266_Config* esp8266_config);
static void Excute_ESP8266_STATE_RECIVE(ESP8266_Config* esp8266_config);
static void Excute_ESP8266_CLIENT_CONNECTING(ESP8266_Config* esp8266_config);
static void Excute_ESP8266_CWJAP_CONFIG(ESP8266_Config* esp8266_config);

static const StateTransition state_transitions[] = {
    {STATE_INIT, STATE_AT, Excute_ESP8266_Init},
    {STATE_AT, STATE_CIPMUX_CONFIG, Excute_ESP8266_AT},
    {STATE_CIPMUX_CONFIG, STATE_CWMODE_CONFIG, Excute_ESP8266_CIPMUX},
    {STATE_CWMODE_CONFIG, STATE_AP_CONFIG, Excute_ESP8266_CWMODE},
    {STATE_AP_CONFIG, STATE_CREATE_WIFI_CONFIG, Excute_ESP8266_CIPAP},
    {STATE_CREATE_WIFI_CONFIG, STATE_TCP_SERVER, Excute_ESP8266_CWSAP},
    {STATE_CWJAP_CONFIG, STATE_TCP_SERVER, Excute_ESP8266_CWJAP_CONFIG},
    {STATE_TCP_SERVER, STATE_READY, Excute_ESP8266_CIP_CONFIG},
    {STATE_READY, STATE_READY, Excute_ESP8266_READY}, // 客户端连接状态
    {STATE_CLIENT_CONNECTING, STATE_CLIENT_CONNECTED, Excute_ESP8266_CLIENT_CONNECTING}, // 连接中状态
    {STATE_CLIENT_CONNECTED, STATE_CLIENT_CONNECTED, Excute_ESP8266_CLIENT_CONNECTED}, // 发送状态
    {STATE_SENDING, STATE_ERROR, NULL}, // 错误处理
    {STATE_RECIVE, STATE_CLIENT_CONNECTED, Excute_ESP8266_STATE_RECIVE}, // 接收状态
    {STATE_ERROR, STATE_TIMEOUT, NULL}, // 超时处理
    {STATE_TIMEOUT, STATE_INIT, Excute_ESP8266_Timeout} // 重置状态
};


static ESP8266_Config esp8266_config;       //全局唯一结构体，所有AT命令的执行都依赖于此结构体



static void Excute_ESP8266_Init(ESP8266_Config* esp8266_config)
{
    // HAL_UART_Transmit(esp8266_config->huart, (uint8_t*)"AT\r\n", strlen("AT\r\n"), HAL_MAX_DELAY);
    esp8266_config->tick = 0;
    // esp8266_config->retry_count = 0;
    esp8266_config->systemstate = STATE_AT;
    esp8266_config->change_Status_flag = True;
    printf("ESP8266 Init...\n");
}
 

/**
 * @brief 执行AT命令,检测当前ESP8266是否可用
 * 
 * @param esp8266_config :结构体句柄
 */
static void Excute_ESP8266_AT(ESP8266_Config* esp8266_config)
{
    HAL_UART_Transmit(esp8266_config->huart, (uint8_t*)"AT\r\n", strlen("AT\r\n"), HAL_MAX_DELAY);
    esp8266_config->tick = 0;
    // esp8266_config->retry_count = 0;
    HAL_TIM_Base_Start_IT(esp8266_config->htim);
    esp8266_config->change_Status_flag = False;
    // HAL_Delay(100);
    // printf("ESP8266 AT Configing...\n");
}

/**
 * @brief 执行CIPSERVER命令，开启TCP服务器模式
 * 
 * @param esp8266_config :结构体句柄
 */
static void Excute_ESP8266_CIP_CONFIG(ESP8266_Config* esp8266_config)
{
    if(esp8266_config->mode != ESP8266_MODE_STA)
    {
        HAL_UART_Transmit(esp8266_config->huart, (uint8_t*)"AT+CIPSERVER=1,80\r\n", strlen("AT+CIPSERVER=1,80\r\n"), HAL_MAX_DELAY);
        esp8266_config->tick = 0;
        HAL_TIM_Base_Start_IT(esp8266_config->htim);
        esp8266_config->change_Status_flag = False;
    }
    else
    {
        HAL_UART_Transmit(esp8266_config->huart, (uint8_t*)"AT+CIPSTART=\"TCP\",\"192.168.4.1\",80\r\n", strlen("AT+CIPSTART=\"TCP\",\"192.168.4.4\",80\r\n"), HAL_MAX_DELAY);
        esp8266_config->tick = 0;
        HAL_TIM_Base_Start_IT(esp8266_config->htim);
        esp8266_config->change_Status_flag = False;
    }
}

/**
 * @brief 执行CIPAP命令，配置ESP8266的静态IP
 * 
 * @param esp8266_config :结构体句柄
 */
static void Excute_ESP8266_CIPAP(ESP8266_Config* esp8266_config)
{     
    if (esp8266_config->mode == ESP8266_MODE_STA)
    {
        handoff_ESP8266(esp8266_config,STATE_CWJAP_CONFIG);
        return;
    }
       
    char buf[64];
    sprintf(buf,"AT+CIPAP=\"%s\"\r\n", esp8266_config->AP_ip);
    HAL_UART_Transmit(esp8266_config->huart, (uint8_t*)buf, strlen(buf), HAL_MAX_DELAY);
    esp8266_config->tick = 0;
    // esp8266_config->retry_count = 0;
    HAL_TIM_Base_Start_IT(esp8266_config->htim);
    esp8266_config->change_Status_flag = False;
    // printf("ESP8266 CIPAP Configing,IP: %s\n", esp8266_config->AP_ip);
}

/**
 * @brief 执行CIPMUX命令，配置ESP8266的多路复用模式
 * 
 * @param esp8266_config :结构体句柄
 */
static void Excute_ESP8266_CIPMUX(ESP8266_Config* esp8266_config)
{
        char buf[100];
        if(esp8266_config->mode == ESP8266_MODE_STA)
        {
            sprintf(buf,"AT+CIPMUX=0\r\n");
        }
        else
        {
            sprintf(buf,"AT+CIPMUX=1\r\n");
        }
        HAL_UART_Transmit(esp8266_config->huart, (uint8_t*)buf, strlen(buf), HAL_MAX_DELAY);
        esp8266_config->tick = 0;
        // esp8266_config->retry_count = 0;
        HAL_TIM_Base_Start_IT(esp8266_config->htim);
        esp8266_config->change_Status_flag = False;
        // printf("ESP8266 CIPMUX Configing\n");

}

/**
 * @brief 执行CWSAP命令，配置ESP8266的热点
 * 
 * @param esp8266_config :结构体句柄
 */
static void Excute_ESP8266_CWSAP(ESP8266_Config* esp8266_config)
{
        uint8_t buf[256];
        sprintf(buf, "AT+CWSAP=\"%s\",\"%s\",%d,%d\r\n",
                esp8266_config->ssid,
                esp8266_config->password,
                1,
                4);
        HAL_UART_Transmit(esp8266_config->huart, buf, strlen(buf), HAL_MAX_DELAY);
        esp8266_config->tick = 0;
        // esp8266_config->retry_count = 0;
        HAL_TIM_Base_Start_IT(esp8266_config->htim);
        esp8266_config->change_Status_flag = False;
        // printf("WIFI CREATING\n");  
}


//static void Excute_ESP8266_CWSAP(ESP8266_Config* esp8266_config)
//{
//        uint8_t buf[256];
//        sprintf(buf, "AT+CWSAP=\"%s\",\"%s\",%d,%d\r\n",
//                esp8266_config->ssid,
//                esp8266_config->password,
//                1,
//                4);
//        HAL_UART_Transmit(esp8266_config->huart, buf, strlen(buf), HAL_MAX_DELAY);
//        esp8266_config->tick = 0;
//        // esp8266_config->retry_count = 0;
//        HAL_TIM_Base_Start_IT(esp8266_config->htim);
//        esp8266_config->change_Status_flag = False;
//        // printf("WIFI CREATING\n");  
//}


static void Excute_ESP8266_CWJAP_CONFIG(ESP8266_Config* esp8266_config)
{
        uint8_t buf[256];
        sprintf(buf, "AT+CWJAP=\"%s\",\"%s\"\r\n", esp8266_config->ssid, esp8266_config->password);
        HAL_UART_Transmit(esp8266_config->huart, buf, strlen(buf), HAL_MAX_DELAY);
        esp8266_config->tick = 0;
        // esp8266_config->retry_count = 0;
        HAL_TIM_Base_Start_IT(esp8266_config->htim);
        esp8266_config->change_Status_flag = False;
}



/**
 * @brief 执行CWMODE命令，配置ESP8266的工作模式
 * 
 * @param esp8266_config :结构体句柄
 */
static void Excute_ESP8266_CWMODE(ESP8266_Config* esp8266_config)
{
        if (esp8266_config->mode == ESP8266_MODE_STA)
        {
            HAL_UART_Transmit(esp8266_config->huart, (uint8_t*)"AT+CWMODE=1\r\n", strlen("AT+CWMODE=1\r\n"), HAL_MAX_DELAY);
        }
        else if (esp8266_config->mode == ESP8266_MODE_AP)
        {
            HAL_UART_Transmit(esp8266_config->huart, (uint8_t*)"AT+CWMODE=2\r\n", strlen("AT+CWMODE=2\r\n"), HAL_MAX_DELAY);
        }
        else if (esp8266_config->mode == ESP8266_MODE_STA_AP)
        {
            HAL_UART_Transmit(esp8266_config->huart, (uint8_t*)"AT+CWMODE=3\r\n", strlen("AT+CWMODE=3\r\n"), HAL_MAX_DELAY);
        }        
        esp8266_config->tick = 0;
        // esp8266_config->retry_count = 0;
        HAL_TIM_Base_Start_IT(esp8266_config->htim);
        esp8266_config->change_Status_flag = False;
        // printf("ESP8266_MODE Configing %d\n", esp8266_config->mode);

}


static void Excute_ESP8266_READY(ESP8266_Config* esp8266_config)
{
    if (esp8266_config->mode == ESP8266_MODE_STA)
    {
        handoff_ESP8266(esp8266_config, STATE_CLIENT_CONNECTED);
        return;
    }
    
        printf("ESP8266 is ready,waiting connecting...\n");
        esp8266_config->change_Status_flag = False;
        esp8266_config->systemstate = STATE_READY;
}



static void Excute_ESP8266_Timeout(ESP8266_Config* esp8266_config)
{
    if (esp8266_config->systemstate == STATE_TIMEOUT)
    {
        printf("ESP8266 Timeout!!!\n");
        esp8266_config->change_Status_flag = False;
    }
}

static void TCP_Send_Data(ESP8266_Config* esp8266_config, char* data, uint16_t length)
{
    uint32_t delay_count = 100000;
    if(esp8266_config->systemstate == STATE_CLIENT_CONNECTED)
    {
        handoff_ESP8266(esp8266_config,STATE_CLIENT_CONNECTING);
        HAL_TIM_Base_Start_IT(esp8266_config->htim);
        char buf[256];
        if(esp8266_config->mode != ESP8266_MODE_STA)
        {
            snprintf(buf, sizeof(buf), "AT+CIPSEND=%d,%d\r\n", esp8266_config->current_client_id,length+1);
            HAL_UART_Transmit(esp8266_config->huart, (uint8_t*)buf, strlen(buf), HAL_MAX_DELAY);
            while(delay_count--);
            sprintf(buf,"%s\n",data);
            HAL_UART_Transmit(esp8266_config->huart, (uint8_t*)buf, strlen(buf), HAL_MAX_DELAY);
            // HAL_Delay(10);
        }
        else
        {
            snprintf(buf, sizeof(buf), "AT+CIPSEND=%d\r\n", length+1);
            HAL_UART_Transmit(esp8266_config->huart, (uint8_t*)buf, strlen(buf), HAL_MAX_DELAY);
            while(delay_count--);
            sprintf(buf,"%s\n",data);
            HAL_UART_Transmit(esp8266_config->huart, (uint8_t*)buf, strlen(buf), HAL_MAX_DELAY);
            // HAL_Delay(10);
        }
        esp8266_config->change_Status_flag = False;
    }
}


static void Excute_ESP8266_CLIENT_CONNECTING(ESP8266_Config* esp8266_config)
{
    // TCP_Send_Data(esp8266_config, esp8266_config->current_client_id, "ACK\n", strlen("ACK\n"));
    handoff_ESP8266(esp8266_config, STATE_CLIENT_CONNECTED);
    if(esp8266_config->mode != ESP8266_MODE_STA)
    {
        esp8266_config->change_Status_flag = True;
        esp8266_config->client_id[esp8266_config->current_client_id] = Exist_Client;
    }
    else
    {
        HAL_TIM_Base_Start_IT(esp8266_config->htim);
        esp8266_config->change_Status_flag = False;
    }
}


static void Excute_ESP8266_CLIENT_CONNECTED(ESP8266_Config* esp8266_config)
{
    if(esp8266_config->mode != ESP8266_MODE_STA)
    {
        for (uint8_t i = 0; i < 4; i++)
        {
            if (esp8266_config->client_id[i] == Exist_Client)
            {
                // printf("ESP8266 Client ID found: %d\n", i);
                break;
            }
            else if (i == 3)
            {
                printf("ESP8266 Client ID not found\n");
                handoff_ESP8266(esp8266_config, STATE_READY);
                return;
            }
        }
        printf("ESP8266 Client Connected,ID: %d\n", esp8266_config->current_client_id);
        esp8266_config->change_Status_flag = False;
    }
    else
    {
        printf("ESP8266 Connected to AP\n");
        esp8266_config->change_Status_flag = False;
    }
}

static void Excute_ESP8266_STATE_RECIVE(ESP8266_Config* esp8266_config)
{
    uint8_t payload[256] = {0};
    uint8_t length = 0;
    if(esp8266_config->last_systemstate != STATE_CLIENT_CONNECTED)
    {
        return;
    }
    if(esp8266_config->mode != ESP8266_MODE_STA)
    {
        if(sscanf(esp8266_config->data,"+IPD,%d,%d:%s",&esp8266_config->current_client_id,&length,&payload) == 3)
        {
            // printf("%s\n",esp8266_config->data);
            esp8266_config->client_id[esp8266_config->current_client_id] = Exist_Client;
            if(esp8266_config->esp8266_rx_callback == NULL)
            {
                printf("Received data from client %d: %s\n", esp8266_config->current_client_id, payload);
            }
            else
            {
                esp8266_config->esp8266_rx_callback(esp8266_config, payload);
            }
        }
    }
    else
    {
        if(sscanf(esp8266_config->data,"+IPD,%d:%s",&length,& payload) == 2)
        {
            if(esp8266_config->esp8266_rx_callback == NULL)
            {
                printf("Received data from client %d: %s\n", esp8266_config->current_client_id, payload);
            }
            else
            {
                esp8266_config->esp8266_rx_callback(esp8266_config, payload);
            }
        }
    }
    // TCP_Send_Data(esp8266_config, esp8266_config->current_client_id, "ACK\n", strlen("ACK\n"));
    handoff_ESP8266(esp8266_config, STATE_CLIENT_CONNECTED);
    esp8266_config->change_Status_flag = True;
}



static void handoff_ESP8266(ESP8266_Config* esp8266_config,SystemState new_state)
{
    if (esp8266_config == NULL)
    {
        return;
    }
    HAL_TIM_Base_Stop_IT(esp8266_config->htim);
    esp8266_config->change_Status_flag = True;
    // if(esp8266_config->systemstate == new_state)
    // {
    //     esp8266_config->change_Status_flag = False;
    // }
    esp8266_config->last_systemstate = esp8266_config->systemstate;
    esp8266_config->systemstate = new_state;
    printf("ESP8266 State Transition: %d -> %d\n", esp8266_config->last_systemstate, esp8266_config->systemstate);

    esp8266_config->retry_count = 0;

}


static void ESP8266_Status_Excute(ESP8266_Config* esp8266_config)
{
    for(int i=0; i < sizeof(state_transitions) / sizeof(state_transitions[0]); i++) {
        if(state_transitions[i].current == esp8266_config->systemstate) {
            if(state_transitions[i].handler) {
                state_transitions[i].handler(esp8266_config);
            }
            break;
        }
    }
}



static void ESP8266_Status_Switch(ESP8266_Config* esp8266_config)
{
    // if(esp8266_config->systemstate)
    // {
        for(int i=0; i < sizeof(state_transitions) / sizeof(state_transitions[0]); i++) {
            if(state_transitions[i].current == esp8266_config->systemstate) {
                handoff_ESP8266(esp8266_config, state_transitions[i].next);
                return;
            }
        }
}


ESP8266_Config* ESP8266_Init(ESP8266_Mode mode,UART_HandleTypeDef *huart,TIM_HandleTypeDef *htim, const char* ssid, const char* password)
{
    while (ssid == NULL || password == NULL);
    
    ESP8266_Config* esp8266_config_temp = &esp8266_config;
    esp8266_config_temp->tick = 0;
    esp8266_config_temp->change_Status_flag = True;
    esp8266_config_temp->last_systemstate = STATE_INIT; // 上一状态
    esp8266_config_temp->systemstate = STATE_INIT; // 初始化状态
    esp8266_config_temp->current_client_id = 255;
    esp8266_config_temp->client_id[0] = No_Client;
    esp8266_config_temp->client_id[1] = No_Client;
    esp8266_config_temp->client_id[2] = No_Client;
    esp8266_config_temp->client_id[3] = No_Client;
    esp8266_config_temp->retry_count = 0;
    esp8266_config_temp->huart = huart;
    esp8266_config_temp->htim = htim;
    esp8266_config_temp->esp8266_rx_callback = NULL; // 初始化回调函数为NULL
    strcpy((char *)esp8266_config_temp->ssid, ssid);
    strcpy((char *)esp8266_config_temp->password, password);
    strcpy((char *)esp8266_config_temp->AP_ip, "192.168.4.1");
    esp8266_config_temp->mode = mode;
    printf("Init success\n");

    return esp8266_config_temp;
}


static void UART_Handle_RX_ESP8266_STATUS(ESP8266_Config* esp8266_config)
{
    if(strcmp((char *)esp8266_config->data, "WIFI DISCONNECTED") == 0)
    {
        handoff_ESP8266(esp8266_config, STATE_CWJAP_CONFIG);
    }
    else if(strstr((char *)esp8266_config->data, "CONNECT") != NULL && esp8266_config->mode != ESP8266_MODE_STA)
    {
        // printf("TCP Connection Request: %s\n", esp8266_config->data);
        if(sscanf(esp8266_config->data,"%d,CONNECT",&esp8266_config->current_client_id) == 1 && esp8266_config->mode != ESP8266_MODE_STA)
        {
            printf("TCP Connection ID: %d\n", esp8266_config->current_client_id);
            handoff_ESP8266(esp8266_config, STATE_CLIENT_CONNECTING);
            // esp8266_config->systemstate = STATE_CLIENT_CONNECTED;
            // esp8266_config->change_Status_flag = True;
            // esp8266_config->retry_count = 0;
        }
        else if (strcmp((char *)esp8266_config->data, "CONNECT") == 0 && esp8266_config->mode == ESP8266_MODE_STA)
        {
            printf("TCP CIPSTART success!!\n");
            handoff_ESP8266(esp8266_config,STATE_CLIENT_CONNECTED);
        }
    }
    else if(strstr((char*)esp8266_config->data,"ERROR"))
    {
        printf("esp8266_status,esp8266_config->systemstate == %d\n", esp8266_config->systemstate);
    }
    switch (esp8266_config->systemstate)
    {
    case STATE_INIT:
        ESP8266_Status_Switch(esp8266_config);
        break;
    case STATE_AT:
        if(strstr((char *)esp8266_config->data, "OK") != NULL)
        {
            ESP8266_Status_Switch(esp8266_config);
        }
        break;
    case STATE_CIPMUX_CONFIG:
        if(strstr((char *)esp8266_config->data, "OK") != NULL)
        {
            ESP8266_Status_Switch(esp8266_config);
        }
        break;
    case STATE_CWMODE_CONFIG:
        if(strstr((char *)esp8266_config->data, "OK") != NULL)
        {
            ESP8266_Status_Switch(esp8266_config);
        }
        break;
    case STATE_CWJAP_CONFIG:
        if (strcmp((char *)esp8266_config->data, "WIFI CONNECTED") == 0)
        {
            printf("%s\n",esp8266_config->data);
            ESP8266_Status_Switch(esp8266_config);    
        }
        break;
    case STATE_CREATE_WIFI_CONFIG:
        if (strstr((char *)esp8266_config->data, "OK") != NULL)
        {
            printf("%s\n",esp8266_config->data);
            ESP8266_Status_Switch(esp8266_config);
        }
        break;
    case STATE_AP_CONFIG:
        if (strstr((char *)esp8266_config->data, "OK") != NULL)
        {
            ESP8266_Status_Switch(esp8266_config);
        }
        break;
    case STATE_TCP_SERVER:
        static uint8_t first_OK = 0;
        if (strstr((char *)esp8266_config->data, "OK") != NULL)
        {
            printf("%s",esp8266_config->data);
            if (first_OK == 0)
            {
                first_OK = 1;
            }
            else
            {
                ESP8266_Status_Switch(esp8266_config);
                first_OK = 0;
            }
        }
        break;
    case STATE_CLIENT_CONNECTING:
        if(strstr((char *)esp8266_config->data,"SEND OK") != NULL)
        {
            ESP8266_Status_Switch(esp8266_config);   
        }
        break;
    case STATE_CLIENT_CONNECTED:
        
        if (strstr((char *)esp8266_config->data,"+IPD") != NULL)
        {
            // esp8266_config->systemstate = STATE_RECIVE;
            handoff_ESP8266(esp8266_config, STATE_RECIVE);
        }
        else if (strstr((char *)esp8266_config->data,"CLOSED") != NULL)
        {
            if (esp8266_config->mode != ESP8266_MODE_STA)
            {
                if (sscanf((char *)esp8266_config->data,"%d,CLOSED",&esp8266_config->current_client_id) == 1)
                {
                    printf("TCP CLOSED ID: %d\n", esp8266_config->current_client_id);
                    esp8266_config->client_id[esp8266_config->current_client_id] = No_Client;
                    esp8266_config->current_client_id = 255;
                    handoff_ESP8266(esp8266_config, STATE_CLIENT_CONNECTED);
                }
                
            }
            else if (esp8266_config->mode == ESP8266_MODE_STA)
            {
                printf("TCP CLOSED\n");
                handoff_ESP8266(esp8266_config, STATE_CWJAP_CONFIG);
            }
        }

        break;
    default:
        break;
    }
}

void Excute_Esp8266(ESP8266_Config* esp8266_config)
{
    if (esp8266_config->change_Status_flag == True)
    {        
        ESP8266_Status_Excute(esp8266_config);
    }
}




void Send_msg(ESP8266_Config* esp8266_config, const char* msg)
{
    if (esp8266_config != NULL && esp8266_config->huart != NULL && msg != NULL && esp8266_config->systemstate == STATE_CLIENT_CONNECTED)
    {
        uint16_t length = strlen(msg);

        if (length > 0)
        {
            TCP_Send_Data(esp8266_config, (char*)msg, length);
        }
    }
    else
    {
        printf("ESP8266 not ready or invalid parameters.\n");
    }
}


void UART_Handle_RX(ESP8266_Config* esp8266_config,uint8_t* data)
{
    if (esp8266_config != NULL && esp8266_config->huart != NULL)
    {
        strcpy((char*) esp8266_config->data, (char*)data);
        UART_Handle_RX_ESP8266_STATUS(esp8266_config);
    }
}

void Excute_ESP8266_TIMEOUT_CHECK(ESP8266_Config* esp8266_config,uint8_t pulse)
{
    if (esp8266_config != NULL)
    {
        esp8266_config->tick++;
        // printf("ESP8266 Timeout Check: %d\n", esp8266_config->global_config.tick);
        if(esp8266_config->tick >= ESP8266_TIMEOUT_TICK_SEC * 1000 / pulse)
        {
            esp8266_config->tick = 0;
            esp8266_config->retry_count++;
            if(esp8266_config->change_Status_flag == False)
            {
                esp8266_config->change_Status_flag = True;
                if(esp8266_config->mode == ESP8266_MODE_STA && esp8266_config->systemstate == STATE_CWJAP_CONFIG)
                {
                    printf("join Wifi timeout...,retrying...\n");
                    esp8266_config->retry_count = 0;
                }
                else
                {
                    printf("ESP8266 Timeout,retrying %d times...\n", esp8266_config->retry_count);
                }
            }
            if(esp8266_config->retry_count >= ESP8266_RETRY_TIMERS)
            {
                esp8266_config->retry_count = 0;
                HAL_TIM_Base_Stop_IT(esp8266_config->htim);
                esp8266_config->change_Status_flag = True;
                esp8266_config->systemstate = STATE_TIMEOUT;
            }
        }
    }
}


void Excute_User_Callback_Rx(ESP8266_Config* esp8266_config,ESP8266_Callback callback)
{
    if(esp8266_config != NULL)
    {
        esp8266_config->esp8266_rx_callback = callback;
    }
}


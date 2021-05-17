#include <stdio.h>
#include "stdlib.h"
#include <string.h>

#include "driver/rmt.h"
#include "led_strip.h"
#include "esp_system.h"
#include "esp_log.h"

#include "cJSON.h"
/*********RTOS Handle-file****************/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

/*********UART Handle-file*****************/
#include "driver/gpio.h"
#include "driver/uart.h"
/**********WiFI Handle-file**************/
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
/***********MQTT Handle-file************/
#include "mqtt_client.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

static const char *TAG = "WS2812S";
static const int RX_BUF_SIZE = 1024;

#define UART_BAUD 38400
#define TXD_PIN (GPIO_NUM_4)
#define RXD_PIN (GPIO_NUM_5)
/**********************************************/
static led_strip_t *strip;

#define RMT_TX_NUM 3
#define RMT_TX_CHANNEL RMT_CHANNEL_0 
#define LED_STRIP_NUM 24

#define LED_OPEN 6
#define LED_CLOSE 7
/*************color define************/
#define COLOR_RED 9
#define COLOR_GREE 8
#define COLOR_BULE 10
#define COLOR_PURPLE 11
#define COLOR_ORANGE 12

struct WS2812_COLOR
{
	uint32_t red;
	uint32_t green;
	uint32_t blue;
};

struct WS2812_COLOR WS2812_RGB;
/************* MQTT Link define**************/

#define MQTT_LINK_Aliyum 1 // 1:connect aliyum  0:not connect aliyum

#define MQTT_LINK_HOST "a1CeZLiC2fi.iot-as-mqtt.cn-shanghai.aliyuncs.com"
#define MQTT_LINK_PORT 1883

//是否开启发布 1 为开启发布 0：关闭发布
#define MQTT_Pub_Status 0

#if MQTT_LINK_Aliyum
/***** 阿里云设备三元组 ************/
#define MQTT_DeviceName "led_strip"
#define MQTT_ProductKey "a1CeZLiC2fi"
char MQTT_LINK_USERNAME[128];
/******* 数据发布的主题**************/
#define MQTT_PUB_TOPIC "/sys/a1CeZLiC2fi/led-strip/thing/service/property/set"

#elif

#define MQTT_LINK_USERNAME "  "

#endif

#define MQTT_LINK_PASS "44c378c18635f42569af46a8924cf370"  //加过hmacmd5加密后的密码
#define MQTT_LINK_CLIENT_ID "led_strip|securemode=3,signmethod=hmacmd5|" //客户端ID 阿里云

static esp_mqtt_client_handle_t client;
bool isConnect2Server = false;

char MqttTopicSub[128], MqttTopicPub[128];

static xQueueHandle ParseJSONQueueHandler = NULL;				//解析json数据的队列
static xTaskHandle mHandlerParseJSON = NULL, handleMqtt = NULL; //任务队列
//发布需要发布的数据
char cJson_data[1024];

typedef struct __User_data
{
	char allData[1024];
	int dataLen;
} User_data;

User_data user_data;

/***************************************************************/
void uartControlLedStrip(int cmd_id);
void set_rgb(uint32_t rgb_24bit);
void WS2812_RGB_cJSONData(struct WS2812_COLOR *ws2812_rgb);
/*************************************************************/

//void set_rgb(uint16_t Red, uint16_t Green, uint16_t Blue)
void set_rgb(uint32_t rgb_24bit)
{	
	WS2812_RGB.blue=rgb_24bit & 0Xff;
	rgb_24bit=rgb_24bit>>8;
	WS2812_RGB.green=rgb_24bit & 0xff;
	rgb_24bit=rgb_24bit>>8;
	WS2812_RGB.red=rgb_24bit&0xff;
	for (int i = 0; i < LED_STRIP_NUM; i++)
	{
		strip->set_pixel(strip, i, WS2812_RGB.red, WS2812_RGB.green,WS2812_RGB.blue);
	}
	/*WS2812_RGB.red = Red;
	WS2812_RGB.green = Green;
	WS2812_RGB.blue = Blue;*/
	strip->refresh(strip, 10);
}
//Ws2812 init function
void init_led()
{
	rmt_config_t config = RMT_DEFAULT_CONFIG_TX(RMT_TX_NUM, RMT_TX_CHANNEL);
	// set counter clock to 40MHz
	config.clk_div = 2;

	ESP_ERROR_CHECK(rmt_config(&config));
	ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));

	// install ws2812 driver
	led_strip_config_t strip_config = LED_STRIP_DEFAULT_CONFIG(24, (led_strip_dev_t)config.channel);
	strip = led_strip_new_rmt_ws2812(&strip_config);
	if (!strip)
	{
		ESP_LOGE(TAG, "install WS2812 driver failed");
	}
	// Clear LED strip (turn off all LEDs)
	ESP_ERROR_CHECK(strip->clear(strip, 100));
	set_rgb(0xff00);
}
/* 
 * @Description: UART配置
 * @param: 
 * @return: 
 */
void UART_Init(void)
{
	const uart_config_t uart_config = {
		.baud_rate = UART_BAUD,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_APB,
	};
	// We won't use a buffer for sending data.
	uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
	uart_param_config(UART_NUM_1, &uart_config);
	uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}
/*
 *
 * USART DATA handle
 *
 *
 */
int uartDataHandle(uint8_t *data)
{
	uint8_t *temp;
	char *TAG = "uartDataHandle";
	int ID_NUM = 0;
	temp = data;
	if (temp != NULL)
	{
		temp = (uint8_t *)strstr((const char *)temp, "ID:");
		if (temp != NULL)
		{
			temp = (uint8_t *)strstr((const char *)temp, ":");
			++temp;
			ID_NUM = atoi((const char *)temp);
			ESP_LOGI(TAG, "Cmd ID:%d", ID_NUM);
			return ID_NUM;
		}
		else
			goto __RETURN_ID;
	}
	else
		goto __RETURN_ID;

__RETURN_ID:
	return ID_NUM;
}
/*
 *
 * uart controu LED strip 
 *
 */
void uartControlLedStrip(int cmd_id)
{
	switch (cmd_id)
	{
	case LED_OPEN:
		set_rgb(0xFFFFF);
		break;
	case LED_CLOSE:
		set_rgb(0X000000);
		break;
	case COLOR_RED:
		if ((WS2812_RGB.red == 0) && (WS2812_RGB.green == 0) && (WS2812_RGB.blue == 0))
		{
			break;
		}
		set_rgb(0Xff0000);
		break;

	case COLOR_GREE:
		if (WS2812_RGB.red == 0 && WS2812_RGB.green == 0 && WS2812_RGB.blue == 0)
		{
			break;
		}
		set_rgb(0x00FF00);
		break;

	case COLOR_BULE:
		if (WS2812_RGB.red == 0 && WS2812_RGB.green == 0 && WS2812_RGB.blue == 0)
		{
			break;
		}
		set_rgb(0X0000ff);
		break;

	case COLOR_ORANGE:
		if (WS2812_RGB.red == 0 && WS2812_RGB.green == 0 && WS2812_RGB.blue == 0)
		{

			break;
		}
		set_rgb(0XF05308);
		break;

	case COLOR_PURPLE:
		if (WS2812_RGB.red == 0 && WS2812_RGB.green == 0 && WS2812_RGB.blue == 0)
		{

			break;
		}
		set_rgb(0X8C0BEE);
		break;
	}
}
/*
 *UART RX Task
 *
 *
 */

static void uartRxTask(void *arg)
{
	static const char *RX_TASK_TAG = "RX_TASK";
	esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
	uint8_t *data = (uint8_t *)malloc(RX_BUF_SIZE + 1);

	while (1)
	{
		const int rxBytes = uart_read_bytes(UART_NUM_1, data, RX_BUF_SIZE, 1000 / portTICK_RATE_MS);
		if (rxBytes > 0)
		{
			data[rxBytes] = 0;
			ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);
			memset(cJson_data, 0, sizeof(cJson_data));
			uartControlLedStrip(uartDataHandle(data));
#if MQTT_Pub_Status
			WS2812_RGB_cJSONData(&WS2812_RGB);
			printf("%s\r\n", cJson_data);
#endif
			ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);
		}
	}
	free(data);
}
/**
 *
 *
 *  cJSON Creat data
 *
 *
 */
#if MQTT_Pub_Status
void WS2812_RGB_cJSONData(struct WS2812_COLOR *ws2812_rgb)
{

	cJSON *Root = cJSON_CreateObject();

	cJSON *Params = cJSON_CreateObject();
	cJSON *RGB = cJSON_CreateObject();

	cJSON_AddStringToObject(Root, "method", "thing.event.property.post");
	cJSON_AddStringToObject(Root, "id", "123456");

	cJSON_AddItemToObject(Root, "params", Params);

	cJSON_AddItemToObject(Params, "RGB", RGB);
	cJSON_AddNumberToObject(RGB, "R", ws2812_rgb->red);
	cJSON_AddNumberToObject(RGB, "G", ws2812_rgb->green);
	cJSON_AddNumberToObject(RGB, "B", ws2812_rgb->blue);
	cJSON_AddStringToObject(Root, "version", "1.0.0");

	//cJson_data=cJSON_Print(Root);

	strcpy(cJson_data, cJSON_Print(Root));

	cJSON_Delete(Root);
}
#endif
/*
 * @Description: MQTT服务器的下发消息回调
 * @param:
 * @return:
 */
esp_err_t MqttCloudsCallBack(esp_mqtt_event_handle_t event)
{
	int msg_id;
	client = event->client;
	switch (event->event_id)
	{
	//连接成功
	case MQTT_EVENT_CONNECTED:
		ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
		msg_id = esp_mqtt_client_subscribe(client, MqttTopicSub, 1);
		ESP_LOGI(TAG, "sent subscribe[%s] successful, msg_id=%d", MqttTopicSub, msg_id);
		ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
		//post_data_to_clouds();
		isConnect2Server = true;
		break;
		//断开连接回调
	case MQTT_EVENT_DISCONNECTED:
		ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
		isConnect2Server = false;
		break;
		//订阅成功
	case MQTT_EVENT_SUBSCRIBED:
		ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
		break;
		//订阅失败
	case MQTT_EVENT_UNSUBSCRIBED:
		ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
		break;
		//推送发布消息成功
	case MQTT_EVENT_PUBLISHED:
		ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
		break;
		//服务器下发消息到本地成功接收回调
	case MQTT_EVENT_DATA:
	{
		printf("TOPIC=%.*s \r\n", event->topic_len, event->topic);
		printf("DATA=%.*s \r\n\r\n", event->data_len, event->data);
		//发送数据到队列
		struct __User_data *pTmper;
		memset(&user_data, 0, sizeof(struct __User_data));
		sprintf(user_data.allData, "%.*s", event->data_len, event->data);
		pTmper = &user_data;
		user_data.dataLen = event->data_len;

		xQueueSend(ParseJSONQueueHandler, (void *)&pTmper, portMAX_DELAY);
		break;
	}
	default:
		break;
	}
	return ESP_OK;
}
/* 
 * @Description: MQTT参数连接的配置
 * @param: 
 * @return: 
 */
void TaskXMqttRecieve(void *p)
{
	//连接的配置参数
#if MQTT_LINK_Aliyum
	sprintf(MQTT_LINK_USERNAME, "%s&%s", MQTT_DeviceName, MQTT_ProductKey);
#endif
	esp_mqtt_client_config_t mqtt_cfg = {
		.host = MQTT_LINK_HOST,			//连接的域名 ，请务必修改为您的
		.port = MQTT_LINK_PORT,			//端口，请务必修改为您的
		.username = MQTT_LINK_USERNAME, //用户名，请务必修改为您的
		.password = MQTT_LINK_PASS,		//密码，请务必修改为您的
		.client_id = MQTT_LINK_CLIENT_ID,
		.event_handle = MqttCloudsCallBack, //设置回调函数
		.keepalive = 120,					//心跳
		.disable_auto_reconnect = false,	//开启自动重连
		.disable_clean_session = false,		//开启 清除会话
	};
	client = esp_mqtt_client_init(&mqtt_cfg);
	esp_mqtt_client_start(client);

	vTaskDelete(NULL);
}
/*
*
*颜色提示音
*
*/
unsigned char beep[][4] = {
	{0xAA, 0x02, 0x03, 0xBB}, //开灯
	{0xAA, 0x02, 0x04, 0xBB}, //关灯
	{0xAA, 0x02, 0x05, 0xBB}, //调成绿色
	{0xAA, 0x02, 0x06, 0xBB}, //调成红色
	{0xAA, 0x02, 0x07, 0xBB}, //调成蓝色
	{0xAA, 0x02, 0x08, 0xBB}, //调成紫色
	{0xAA, 0x02, 0x09, 0xBB}, //调成橙色
};

int beepPlayTheTone(struct WS2812_COLOR *RGB)
{

	if (RGB->green == 254 && (RGB->blue + RGB->red) == 0)
	{
		uart_write_bytes(UART_NUM_1, beep[2], sizeof(beep[0]));
		return ESP_OK;
	}

	if (RGB->red == 254 && (RGB->blue + RGB->green) == 0)
	{
		uart_write_bytes(UART_NUM_1, beep[3], sizeof(beep[0]));
		return ESP_OK;
	}

	if (RGB->blue == 254 && (RGB->green + RGB->red) == 0)
	{
		uart_write_bytes(UART_NUM_1, beep[4], sizeof(beep[0]));
		return ESP_OK;
	}

	if (RGB->blue == 238 && ((RGB->blue + RGB->green + RGB->red) == 391) &&RGB->red == 140)
	{
		uart_write_bytes(UART_NUM_1, beep[5], sizeof(beep[0]));
		return ESP_OK;
	}

	if (RGB->red == 240 && (RGB->blue + RGB->green == 94) && RGB->blue == 11)
	{
		uart_write_bytes(UART_NUM_1, beep[6], sizeof(beep[0]));
		return ESP_OK;
	}

	if ((RGB->red + RGB->green + RGB->blue) == 762)
	{
		uart_write_bytes(UART_NUM_1, beep[0], 4); //播放：已经打开灯了
		return ESP_OK;
	}

	if ((RGB->red + RGB->green + RGB->blue) == 0)
	{
		uart_write_bytes(UART_NUM_1, beep[1], 4); //播放：已经关灯了
		return ESP_OK;
	}
	return ESP_FAIL;
}

/* 
 * @Description: 解析下发数据的队列逻辑处理
 * @param: null
 * @return: 
 */
void Task_ParseJSON(void *pvParameters)
{
	printf("[SY] Task_ParseJSON_Message creat ... \n");

	while (1)
	{
		struct __User_data *pMqttMsg;

		printf("Task_ParseJSON_Message xQueueReceive wait [%d] ... \n", esp_get_free_heap_size());
		xQueueReceive(ParseJSONQueueHandler, &pMqttMsg, portMAX_DELAY);

		printf("Task_ParseJSON_Message xQueueReceive get [%s] ... \n", pMqttMsg->allData);

		////首先整体判断是否为一个json格式的数据
		cJSON *pJsonRoot = cJSON_Parse(pMqttMsg->allData);

		//如果是否json格式数据
		if (pJsonRoot == NULL)
		{
			printf("[SY] Task_ParseJSON_Message xQueueReceive not json ... \n");
			goto __cJSON_Delete;
		}

		cJSON *pJsonParams = cJSON_GetObjectItem(pJsonRoot, "params");
		if (pJsonParams == NULL)
		{
			printf("pJsonParams NULL\r\n");
			goto __cJSON_Delete;
		}

		cJSON *pJsonGRB = cJSON_GetObjectItem(pJsonParams, "RGB");
		if (pJsonGRB == NULL)
			goto __cJSON_Delete;

		/*cJSON *pJSON_Item_Red = cJSON_GetObjectItem(pJsonGRB, "R");
		cJSON *pJSON_Item_Gree = cJSON_GetObjectItem(pJsonGRB, "G");
		cJSON *pJSON_Item_Blue = cJSON_GetObjectItem(pJsonGRB, "B");*/

		set_rgb(pJsonGRB->valueint);

		//播放提示音
		beepPlayTheTone(&WS2812_RGB);
	__cJSON_Delete:
		cJSON_Delete(pJsonRoot);
	}
}

void app_main(void)
{
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	init_led();
	UART_Init();
	set_rgb(0x0);
	//开启串口接收任务
	xTaskCreate(uartRxTask, "uart_rx_task", 1024 * 3, NULL, 4, NULL);
	//组建MQTT订阅的主题
	sprintf(MqttTopicSub, "/sys/%s/%s/thing/service/property/set", MQTT_ProductKey, MQTT_DeviceName);
	//组建MQTT推送的主题
	//sprintf(MqttTopicPub, "/sys/%s/%s/thing/event/property/post",MQTT_ProductKey,MQTT_DeviceName);

	ESP_LOGI(TAG, "MqttTopicSub: %s", MqttTopicSub);
	//ESP_LOGI(TAG, "MqttTopicPub: %s", MqttTopicPub);

	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	/* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
	 * Read "Establishing Wi-Fi or Ethernet Connection" section in
	 * examples/protocols/README.md for more information about this function.
	 */
	ESP_ERROR_CHECK(example_connect());

	//开启MQTT连接任务
	if (handleMqtt == NULL)
		xTaskCreate(TaskXMqttRecieve, "MQTT_STA_TASK", 1024 * 4, NULL, 5, &handleMqtt);
	//创建消息队列
	if (ParseJSONQueueHandler == NULL)
		ParseJSONQueueHandler = xQueueCreate(5, sizeof(struct __User_data *));
	//开启json解析线程

	if (mHandlerParseJSON == NULL)
	{
		xTaskCreate(Task_ParseJSON, "Task_ParseJSONData", 1024 * 3, NULL, 6, &mHandlerParseJSON);
	}
}

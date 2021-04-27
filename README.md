# 前言
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;ESP32-C3模组是4月初发布上线的一款双模(==2.4GWiFi+BLE5.0==)的通信模块，博主手上的是一款外置2M Flash的型号ESP32-C3F：

  ![在这里插入图片描述](https://img-blog.csdnimg.cn/20210426091709572.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxMTE0MDkyMDc0NQ==,size_16,color_FFFFFF,t_70#pic_center)
  
 **本文是在Linux 开发环境用的是乐鑫的ESP-IDF的<font color=#1e15d9>master<font color=#4D4D4D>分 支的SDK基础上做的二次开发。** 所以需要准备的软件：
 
 &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; Linux 开发环境(当然Windos也是可以的，请参考： [ESP-IDF编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html#get-started-get-prerequisites).)
 
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; ESP-IDF: [master](https://github.com/espressif/esp-idf).(ESP-IDF的使用请参考 [ESP-IDF编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html#linux-macos).)

**注意：** 在进行配置<font color=#e90827>menuconifg<font color=#> 的时候需要把 **Revision** 设置为<font color=#e90827>Rev2<font color=#>![在这里插入图片描述](https://img-blog.csdnimg.cn/20210426104540547.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxMTE0MDkyMDc0NQ==,size_16,color_FFFFFF,t_70)
**硬件准备：**

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;1、ESP32-C3F小开发板

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;2、VB01离线语音模块

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;3、WS2812 RGB灯条

#  一、<font color=#19b2f2>新建工程文件
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; 把ESP-IDF中的一个空例程复制到自己工程目录中，空例程在ESP-IDF中的路径如下：
```c
.../esp=idf/examples/get-started/sample_project
```
主函数文件就在该目录下的：<font color=#e90827>main<font color=#>文件夹中的main.c
![在这里插入图片描述](https://img-blog.csdnimg.cn/20210426114719725.png#pic_center)
# 二、<font color=#1da306>WS2812<font color=#f91454>R<font color=#14f93f>G<font color=#0d2efa>B<font color=#>驱动
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; WS2812的驱动其实乐鑫的ESP-IDF中就做有相关的例程：<font color=#e90827>.../examples/peripherals/rmt/led_strip<font color=#>，这个例程使用了RMT红外驱动，还连接了les_strip 驱动库（库路径：..../examples/common_components/led_strip）。但是那个例程无法驱动WS2812灯条，但是它的库却可以使用，所以要移植一下这个库。
## 1.led_strip 驱动库移植
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;在main文件夹的同级目录下创建一个名为<font color=#e90827>**compornents**<font color=#>的文件夹：
```c
mkidr compornents
```
![在这里插入图片描述](https://img-blog.csdnimg.cn/2021042611570337.png#pic_center)
然后把<font color=#e90827>**/examples/common_components/** <font color=#>下的 **led_strip** 文件夹全部复制到==刚刚创建的compornents文件夹中== 。并且在main.c中引用以下头文件：
```c
#include "driver/rmt.h"
#include "led_strip.h"
#include "esp_system.h"
#include "esp_log.h"
```
## 2.初始化WS2812 灯条
定义RMT发送管脚和频道及灯珠个数的宏
```
#define RMT_TX_NUM 3  //发送口
#define RMT_TX_CHANNEL RMT_CHANNEL_0//发送频道
#define LED_STRIP_NUM 24//灯珠数量
```
```c
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
}
```
## 3.设置RGB颜色的函数
缓存颜色的结构体
```json
struct WS2812_COLOR{
uint32_t red;
uint32_t green ;
uint32_t blue;
};

struct WS2812_COLOR WS2812_RGB;
```
```c
void set_rgb(uint16_t Red, uint16_t Green, uint16_t Blue)
{
	for (int i = 0; i < LED_STRIP_NUM; i++)
	{
		strip->set_pixel(strip, i, Red, Green, Blue);//设置颜色
	}
	WS2812_RGB.red = Red;
	WS2812_RGB.green = Green;
	WS2812_RGB.blue = Blue;
	strip->refresh(strip, 10);
}
```

# 三、<font color=#1558ee>UART 串口驱动与VB01通信
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;UART通信同样也可以使用ESP-IDF的example，参考的例程在：**examples/peripherals/uart/uart_async_rxtxtasks** 目录之下。这个例程使用了**FreeRTOS**来管理UART的发送函数和接收函数，博主也照样使用了**freeRTOS** ，<font color=#999AAA >UART驱动这里只是使用了**FreeRTOS** 创建任务，并没有使用太多**FreeRTOS**的各种功能，所以对于不太熟悉**FreeRTOS** 的同学也不要怕。<font color=#>
## 1.串口初始化配置
同样的，先要引用一下相关头文件
```
/*********RTOS Handle-file****************/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
/*********UART Handle-file*****************/
#include "driver/gpio.h"
#include "driver/uart.h"
```
串口初始化配置函数：
```c
void UART_Init(void){
	const uart_config_t uart_config = {
		.baud_rate =38400,  //波特率
		.data_bits = UART_DATA_8_BITS,//数据位
		.parity = UART_PARITY_DISABLE,//校验位
		.stop_bits = UART_STOP_BITS_1,//停止位
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,//流控制
		.source_clk = UART_SCLK_APB,//时钟
	};
	// We won't use a buffer for sending data.
	uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
	uart_param_config(UART_NUM_1, &uart_config);
	uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}
```
## 2.串口接收任务创建
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;在主函数中初始化UART之后，就创建串口的接收数据的任务，提高数据接收的速度：
```c
//开启串口接收任务
xTaskCreate(uartRxTask, "uart_rx_task", 1024*3, NULL, 4, NULL);
```
**任务函数:**
```c
static void uartRxTask(void *arg)
{
	static const char *RX_TASK_TAG = "RX_TASK";
	esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
	uint8_t *data=(uint8_t *)malloc(RX_BUF_SIZE+1);
	
	while (1) {
		const int rxBytes = uart_read_bytes(UART_NUM_1, data, RX_BUF_SIZE, 1000 / portTICK_RATE_MS);
		if (rxBytes > 0) {
			data[rxBytes] = 0;
			ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);
			memset(cJson_data, 0, sizeof(cJson_data));
			uartControlLedStrip(uartDataHandle(data));	//设置灯条颜色	
			ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);
		}
	}
	free(data);
}
```
<font color=#10bcba>**说明**：<font color=#>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;uartControlLedStrip函数和uartDataHandle函数是博主自己写一个串口数据处理函数，目的是把串口接收的数据中提取控制RGB的指令或者数据，各位同学可通过自己的需求去写这样的函数，比如提取指令ID号，只是用一些字符串操作的函数就可以了。
# 四、<font color=#d1260a>WiFi 配置连接
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;WiFi的配置连接极为简单，不需要些配置函数，也不需要写初始化函数，需要连接的WiFi名称和密码都可以通过<font color=#d1260a>menuconfig<font color=#>来配置。因为ESP-IDF的例程中，已经写好了相关的配置文件，只需要简单的修改就可以配置连接WiFi。
## 1.引入头文件并使用几个函数
### (1)头文件的引用
```
/**********WiFI Handle-file**************/
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
```
### (2)在main()中使用以下函数
```c
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
	
	ESP_ERROR_CHECK(esp_netif_init());
	/* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
	 * Read "Establishing Wi-Fi or Ethernet Connection" section in
	 * examples/protocols/README.md for more information about this function.
	 */
	ESP_ERROR_CHECK(example_connect());	
```
## 2.修改CMakefile.txt 文件
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;在工程文件目录下，打开**CMakefile**文件并加入以下内容：
```c
set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/protocol_examples_common)
```
![在这里插入图片描述](https://img-blog.csdnimg.cn/20210426145021662.png)
## 3.运行idf.py menuconfig
![在这里插入图片描述](https://img-blog.csdnimg.cn/20210426145353714.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxMTE0MDkyMDc0NQ==,size_16,color_FFFFFF,t_70)
![在这里插入图片描述](https://img-blog.csdnimg.cn/20210426145509125.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxMTE0MDkyMDc0NQ==,size_16,color_FFFFFF,t_70)
```c
idf.py flash //烧录程序进ESP32C3
idf.py monitor //监控串口
```
获取到IP地址，连接成功。
![在这里插入图片描述](https://img-blog.csdnimg.cn/20210426150159818.png)

# 五、<font color=#b9bf0b>MQTT 配置连接阿里云
## 1.阿里云接入要点
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;本项目是使用阿里云远程控制RGB灯条，所以需要在阿里云物联网平台创建响应的产品和设备。创建方式请参考： [阿里云创建产品与设备](https://help.aliyun.com/document_detail/73705.html?spm=a2c4g.11186623.6.579.56c6c5dbZfdp5g).
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;除此之外，还需要了解阿里云连接所需要的三元组与MQTT客户端和密码的关系。这个请参考：[安信可ESP32-S 模组AT固件连接阿里云物联网平台使用文档](https://aithinker.blog.csdn.net/article/details/114983203).中的==一机一密接入==。

## 2.配置MQTT接入阿里云物联网平台
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;博主是使用创建**RTOS任务**的方式来配置MQTT，因为在配置的过程中需要引入==MQTT事件回调函数==，该函数可以负责处理各个事件所要执行的动作，比如MQTT连接成功事件：MQTT_EVENT_CONNECTED；订阅成功事件：MQTT_EVENT_SUBSCRIBED等。当然收到数据事件也在其中。
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;当MQTT有一个事件被触发的时候，就会跳到事件回调函数去执行相关事件下的动作。所以创建任务来管理是很有必要的。
### (1)头文件
```
/***********MQTT Handle-file************/
#include "mqtt_client.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
```

### (2) MQTT配置

```c
void TaskXMqttRecieve(void *p)
{
	//连接的配置参数
#if MQTT_LINK_Aliyum
	sprintf(MQTT_LINK_USERNAME,"%s&%s",MQTT_DeviceName,MQTT_ProductKey);
#endif
	esp_mqtt_client_config_t mqtt_cfg = {
		.host = MQTT_LINK_HOST, 		//连接的域名 ，请务必修改为您的
		.port = MQTT_LINK_PORT,              //端口，请务必修改为您的
		.username = MQTT_LINK_USERNAME,       //用户名，请务必修改为您的
		.password = MQTT_LINK_PASS,   //密码，请务必修改为您的
		.client_id = MQTT_LINK_CLIENT_ID,
		.event_handle = MqttCloudsCallBack, //设置回调函数
		.keepalive = 120,                   //心跳
		.disable_auto_reconnect = false,    //开启自动重连
		.disable_clean_session = false,     //开启 清除会话
	};
	client = esp_mqtt_client_init(&mqtt_cfg);
	esp_mqtt_client_start(client);

	vTaskDelete(NULL);
}
```
**博主自己定义的宏**
```c
#define MQTT_LINK_HOST 		"www.baidu.com" //域名：改成自己的域名
#define MQTT_LINK_PORT 		1883//端口
//是否开启发布 1 为开启发布 0：关闭发布
/***** 阿里云设备三元组 ************/
#define MQTT_DeviceName "xxxxx"//设备名:改成你们自己的域名
#define MQTT_ProductKey "xxxx"//
char MQTT_LINK_USERNAME[128];
#define MQTT_LINK_PASS 		"xxxxxxxxxxxxx"  //加过hmacmd5加密后的密码
#define MQTT_LINK_CLIENT_ID	"改成自己的设备名|securemode=3,signmethod=hmacmd5|"//客户端ID 阿里云
```
**域名查看方式：<font color=#d1260a>注意看鼠标箭头**
![在这里插入图片描述](https://img-blog.csdnimg.cn/2021042615421567.gif#pic_center)==只有用户名，密码，MQTT客户端及域名都正确才能连上阿里云，物联网平台的设备才会是在线状态==
![在这里插入图片描述](https://img-blog.csdnimg.cn/20210426155056821.png)
如果一直不在线，请仔细阅读：[安信可ESP32-S 模组AT固件连接阿里云物联网平台使用文档](https://aithinker.blog.csdn.net/article/details/114983203).中的==一机一密接入==。
![在这里插入图片描述](https://img-blog.csdnimg.cn/20210426155258699.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxMTE0MDkyMDc0NQ==,size_16,color_FFFFFF,t_70)
## 3.主题订阅
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;如果通过阿里云控制设备，就必须==订阅阿里云的设备Topic==，
先查看以下阿里云的产品 ——>==Topic类列表==——>**属性Topic** 中的**属性设置**。
![在这里插入图片描述](https://img-blog.csdnimg.cn/20210426155923970.gif#pic_center)
### Topic：/sys/==g6oroz0VLBN==/==${deviceName}==/thing/service/property/set
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; 这个Topic中，
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;**“g6oroz0VLBN”** 是产品的：ProductKey
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;**"${deviceName}"** 是设备名：DeviceName
比如现有有以下设备信息：ProductKey = abcdf12；DeviceName=asdvsa。那么该Topic就是：**/sys/==abcdf12==/==asdvsa==/thing/service/property/set。**
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;订阅主题的代码:
```c
	sprintf(MqttTopicSub, "/sys/%s/%s/thing/service/property/set",MQTT_ProductKey,MQTT_DeviceName);
	ESP_LOGI(TAG, "MqttTopicSub: %s", MqttTopicSub);
	//ESP_LOGI(TAG, "MqttTopicPub: %s", MqttTopicPub);
	ESP_ERROR_CHECK(esp_event_loop_create_default());
```
## 4.事件回调函数中的数据接收事件
```c
case MQTT_EVENT_DATA:
			{
				printf("TOPIC=%.*s \r\n", event->topic_len, event->topic);
				printf("DATA=%.*s \r\n\r\n", event->data_len, event->data);
				//发送数据到队列
				struct __User_data *pTmper;
				memset(&user_data,0,sizeof(struct __User_data));
				sprintf(user_data.allData, "%.*s",event->data_len,event->data);
				pTmper = &user_data;
				user_data.dataLen = event->data_len;
				//把数据发送到消息队列
				xQueueSend(ParseJSONQueueHandler, (void *)&pTmper, portMAX_DELAY);
				break;
			}
```
## 5.消息队列处理函数
```c
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
		cJSON *pJsonParams = cJSON_GetObjectItem(pJsonRoot,"params");
		if(pJsonParams==NULL){
			printf("pJsonParams NULL\r\n");
			goto __cJSON_Delete;
		}
		cJSON *pJsonGRB =cJSON_GetObjectItem(pJsonParams,"RGB");
		if(pJsonGRB==NULL) goto __cJSON_Delete;

		cJSON *pJSON_Item_Red = cJSON_GetObjectItem(pJsonGRB, "R");
		cJSON *pJSON_Item_Gree = cJSON_GetObjectItem(pJsonGRB, "G");
		cJSON *pJSON_Item_Blue = cJSON_GetObjectItem(pJsonGRB, "B");
		//设置RGB颜色
		set_rgb(pJSON_Item_Red->valueint, pJSON_Item_Gree->valueint, pJSON_Item_Blue->valueint);

		//播放提示音
		if(WS2812_RGB.red!=0&&WS2812_RGB.green!=0&&WS2812_RGB.blue!=0);
		else uart_write_bytes(UART_NUM_1,"AT+PLAY=21\r\n",13);//播放：已经打开灯了

		if(WS2812_RGB.red!=254&&WS2812_RGB.green!=254&&WS2812_RGB.blue!=254)goto __cJSON_Delete;
		else uart_write_bytes(UART_NUM_1,"AT+PLAY=22\r\n",13);//播放：已经关灯了
		
__cJSON_Delete:
		cJSON_Delete(pJsonRoot);
	}
}
```
# 六、相关文档
## 感谢
## 1.[半颗心脏](https://xuhong.blog.csdn.net/)的[【微信小程序控制硬件15 】 开源一个微信小程序，支持蓝牙快速配网+WiFi双控制ESP32-C3应用示范；（附带Demo）](https://xuhong.blog.csdn.net/article/details/115612720).
## 2. [ESP-IDF编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32c3/api-reference/index.html).
## 3.[ESP-IDF源码地址](https://github.com/espressif/esp-idf.git):https://github.com/espressif/esp-idf.git



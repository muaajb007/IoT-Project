#ifndef __EY_IOT_H__
#define __EY_IOT_H__

//------------------------------------------------------------------------------
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <stdarg.h> // **IMP** Variadic Functions

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "lwip/apps/sntp.h"

#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_types.h"
#include "esp_event_loop.h"
#include "esp_err.h"
#include "esp_spiffs.h"
#include "esp_sleep.h"
#include "esp_adc_cal.h"
#include "esp_vfs_fat.h"
#include "tcpip_adapter.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"

#include "driver/periph_ctrl.h"
#include "driver/timer.h"
#include <driver/rmt.h>
#include "driver/adc.h"
#include "driver/sdmmc_host.h"
#include "driver/gpio.h"
#include <soc/rmt_reg.h>
#include "soc/timer_group_struct.h"
#include <sys/unistd.h>
#include <sys/stat.h>
#include <sys/param.h>
#include "nvs.h"
#include "nvs_flash.h"

#include "mqtt_client.h"
#include "ey_iot_config.h"

//------------------------------------------------------------------------------
// Macros for coloured std-output
#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define WHT   "\x1B[37m"
#define RESET "\x1B[0m"


// Macros for Wi-Fi
#define WIFI_MAXIMUM_RETRY	CONFIG_ESP_MAXIMUM_RETRY
#define WIFI_CONNECTED_BIT 	BIT0
#define WIFI_FAIL_BIT      	BIT1


// Macros for HTTP
#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048

// Macros for ISP Location
#define LOC_PUBLIC_IP_URL	"http://api.ipify.org/?format=json"
#define LOC_ISP_DETAILS_URL "http://ip-api.com/json/"

// Macros for Database
#define GOOGLE_SHEET 1
#define OPEN_WEATHER 2


// Macros for Events
#define EY_WIFI_EVENT_STA_START			1
#define EY_WIFI_EVENT_STA_DISCONNECTED	2

#define EY_MQTT_EVENT_CONNECTED 		3
#define EY_MQTT_EVENT_DISCONNECTED 		4
#define EY_MQTT_EVENT_SUBSCRIBED 		5
#define EY_MQTT_EVENT_UNSUBSCRIBED  	6
#define EY_MQTT_EVENT_PUBLISHED 		7
#define EY_MQTT_EVENT_DATA_RX 			8
#define EY_MQTT_EVENT_ERROR   			9
#define EY_MQTT_EVENT_MISC   			10


//------------------------------------------------------------------------------
int ey_mqtt_event;
char ey_mqtt_sub_topic[EY_SUB_TOPIC_LEN];
char ey_mqtt_sub_data[EY_SUB_DATA_LEN];


//------------------ The Struct -----------------
typedef struct {
	int event_id;

	// Function Pointers for Callbacks
	void (*fn_ptr_WIFI_EVENT_STA_START)();
	void (*fn_ptr_WIFI_EVENT_STA_DISCONNECTED)();

	void (*fn_ptr_MQTT_EVENT_CONNECTED)();
	void (*fn_ptr_MQTT_EVENT_DISCONNECTED)();
	void (*fn_ptr_MQTT_EVENT_SUBSCRIBED)();
	void (*fn_ptr_MQTT_EVENT_UNSUBSCRIBED)();
	void (*fn_ptr_MQTT_EVENT_PUBLISHED)();
	void (*fn_ptr_MQTT_EVENT_DATA_RX)();
	void (*fn_ptr_MQTT_EVENT_ERROR)();
	void (*fn_ptr_MQTT_EVENT_MISC)();

	void (*fn_ptr_OTA_EVENT_SUCCESS)();
	void (*fn_ptr_OTA_EVENT_FAIL)();

	// ISP Location
	int isp_loc_status;
	char isp_country[50];
	char isp_countryCode[10];
	char isp_region[10];
	char isp_regionName[50];
	char isp_city[50];
	char isp_zip[50];
	char isp_lat[50];
	char isp_lon[50];
	char isp_timezone[50];
	char isp_isp[50];
	char isp_org[50];
	char isp_as[50];
	char isp_public_ip[50];

} ey_iot_t;

extern ey_iot_t ey_iot;
extern char alert_msg[500];


//------------------------------------------------------------------------------
int ey_callback_null_fn(void);
void ey_callback_fn(void (*ptr)());
int ey_register_callback(unsigned char event_id, void (*user_fn_ptr)());

int ey_init_nvs(void);
void ey_wifi_auto_reconnect(void);
void ey_wifi_sta_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void ey_init_wifi_sta(char str_ssid[], char str_pass[]);

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event);
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
void ey_mqtt_start(void);

int ey_mqtt_publish(char str_topic[], char str_data[], unsigned char qos);
int ey_mqtt_subscribe(char str_topic[], unsigned char qos);
int ey_mqtt_unsubscribe(char str_topic[]);

const char* ey_xor_encryptDecrypt(char inpString[]);

void initialize_sntp(void);
void ey_init_ntp(void);
time_t ey_sntp_get_epoch(void);
char* ey_sntp_date_string(void);


esp_err_t _http_event_handler(esp_http_client_event_t *evt);

void ey_helper_trigger_http_request(const char *str_url);
int ey_post_sensor_data(unsigned char mode, char sheet_name[50], int number_of_values, ...);
int ey_delete_sensor_data(unsigned char mode, char sheet_name[50], char param[50]);
int ey_put_sensor_data(unsigned char mode, char sheet_name[50], char current[50], char new[50]);
int ey_get_sensor_data(unsigned char mode, char sheet_name[50], char header_name[50], char param[50]);


const char *get_json_value(char* JSON_data, char value[]);
char* ey_get_public_ip(void);
char* ey_get_location_isp(void);
void ey_task_populate_isp_location(void *pvParameters);
void ey_populate_isp_location(void);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#endif /* __EY_IOT_H__ */
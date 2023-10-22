/* 
TITLE	: UART Communication: ESP32 & ATmega 2560
DATE	: 2019/11/12
AUTHOR	: e-Yantra Team
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "tcpip_adapter.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>


#include "driver/uart.h"
#include "driver/gpio.h"

#define BUF_SIZE (1024)

//------------------------------------------------------------------------------
#include "ey_iot.c"
#include "ey_iot_config.h"

//------------------------------------------------------------------------------
#define SUB_TOPIC   "notify/me"

//------------------------------------------------------------------------------
void init_callback(void);
void my_wifi_start(void);
void my_wifi_disconnect(void);
void my_mqtt_rx(void);
void my_mqtt_disconnect(void);
void my_task_alert_msg_rcv(void *p);

//------------------------------------------------------------------------------



//------------------------------------------------------------------------------
void init_callback(void){
	
	ey_register_callback(EY_WIFI_EVENT_STA_START, my_wifi_start);
	ey_register_callback(EY_WIFI_EVENT_STA_DISCONNECTED, my_wifi_disconnect);

	ey_register_callback(EY_MQTT_EVENT_DATA_RX, my_mqtt_rx);

}


void my_wifi_start() { 
	printf("USER: Wi-Fi Start\n");
}


void my_wifi_disconnect() { 
	printf("USER: Wi-Fi Disconnect\n");
	ey_wifi_auto_reconnect();
}


void my_mqtt_rx(){
	printf("SUB_TOPIC: %s\n", ey_mqtt_sub_topic);
    printf("SUB_DATA: %s\n", ey_mqtt_sub_data);
}

void my_mqtt_disconnect(){
	printf("USER: MQTT Disconnect\n");
	ey_wifi_auto_reconnect();
}


void my_task_alert_msg_rcv(void *p)
{
    long unsigned int start_time = 0, current_time = 0;

    start_time = ey_sntp_get_epoch();
    int i=0;
    // char fan_msg[20];
    char fan_msg[20], light_msg[20];

    gpio_set_direction(GPIO_NUM_25, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_NUM_26, GPIO_MODE_OUTPUT);

    while(1)
    {
        current_time = ey_sntp_get_epoch();

        if((current_time - start_time) >= 10) {

            if ( ey_mqtt_sub_data[0] != '\0' ) {

                /****** ADD YOUR CODE HERE ******/
                    /****** ADD YOUR CODE HERE ******/

                // Parse the received MQTT message
                if (strstr(ey_mqtt_sub_data, "FAN on | LED on"))
                {
                    printf("Received: FAN on | LED on\n");
                    gpio_set_level(GPIO_NUM_25, 1); // Turn LED ON
                    vTaskDelay(5000 / portTICK_RATE_MS);
                    gpio_set_level(GPIO_NUM_26, 1); // Turn FAN ON
                }
                else if (strstr(ey_mqtt_sub_data, "FAN on | LED off"))
                {
                    printf("Received: FAN on | LED off\n");
                    gpio_set_level(GPIO_NUM_26, 1); // Turn FAN ON
                    vTaskDelay(5000 / portTICK_RATE_MS);
                    gpio_set_level(GPIO_NUM_25, 0); // Turn LED OFF
                }
                else if (strstr(ey_mqtt_sub_data, "FAN off | LED on"))
                {
                    printf("Received: FAN off | LED on\n");
                    gpio_set_level(GPIO_NUM_26, 0); // Turn FAN OFF
                    vTaskDelay(5000 / portTICK_RATE_MS);
                    gpio_set_level(GPIO_NUM_25, 1); // Turn LED ON
                }
                else if (strstr(ey_mqtt_sub_data, "FAN off | LED off"))
                {
                    printf("Received: FAN off | LED off\n");
                    gpio_set_level(GPIO_NUM_26, 0); // Turn FAN OFF
                    vTaskDelay(5000 / portTICK_RATE_MS);
                    gpio_set_level(GPIO_NUM_25, 0); // Turn LED OFF
                }

                ey_mqtt_sub_data[0] = '\0'; // Clear the received data buffer



		        /*******************************/
                
            }

            start_time = current_time;
        }
    }
}

void app_main()
{
    init_callback();
    ey_init_nvs();
    ey_init_wifi_sta(EY_CONFIG_WIFI_SSID, EY_CONFIG_WIFI_PASS);
    ey_init_ntp();
    ey_mqtt_start();
    ey_mqtt_subscribe(SUB_TOPIC, 0);
    xTaskCreate(&my_task_alert_msg_rcv, "my_task_alert_msg_rcv", 8120, NULL, 10, NULL);
}
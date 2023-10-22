//------------------------------------------------------------------------------
#include "ey_iot.c"
#include "ey_iot_config.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include <stdlib.h>

#include "DHT11.h"

//------------------------------------------------------------------------------
#define PUB_TOPIC   "notify/me"
#define BUF_SIZE (150)



//------------------------------------------------------------------------------
// Global variables, sensor data and actuator data
static esp_adc_cal_characteristics_t adc1_chars;

//------------------------------------------------------------------------------
void init_callback(void);
void my_wifi_start(void);
void my_wifi_disconnect(void);
void my_mqtt_disconnect(void);

void rcv_sensor_data(void *arg)
{
       
    DHT11_init(GPIO_NUM_22);

    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 0, &adc1_chars);
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11);

    char temp[sizeof(int)], humid[sizeof(int)], ldr[sizeof(int)];

    while (1)
    {   
        int temperature = DHT11_read().temperature;     // read temperature using dht
        int humidity = DHT11_read().humidity;           // read humidity using dht
        int ldr_value = adc1_get_raw(ADC1_CHANNEL_7);   // read ldr values
        
        snprintf(temp, sizeof(temp), "%d", temperature);
        snprintf(humid, sizeof(humid), "%d", humidity);
        snprintf(ldr, sizeof(ldr), "%d", ldr_value);

        // ey_post_sensor_data(GOOGLE_SHEET, "Sensor_1", 6, "temperature", temp, "humidity", humid, "ldr_value", ldr);
        // vTaskDelay(500 / portTICK_RATE_MS);

        /******************************************************/
        // Check the threshold values of temperature and LDR
        if (temperature > 50 && ldr_value > 200)
        {
            // Publish an alert message to turn on the fan
            char ey_http_url_buffer = "FAN on | LED on";
            ey_mqtt_publish(PUB_TOPIC, "FAN on | LED on", 0);
        }
        else if (temperature > 50 && ldr_value < 200)
        {
            // Publish an alert message to turn on the LED
            char ey_http_url_buffer = "FAN on | LED off";
            ey_mqtt_publish(PUB_TOPIC, "FAN on | LED off", 0);
        }
         else if (temperature <= 50 && ldr_value > 200)
        {
            // Publish an alert message to turn on the LED
            char ey_http_url_buffer = "FAN off | LED on";
            ey_mqtt_publish(PUB_TOPIC, "FAN off | LED on", 0);
        }
         else if (temperature <= 50 && ldr_value < 200)
        {
            // Publish an alert message to turn on the LED
            char ey_http_url_buffer = "FAN off | LED off";
            ey_mqtt_publish(PUB_TOPIC, "FAN off | LED off", 0);
        }                                                                                                                   

		/******************************************************************/
        

        ey_post_sensor_data(GOOGLE_SHEET, "Sensor_1", 6, "temperature", temp, "humidity", humid, "ldr_value", ldr);
        vTaskDelay(500 / portTICK_RATE_MS);

        if (http_data_buffer[0] != '\0') 
        {
            printf("alert rcvd: %s", ey_http_url_buffer);
            ey_http_url_buffer[0] = '\0';
        }
    }
}

//------------------------------------------------------------------------------
void app_main(void){
	init_callback();
	ey_init_nvs();
    ey_init_wifi_sta(EY_CONFIG_WIFI_SSID, EY_CONFIG_WIFI_PASS);
    ey_mqtt_start();
    xTaskCreate(rcv_sensor_data, "uart_rcv_task_and_publish", 8192, NULL, 10, NULL);
}


//------------------------------------------------------------------------------
void init_callback(void){
	
	ey_register_callback(EY_WIFI_EVENT_STA_START, my_wifi_start);
	ey_register_callback(EY_WIFI_EVENT_STA_DISCONNECTED, my_wifi_disconnect);
}


void my_wifi_start() { 
	printf("USER: Wi-Fi Start\n");
}


void my_wifi_disconnect() { 
	printf("USER: Wi-Fi Disconnect\n");
	ey_wifi_auto_reconnect();
}


void my_mqtt_disconnect(){
	printf("USER: MQTT Disconnect\n");
	ey_wifi_auto_reconnect();
}

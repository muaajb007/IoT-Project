#ifndef __EY_IOT_CONFIG_H__
#define __EY_IOT_CONFIG_H__

//--------- Wi-Fi --------- 
#define EY_CONFIG_WIFI_SSID      "Redmi9"
#define EY_CONFIG_WIFI_PASS      "12345678"

//--------- MQTT --------- 
#define EY_CONFIG_MQTT_SERVER_URL 	"mqtt://broker.emqx.io"
#define EY_CONFIG_MQTT_SERVER_PORT 	1883

#define EY_SUB_TOPIC_LEN			100
#define EY_SUB_DATA_LEN				100


//--------- Encryption --------- 
#define EY_ENCRYP_XOR_KEY	'e'

//--------- NTP --------- 
#define EY_NTP_SERVER	    "pool.ntp.org"
#define EY_NTP_TIMEZONE     "UTC-5:30"


//--------- Database --------- 
#define SPREADSHEET_ID              "<unique-deployment-id>"

#endif /* __EY_IOT_CONFIG_H__ */

//------------------------------------------------------------------------------
#include "ey_iot.h"
#include "jsmn.h"

//------------------------------------------------------------------------------
static const char *TAG = "eY-IOT";

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
char http_data_buffer[500] = "";

esp_http_client_handle_t http_client; // HTTP Client Object
char ey_http_url_buffer[500];

esp_mqtt_client_handle_t client; // MQTT Client Object

// Initialize Function Pointers to Null Function
ey_iot_t ey_iot = {
	.event_id = 0,

	.fn_ptr_WIFI_EVENT_STA_START = ey_callback_null_fn,
	.fn_ptr_WIFI_EVENT_STA_DISCONNECTED = ey_callback_null_fn,

	.fn_ptr_MQTT_EVENT_CONNECTED = ey_callback_null_fn,
	.fn_ptr_MQTT_EVENT_DISCONNECTED = ey_callback_null_fn,
	.fn_ptr_MQTT_EVENT_SUBSCRIBED = ey_callback_null_fn,
	.fn_ptr_MQTT_EVENT_UNSUBSCRIBED = ey_callback_null_fn,
	.fn_ptr_MQTT_EVENT_PUBLISHED = ey_callback_null_fn,
	.fn_ptr_MQTT_EVENT_DATA_RX = ey_callback_null_fn,
	.fn_ptr_MQTT_EVENT_ERROR = ey_callback_null_fn,
	.fn_ptr_MQTT_EVENT_MISC = ey_callback_null_fn,

	.fn_ptr_OTA_EVENT_SUCCESS = ey_callback_null_fn,
	.fn_ptr_OTA_EVENT_FAIL = ey_callback_null_fn,
};

//------------------------------------------------------------------------------

//--------------- eY-IOT Framework Callbacks -------------------

// NULL Function Callback
int ey_callback_null_fn(void)
{
	return 0;
}

void ey_callback_fn(void (*ptr)())
{
	(*ptr)(); // callback to A
}

int ey_register_callback(unsigned char event_id, void (*user_fn_ptr)()) {

	switch (event_id)
	{

	case EY_WIFI_EVENT_STA_START:
		ey_iot.fn_ptr_WIFI_EVENT_STA_START = user_fn_ptr;
		break;

	case EY_WIFI_EVENT_STA_DISCONNECTED:
		ey_iot.fn_ptr_WIFI_EVENT_STA_DISCONNECTED = user_fn_ptr;
		break;

	case EY_MQTT_EVENT_CONNECTED:
		ey_iot.fn_ptr_MQTT_EVENT_CONNECTED = user_fn_ptr;
		break;
	case EY_MQTT_EVENT_DISCONNECTED:
		ey_iot.fn_ptr_MQTT_EVENT_DISCONNECTED = user_fn_ptr;
		break;

	case EY_MQTT_EVENT_SUBSCRIBED:
		ey_iot.fn_ptr_MQTT_EVENT_SUBSCRIBED = user_fn_ptr;
		break;

	case EY_MQTT_EVENT_UNSUBSCRIBED:
		ey_iot.fn_ptr_MQTT_EVENT_UNSUBSCRIBED = user_fn_ptr;
		break;

	case EY_MQTT_EVENT_PUBLISHED:
		ey_iot.fn_ptr_MQTT_EVENT_PUBLISHED = user_fn_ptr;
		break;

	case EY_MQTT_EVENT_DATA_RX:
		ey_iot.fn_ptr_MQTT_EVENT_DATA_RX = user_fn_ptr;
		break;

	case EY_MQTT_EVENT_ERROR:
		ey_iot.fn_ptr_MQTT_EVENT_ERROR = user_fn_ptr;
		break;

	case EY_MQTT_EVENT_MISC:
		ey_iot.fn_ptr_MQTT_EVENT_MISC = user_fn_ptr;
		break;

	default:
		break;
	}

	return 0;
}

//--------------- NVS ----------------------
int ey_init_nvs(void) {

	// Initialize NVS
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	return ret;
}

//----------------- Wi-Fi --------------------
void ey_wifi_auto_reconnect(void) {

	ESP_LOGW(TAG, "Wi-Fi STA Disconnected");
	ESP_LOGI(TAG, "Wi-Fi STA Trying to Reconnect...");
	esp_wifi_connect();
	xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
}

void ey_wifi_sta_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {

	if (event_id == WIFI_EVENT_STA_DISCONNECTED)
	{
		ey_callback_fn(ey_iot.fn_ptr_WIFI_EVENT_STA_DISCONNECTED);
	}

	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
	{
		ey_callback_fn(ey_iot.fn_ptr_WIFI_EVENT_STA_START);
		esp_wifi_connect();
	}

	// **TODO**: not getting to this event when wifi disconnects going to mqtt disconnect event
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
	{

		ey_callback_fn(ey_iot.fn_ptr_WIFI_EVENT_STA_DISCONNECTED);
	}

	else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
	{
		ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
		ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
		s_retry_num = 0;
		xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
	}

}

void ey_init_wifi_sta(char str_ssid[], char str_pass[]) {

	s_wifi_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(esp_netif_init());

	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
														ESP_EVENT_ANY_ID,
														&ey_wifi_sta_event_handler,
														NULL,
														&instance_any_id));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
														IP_EVENT_STA_GOT_IP,
														&ey_wifi_sta_event_handler,
														NULL,
														&instance_got_ip));

	wifi_config_t wifi_config = {
		.sta = {
			.threshold.authmode = WIFI_AUTH_WPA2_PSK,

			.pmf_cfg = {
				.capable = true,
				.required = false},
		},
	};

	strcpy((char *)wifi_config.sta.ssid, str_ssid);
	strcpy((char *)wifi_config.sta.password, str_pass);

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGI(TAG, "wifi_init_sta finished.");

	EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
										   WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
										   pdFALSE,
										   pdFALSE,
										   portMAX_DELAY);

	if (bits & WIFI_CONNECTED_BIT)
	{
		ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", str_ssid, str_pass);
	}
	else if (bits & WIFI_FAIL_BIT)
	{
		ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", str_ssid, str_pass);
	}
	else
	{
		ESP_LOGE(TAG, "UNEXPECTED EVENT");
	}

	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
	vEventGroupDelete(s_wifi_event_group);
}

//-------------------------- MQTT ------------------------------
static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event) {

	esp_mqtt_client_handle_t client = event->client;
	int msg_id;
	// your_context_t *context = event->context;

	ey_mqtt_event = event->event_id;

	switch (ey_mqtt_event)
	{
	case MQTT_EVENT_CONNECTED:
		ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
		ey_callback_fn(ey_iot.fn_ptr_MQTT_EVENT_CONNECTED);
		break;
	case MQTT_EVENT_DISCONNECTED:
		ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
		ey_callback_fn(ey_iot.fn_ptr_MQTT_EVENT_DISCONNECTED);
		break;

	case MQTT_EVENT_SUBSCRIBED:
		ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
		break;
	case MQTT_EVENT_UNSUBSCRIBED:
		ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
		break;
	case MQTT_EVENT_PUBLISHED:
		ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
		break;
	case MQTT_EVENT_DATA:
		ESP_LOGI(TAG, "MQTT_EVENT_DATA");

		sprintf(ey_mqtt_sub_topic, "%.*s", event->topic_len, event->topic);
		sprintf(ey_mqtt_sub_data, "%.*s", event->data_len, event->data);

		ey_callback_fn(ey_iot.fn_ptr_MQTT_EVENT_DATA_RX);

		break;
	case MQTT_EVENT_ERROR:
		ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
		break;
	default:
		ESP_LOGI(TAG, "Other event id:%d", event->event_id);
		break;
	}
	return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {

	ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
	mqtt_event_handler_cb(event_data);
}

void ey_mqtt_start(void) {

	// TODO pass url, port as arguments
	esp_mqtt_client_config_t mqtt_cfg = {
		.uri = EY_CONFIG_MQTT_SERVER_URL,
		.port = EY_CONFIG_MQTT_SERVER_PORT,
	};


	client = esp_mqtt_client_init(&mqtt_cfg);
	esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
	esp_mqtt_client_start(client);
}

int ey_mqtt_publish(char str_topic[], char str_data[], unsigned char qos) {

	int msg_id = esp_mqtt_client_publish(client, str_topic, str_data, 0, qos, 0);
	ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
	return msg_id;
}

int ey_mqtt_subscribe(char str_topic[], unsigned char qos) {

	while (1)
	{
		if (ey_mqtt_event == MQTT_EVENT_CONNECTED)
		{
			int msg_id = esp_mqtt_client_subscribe(client, str_topic, qos);
			ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
			return msg_id;
		}
		else
		{
			ESP_LOGW(TAG, "Not Connected to MQTT Server. Can't Subscribe. Retrying...");
			vTaskDelay(1000 / portTICK_PERIOD_MS);
		}
	}
}

int ey_mqtt_unsubscribe(char str_topic[]) {

	int msg_id = esp_mqtt_client_unsubscribe(client, str_topic);
	ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
	return msg_id;
}

//------------------------- ENCRPYTION ---------------------
const char *ey_xor_encryptDecrypt(char inpString[]) {

	// Define XOR key
	// Any character value will work
	char xorKey = EY_ENCRYP_XOR_KEY;

	// calculate length of input string
	int len = strlen(inpString);

	// perform XOR operation of key
	// with every caracter in string
	for (int i = 0; i < len; i++)
	{
		inpString[i] = inpString[i] ^ xorKey;
	}

	return inpString;
}

//--------------------- NTP -------------------------
void initialize_sntp(void) {

	ESP_LOGI(TAG, "Initializing SNTP");
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	sntp_setservername(0, EY_NTP_SERVER); // pool.ntp.org
	sntp_init();
}

void ey_init_ntp(void) {

	initialize_sntp();

	// wait for time to be set
	time_t now = 0;
	struct tm timeinfo = {0};
	int retry = 0;
	const int retry_count = 10;
	while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count)
	{
		ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
		vTaskDelay(2000 / portTICK_PERIOD_MS);
		time(&now);
		localtime_r(&now, &timeinfo);
	}

	if (retry == 10)
	{
		esp_restart();
	}
}

time_t ey_sntp_get_epoch(void) {

	time_t now;
	struct tm timeinfo;
	time(&now);
	localtime_r(&now, &timeinfo);
	// Is time set? If not, tm_year will be (1970 - 1900).

	if (timeinfo.tm_year < (2016 - 1900))
	{
		ESP_LOGI(TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");

		// ESP_ERROR_CHECK( nvs_flash_init() );
		// initialise_wifi();
		xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT,
							false, true, portMAX_DELAY);
		initialize_sntp();

		// wait for time to be set
		time_t now = 0;
		struct tm timeinfo = {0};
		int retry = 0;
		const int retry_count = 10;
		while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count)
		{
			ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
			vTaskDelay(2000 / portTICK_PERIOD_MS);
			time(&now);
			localtime_r(&now, &timeinfo);
		}

		esp_restart();

	}

	return time(&now);
}

char *ey_sntp_date_string(void) {

	static char dateString[64];
	char *utcDate;
	time_t now;
	struct tm timeinfo;
	now = ey_sntp_get_epoch();

	localtime_r(&now, &timeinfo);
	// Is time set? If not, tm_year will be (1970 - 1900).
	if (timeinfo.tm_year < (2016 - 1900))
	{
		ESP_LOGI(TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
		ey_init_ntp();
		// update 'now' variable with current time
		time(&now);
	}
	char strftime_buf[64];

	// Set timezone to Indian Standard Time
	setenv("TZ", EY_NTP_TIMEZONE, 1);
	tzset();
	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
	sprintf(dateString, "%02d-%02d-%02d %02d:%02d:%02d", (timeinfo.tm_year - 100),
			(timeinfo.tm_mon + 1),
			timeinfo.tm_mday,
			timeinfo.tm_hour,
			timeinfo.tm_min,
			timeinfo.tm_sec);

	return dateString;
}

//------------------------------ OTA -----------------------------------------
esp_err_t _http_event_handler(esp_http_client_event_t *evt) {

	switch (evt->event_id)
	{
	case HTTP_EVENT_ERROR:
		ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
		break;
	case HTTP_EVENT_ON_CONNECTED:
		ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
		break;
	case HTTP_EVENT_HEADER_SENT:
		ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
		break;
	case HTTP_EVENT_ON_HEADER:
		ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
		break;

	case HTTP_EVENT_ON_DATA:
		ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);


		sprintf(http_data_buffer, "%.*s\n", evt->data_len,  (char*) evt -> data);
		char *tmp;

		tmp = strstr(http_data_buffer,"<HTML>");


		if (!esp_http_client_is_chunked_response(evt->client)){
		printf("Chunked Data:%.*s\r\n", evt->data_len, (char*)evt->data);
		}
		else{
			if(tmp == NULL) {
				printf("[ INFO ] %s\n", http_data_buffer);
			}
		}

		break;
	case HTTP_EVENT_ON_FINISH:
		ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
		break;
	case HTTP_EVENT_DISCONNECTED:
		ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
		break;
	}
	return ESP_OK;
}



//------------------------- OTA Advanced ----------------------
int versionCompare(char *v1, char *v2) {

	// vnum stores each numeric
	// part of version
	int vnum1 = 0, vnum2 = 0;

	// loop untill both string are
	// processed
	for (int i = 0, j = 0; (i < strlen(v1) || j < strlen(v2));)
	{
		// storing numeric part of
		// version 1 in vnum1
		while (i < strlen(v1) && v1[i] != '.')
		{
			vnum1 = vnum1 * 10 + (v1[i] - '0');
			i++;
		}

		// storing numeric part of
		// version 2 in vnum2
		while (j < strlen(v2) && v2[j] != '.')
		{
			vnum2 = vnum2 * 10 + (v2[j] - '0');
			j++;
		}

		if (vnum1 > vnum2)
			return 1;
		if (vnum2 > vnum1)
			return -1;

		// if equal, reset variables and
		// go for next numeric part
		vnum1 = vnum2 = 0;
		i++;
		j++;
	}
	return 0;
}

int ey_helper_ota_version_compare(char *version1, char *version2) {

	if (versionCompare(version1, version2) < 0)
	{
		printf("%s < %s\n", version1, version2);
		return 2;
	}

	else if (versionCompare(version1, version2) > 0)
	{
		printf("%s > %s\n", version1, version2);
		return 1;
	}

	else
	{
		printf("v1 == v2\n");
		return 0;
	}
}

// ------------------------ GOOGLE SHEET ----------------------
/* @brief: Function to trigger an HTTP request
 * @param: url, the URL to which an HTTP request is to be sent
 * @retval:
 */
void ey_helper_trigger_http_request(const char *str_url) {

	esp_http_client_config_t config = {.url = str_url, .event_handler = _http_event_handler};
	http_client = esp_http_client_init(&config);
	// GET
	esp_err_t err = esp_http_client_perform(http_client);

	if (err == ESP_OK)
	{
		ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d",
				 esp_http_client_get_status_code(http_client),
				 esp_http_client_get_content_length(http_client));
	}
	else
	{
		ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
	}

	esp_http_client_cleanup(http_client);
}


// convert sensor values to float before passing 10 -> 10.00
int ey_post_sensor_data(unsigned char mode, char sheet_name[50], int number_of_values, ...) {

	char sensor_value_buffer[100];

	va_list ap;
	va_start(ap, number_of_values);

	// Logic
	switch (mode)
	{

	case GOOGLE_SHEET:
		sprintf(ey_http_url_buffer, "https://script.google.com/macros/s/" SPREADSHEET_ID "/exec?id=%s&method=POST",sheet_name);

		for (int i = 1; i <= number_of_values; i=i+2)
		{
			sprintf(sensor_value_buffer, "&%s=%s", va_arg(ap, char *), va_arg(ap, char *));
			strcat(ey_http_url_buffer, sensor_value_buffer);

			bzero(sensor_value_buffer, strlen(sensor_value_buffer));
		}

		printf("%s\n", ey_http_url_buffer);
		ey_helper_trigger_http_request(ey_http_url_buffer);

		break;

		// **TODO** case Local File:

	default:
		break;
	}

	va_end(ap);

	return 0;
}

int ey_delete_sensor_data(unsigned char mode, char sheet_name[50], char param[500]) {

	char sensor_value_buffer;

	switch (mode)
	{
		case GOOGLE_SHEET:
			sprintf(ey_http_url_buffer, "https://script.google.com/macros/s/" SPREADSHEET_ID "/exec?id=%s&method=DELETE&value1=%s", sheet_name, param);
			printf("%s\n", ey_http_url_buffer);
			ey_helper_trigger_http_request(ey_http_url_buffer);

			break;

		default:
			break;
	}

	return 0;
}

int ey_put_sensor_data(unsigned char mode, char sheet_name[50], char current[50], char new[50]) {

	char sensor_value_buffer;

	switch (mode)
	{

	case GOOGLE_SHEET:

		sprintf(ey_http_url_buffer, "https://script.google.com/macros/s/" SPREADSHEET_ID "/exec?id=%s&method=PUT&current=%s&new=%s", sheet_name, current, new);
		printf("%s\n", ey_http_url_buffer);
		ey_helper_trigger_http_request(ey_http_url_buffer);
		break;

	default:
		break;
	}

	return 0;
}


int ey_get_sensor_data(unsigned char mode, char sheet_name[50], char header_name[50], char param[50]) {

	switch (mode)
	{
		case GOOGLE_SHEET:
			sprintf(ey_http_url_buffer, "https://script.google.com/macros/s/" SPREADSHEET_ID "/exec?id=%s&method=GET&%s=%s", sheet_name, header_name, param);
			printf("%s\n", ey_http_url_buffer);
			ey_helper_trigger_http_request(ey_http_url_buffer);
			break;

		case OPEN_WEATHER:
			sprintf(ey_http_url_buffer, "https://api.openweathermap.org/data/2.5/weather?q=%s&appid=%s", header_name, param);
			printf("%s\n", ey_http_url_buffer);
			ey_helper_trigger_http_request(ey_http_url_buffer);
			break;

		default:
			break;
	}
	return 0;
}


int addingNumbers(int nHowMany, ...) {

	int nSum = 0;

	va_list intArgumentPointer;

	va_start(intArgumentPointer, nHowMany);
	for (int i = 0; i < nHowMany; i++)
		nSum += va_arg(intArgumentPointer, int);
	va_end(intArgumentPointer);

	return nSum;
}

//----------------------------------- LOCATION -----------------------

static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {

	if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
		strncmp(json + tok->start, s, tok->end - tok->start) == 0)
	{
		return 0;
	}
	return -1;
}

const char *get_json_value(char *JSON_data, char value[]) {

	jsmn_parser p;
	jsmntok_t t[256];
	jsmn_init(&p);
	char *name;
	int r1;
	int i;

	static char str_buffer[1000];

	r1 = jsmn_parse(&p, JSON_data, strlen(JSON_data), t, sizeof(t) / sizeof(t[0]));

	for (i = 1; i < r1; i++)
	{
		if (jsoneq(JSON_data, &t[i], value) == 0)
		{

			name = JSON_data + t[i + 1].start;
			for (int k = 0; k < t[i + 1].end - t[i + 1].start; k++)
			{
				str_buffer[k] = *name++;
			}
			str_buffer[t[i + 1].end - t[i + 1].start] = '\0';
			i++;
		}
	}
	char *rtun = "0";
	return str_buffer;
}

char *ey_get_public_ip(void) {

	static char str_return[100];

	char *buffer = malloc(MAX_HTTP_RECV_BUFFER + 1);
	if (buffer == NULL)
	{
		ESP_LOGE(TAG, "Cannot malloc http receive buffer");
		return;
	}
	// http://api.ipify.org/?format=json
	// http://ip-api.com/json/157.32.130.153
	esp_http_client_config_t config = {
		.url = "http://api.ipify.org/?format=json",
	};
	esp_http_client_handle_t client = esp_http_client_init(&config);
	esp_err_t err;
	if ((err = esp_http_client_open(client, 0)) != ESP_OK)
	{
		ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
		free(buffer);
		return;
	}
	int content_length = esp_http_client_fetch_headers(client);
	int total_read_len = 0, read_len;
	if (total_read_len < content_length && content_length <= MAX_HTTP_RECV_BUFFER)
	{
		read_len = esp_http_client_read(client, buffer, content_length);
		if (read_len <= 0)
		{
			ESP_LOGE(TAG, "Error read data");
		}
		buffer[read_len] = 0;
		ESP_LOGD(TAG, "read_len = %d", read_len);
		strcpy(str_return, get_json_value(buffer, "ip"));
	}
	ESP_LOGI(TAG, "HTTP Stream reader Status = %d, content_length = %d",
			 esp_http_client_get_status_code(client),
			 esp_http_client_get_content_length(client));
	esp_http_client_close(client);
	esp_http_client_cleanup(client);
	free(buffer);

	return str_return;
}

char *ey_get_location_isp(void) {

	static char str_return[100];
	char str_url[100];
	sprintf(str_url, "http://ip-api.com/json/%s", ey_get_public_ip());

	char *buffer = malloc(MAX_HTTP_RECV_BUFFER + 1);
	if (buffer == NULL)
	{
		ESP_LOGE(TAG, "Cannot malloc http receive buffer");
		return;
	}
	// http://api.ipify.org/?format=json
	// http://ip-api.com/json/157.32.130.153
	esp_http_client_config_t config = {
		.url = str_url,
	};
	esp_http_client_handle_t client = esp_http_client_init(&config);
	esp_err_t err;
	if ((err = esp_http_client_open(client, 0)) != ESP_OK)
	{
		ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
		free(buffer);
		return;
	}
	int content_length = esp_http_client_fetch_headers(client);
	int total_read_len = 0, read_len;
	if (total_read_len < content_length && content_length <= MAX_HTTP_RECV_BUFFER)
	{
		read_len = esp_http_client_read(client, buffer, content_length);
		if (read_len <= 0)
		{
			ESP_LOGE(TAG, "Error read data");
		}
		buffer[read_len] = 0;
		ESP_LOGD(TAG, "read_len = %d", read_len);
		// printf("%s\n", buffer);
		// printf("%s\n", get_json_value(buffer, "ip"));
		strcpy(ey_iot.isp_country, get_json_value(buffer, "country"));
		strcpy(ey_iot.isp_countryCode, get_json_value(buffer, "countryCode"));
		strcpy(ey_iot.isp_region, get_json_value(buffer, "region"));
		strcpy(ey_iot.isp_regionName, get_json_value(buffer, "regionName"));
		strcpy(ey_iot.isp_city, get_json_value(buffer, "city"));
		strcpy(ey_iot.isp_zip, get_json_value(buffer, "zip"));
		strcpy(ey_iot.isp_lat, get_json_value(buffer, "lat"));
		strcpy(ey_iot.isp_lon, get_json_value(buffer, "lon"));
		strcpy(ey_iot.isp_timezone, get_json_value(buffer, "timezone"));
		strcpy(ey_iot.isp_isp, get_json_value(buffer, "isp"));
		strcpy(ey_iot.isp_org, get_json_value(buffer, "org"));
		strcpy(ey_iot.isp_as, get_json_value(buffer, "as"));
		strcpy(ey_iot.isp_public_ip, get_json_value(buffer, "query"));
	}
	ESP_LOGI(TAG, "HTTP Stream reader Status = %d, content_length = %d",
			 esp_http_client_get_status_code(client),
			 esp_http_client_get_content_length(client));
	esp_http_client_close(client);
	esp_http_client_cleanup(client);
	free(buffer);

	return str_return;
}

void ey_task_populate_isp_location(void *pvParameters) {

	ESP_LOGI(TAG, "Getting ISP Information\n");
	ey_get_location_isp();
	printf("-----------------------------------------------\n");
	printf("Country: %s\n", ey_iot.isp_country);
	printf("Country Code: %s\n", ey_iot.isp_countryCode);
	printf("Region: %s\n", ey_iot.isp_region);
	printf("Region Name: %s\n", ey_iot.isp_regionName);
	printf("City: %s\n", ey_iot.isp_city);
	printf("Zip: %s\n", ey_iot.isp_zip);
	printf("Latitude: %s\n", ey_iot.isp_lat);
	printf("Longitude: %s\n", ey_iot.isp_lon);
	printf("Timezone: %s\n", ey_iot.isp_timezone);
	printf("ISP: %s\n", ey_iot.isp_isp);
	printf("ORG: %s\n", ey_iot.isp_org);
	printf("AS: %s\n", ey_iot.isp_as);
	printf("Public IP: %s\n", ey_iot.isp_public_ip);
	printf("-----------------------------------------------\n");

	ey_iot.isp_loc_status = 1;
	ESP_LOGI(TAG, "ISP Information Gathered\n");

	// **TODO** Error Handling; ey_iot.isp_loc_status = -1;

	vTaskDelete(NULL);
}

void ey_populate_isp_location(void) {

	xTaskCreate(&ey_task_populate_isp_location, "ey_task_populate_isp_location", 8192, NULL, 5, NULL);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
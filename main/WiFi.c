/**
 * @file WiFi.c
 * @author  gwicks
 * @brief  As the name implies, the WiFi handling for the incubator. For now, will also contain HTML related code.
 * @version 0.1
 * @date 2026-04-06
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  //Requires by memset
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "spi_flash_mmap.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_http_server.h"

#ifdef  LEGACY_I2C
#include "driver/i2c.h"
#else
#include "driver/i2c_master.h"
#endif

#include "nvs_flash.h"
#include "esp_netif.h"
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/api.h>
#include <lwip/netdb.h>

#include "driver/gpio.h"
// #include "ds18b20.h"
#include "mcp9808.h"
#include "lwip/apps/mdns.h"

#include "TempControl.h"
#include "Incubator.h"

extern QueueHandle_t xQueueHttp;
Incubator_URL 	to_send;

static const char *TAG = "WiFi";	//TAG for debug

 
extern int PWR5V_PIN;
extern int	PWR12V_PIN;

/*
// Build http header
const static char http_html_hdr[] =
		"HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n";

// Build 404 header
const static char http_404_hdr[] =
"HTTP/1.1 404 Not Found\r\nContent-type: text/html\r\n\r\n";

// Build http body
const static char http_index_hml[] =
		"<html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\
		<title>Control</title><style>body{background-color:lightblue;font-size:24px;}</style></head>\
		<body><h1>Control</h1><a href=\"high\">ON</a><br><a href=\"low\">OFF</a></body></html>";
 */


// Copy over the station_example_main.c defines, constants vars:
/* The examples use WiFi configuration that you can set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

#if CONFIG_ESP_STATION_EXAMPLE_WPA3_SAE_PWE_HUNT_AND_PECK
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER ""
#elif CONFIG_ESP_STATION_EXAMPLE_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#elif CONFIG_ESP_STATION_EXAMPLE_WPA3_SAE_PWE_BOTH
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#endif
#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif


#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

/**
 * @brief These functions were copied from nopnop2002's esp-idf-pwm-slider github repo. The 
 * Text2Html() is of primary importance, as it allows us to have an exact HTML file to work with.
 * 
 * @param req 
 * @param filename 
 * @return esp_err_t 
 */
static esp_err_t Text2Html(httpd_req_t *req, char * filename) {
	ESP_LOGI(TAG, "Reading %s", filename);
	FILE* fhtml = fopen(filename, "r");
	if (fhtml == NULL) {
		ESP_LOGE(TAG, "fopen fail. [%s]", filename);
		return ESP_FAIL;
	} else {
		char line[128];
		while (fgets(line, sizeof(line), fhtml) != NULL) {
			size_t linelen = strlen(line);
			//remove EOL (CR or LF)
			for (int i=linelen;i>0;i--) {
				if (line[i-1] == 0x0a) {
					line[i-1] = 0;
				} else if (line[i-1] == 0x0d) {
					line[i-1] = 0;
				} else {
					break;
				}
			}
			ESP_LOGD(TAG, "line=[%s]", line);
			if (strlen(line) == 0) continue;
			esp_err_t ret = httpd_resp_sendstr_chunk(req, line);
			if (ret != ESP_OK) {
				ESP_LOGE(TAG, "httpd_resp_sendstr_chunk fail %d", ret);
			}
		}
		fclose(fhtml);
	}
	return ESP_OK;
}

esp_err_t Image2Html(httpd_req_t *req, char * filename, char * type)
{
	FILE * fhtml = fopen(filename, "r");
	if (fhtml == NULL) {
		ESP_LOGE(TAG, "fopen fail. [%s]", filename);
		return ESP_FAIL;
	}else{
		char	buffer[64];

		if (strcmp(type, "jpeg") == 0) {
			httpd_resp_sendstr_chunk(req, "<img src=\"data:image/jpeg;base64,");
		} else if (strcmp(type, "jpg") == 0) {
			httpd_resp_sendstr_chunk(req, "<img src=\"data:image/jpeg;base64,");
		} else if (strcmp(type, "png") == 0) {
			httpd_resp_sendstr_chunk(req, "<img src=\"data:image/png;base64,");
		} else {
			ESP_LOGW(TAG, "file type fail. [%s]", type);
			httpd_resp_sendstr_chunk(req, "<img src=\"data:image/png;base64,");
		}
		while(1) {
			size_t bufferSize = fread(buffer, 1, sizeof(buffer), fhtml);
			ESP_LOGD(TAG, "bufferSize=%d", bufferSize);
			if (bufferSize > 0) {
				httpd_resp_send_chunk(req, buffer, bufferSize);
			} else {
				break;
			}
		}
		fclose(fhtml);
		httpd_resp_sendstr_chunk(req, "\">");
	}
	return ESP_OK;
}

static int s_retry_num = 0;
// Has to implement this event_handler for WIFI station connection
// static esp_err_t event_handler(void *ctx, system_event_t *event) {
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) 
{
    // Replacing with newer station_example_main.c
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) 
	{
        esp_wifi_connect();
    } 
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) 
	{
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } 
	else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) 
	{
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } 
}

void wifi_init_sta(void)
{
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
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (password len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
             * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
            .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

	// apparently, the following is a good practice:
		/* The event will not be processed after unregister */
	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
	vEventGroupDelete(s_wifi_event_group);

}

/* HTTP get handler */
static esp_err_t root_get_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "root_get_handler req->uri=[%s]", req->uri);

	/* Send index.html */
	Text2Html(req, "/html/index.html");

	/* Send Image */
	Image2Html(req, "/html/ESP-LOGO.txt", "png");

	/* Send empty chunk to signal HTTP response completion */
	httpd_resp_sendstr_chunk(req, NULL);

	return ESP_OK;
}

// The HTTP POST handlers
// Update Target temperature
static esp_err_t TargetTemperature_post_handler(httpd_req_t *req)
{
    char buf[10];
    int  ret;
    char outbuf[50];
	ESP_LOGI(TAG, "TargetTemperature_post_handler req->uri=[%s]", req->uri);

    /* Read data received in the request */
    ret = httpd_req_recv(req, buf, sizeof(buf));
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    int val = atoi(buf);

	// Send to http_server_task
	memcpy(to_send.data,"TargetTemp triggered", 32);

	if (xQueueSend(xQueueHttp, &to_send.data[0], portMAX_DELAY) != pdPASS) {
		ESP_LOGE(TAG, "xQueueSend Fail");
	}

	// val contains the number requested by the user from the browser...
	ret = Temp_set_target_temp(val);
	if(ret != ESP_OK)
	{
		httpd_resp_send(req,"Invalid temp value!",HTTPD_RESP_USE_STRLEN );
	}
    snprintf(outbuf, sizeof(outbuf),"%d", val);		// just echo??
    httpd_resp_send(req, outbuf, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;

}

// Update Target temperature range
static esp_err_t TargetTempRange_post_handler(httpd_req_t *req)
{
    char buf[10];
    int  ret;
    char outbuf[50];
	ESP_LOGI(TAG, "TargetTempRange_post_handler req->uri=[%s]", req->uri);

    /* Read data received in the request */
    ret = httpd_req_recv(req, buf, sizeof(buf));
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    int val = atoi(buf);

	// val contains the number requested by the user from the browser...
	ret = Temp_set_target_range(val);
	if(ret != ESP_OK)
	{
		httpd_resp_send(req,"Invalid temp range value!",HTTPD_RESP_USE_STRLEN );
	}
    snprintf(outbuf, sizeof(outbuf),"%d", val);		// just echo??
    httpd_resp_send(req, outbuf, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;

}

/* Function to start the web server */
esp_err_t start_server(const char *base_path, int port)
{
	httpd_handle_t server = NULL;
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	config.server_port = port;

	/* Use the URI wildcard matching function in order to
	 * allow the same handler to respond to multiple different
	 * target URIs which match the wildcard scheme */
	config.uri_match_fn = httpd_uri_match_wildcard;

	ESP_LOGI(TAG, "Starting HTTP Server on port: '%d'", config.server_port);
	if (httpd_start(&server, &config) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to start file server!");
		return ESP_FAIL;
	}

	/* URI handler for get */
	httpd_uri_t _root_get_handler = {
		.uri		 = "/",
		.method		 = HTTP_GET,
		.handler	 = root_get_handler,
		//.user_ctx  = server_data	// Pass server data as context
	};
	httpd_register_uri_handler(server, &_root_get_handler);

#if 0
	/* URI handler for post */
	httpd_uri_t _root_post_handler = {
		.uri		 = "/post",
		.method		 = HTTP_POST,
		.handler	 = root_post_handler,
		//.user_ctx  = server_data	// Pass server data as context
	};
	httpd_register_uri_handler(server, &_root_post_handler);
#endif

	httpd_uri_t _TargetTemperature_post_handler = {
		.uri		 = "/TargetTemperature",
		.method		 = HTTP_POST,
		.handler	 = TargetTemperature_post_handler,
	};
	httpd_register_uri_handler(server, &_TargetTemperature_post_handler);
	
	httpd_uri_t _TargetTempRange_post_handler = {
		.uri		 = "/TargetTempRange",
		.method		 = HTTP_POST,
		.handler	 = TargetTempRange_post_handler,
	};
	httpd_register_uri_handler(server, &_TargetTempRange_post_handler);

#if 0
	/* URI handler for favicon.ico */
	httpd_uri_t _favicon_get_handler = {
		.uri		 = "/favicon.ico",
		.method		 = HTTP_GET,
		.handler	 = favicon_get_handler,
		//.user_ctx  = server_data	// Pass server data as context
	};
	httpd_register_uri_handler(server, &_favicon_get_handler);
#endif
	return ESP_OK;
}

void http_server_task(void *pvParameters)
{
	char 			*task_parameter = (char *)pvParameters;
	char 			url[64];
	Incubator_URL	recvd;

	ESP_LOGI(TAG, "Start task_parameter=%s", task_parameter);
	sprintf(url, "http://%s:%d", task_parameter, CONFIG_WEB_PORT);

	while(1) 
	{
		// Waiting for post - blocks indefinitely...
		if (xQueueReceive(xQueueHttp, &recvd, portMAX_DELAY) == pdTRUE) 
		{
			ESP_LOGI(TAG,"Recvd: %s",recvd.data);
		}
	}

#ifdef	DO_NOPNOP

	// Set the LEDC peripheral configuration
	ledc_init();

	// Set duty to 50%
	double maxduty = pow(2, 13) - 1;
	float percent_red = 0.5;
	uint32_t duty_red = maxduty * percent_red;
	ESP_LOGI(TAG, "duty_red=%"PRIu32, duty_red);
	ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, duty_red));
	// Update duty to apply the new value
	ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0));

	float percent_green = 0.5;
	uint32_t duty_green = maxduty * percent_green;
	ESP_LOGI(TAG, "duty_green=%"PRIu32, duty_green);
	ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, duty_green));
	// Update duty to apply the new value
	ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1));

	float percent_blue = 0.5;
	uint32_t duty_blue = maxduty * percent_blue;
	ESP_LOGI(TAG, "duty_blue=%"PRIu32, duty_blue);
	ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_2, duty_blue));
	// Update duty to apply the new value
	ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_2));

	// Start Server
	ESP_LOGI(TAG, "Starting server on %s", url);
	ESP_ERROR_CHECK(start_server("/spiffs", CONFIG_WEB_PORT));
	
	URL_t urlBuf;
	while(1) {
		// Waiting for post
		if (xQueueReceive(xQueueHttp, &urlBuf, portMAX_DELAY) == pdTRUE) {
			ESP_LOGI(TAG, "str_value_red=%s long_value_red=%ld", urlBuf.str_value_red, urlBuf.long_value_red);
			ESP_LOGI(TAG, "str_value_green=%s long_value_green=%ld", urlBuf.str_value_green, urlBuf.long_value_green);
			ESP_LOGI(TAG, "str_value_blue=%s long_value_blue=%ld", urlBuf.str_value_blue, urlBuf.long_value_blue);

			// Set duty value for red
			percent_red = urlBuf.long_value_red / 100.0;
			duty_red = maxduty * percent_red;
			ESP_LOGI(TAG, "percent_red=%f duty_red=%"PRIu32, percent_red, duty_red);
			ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, duty_red));
			// Update duty to apply the new value
			ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0));

			// Set duty value for green
			percent_green = urlBuf.long_value_green / 100.0;
			duty_green = maxduty * percent_green;
			ESP_LOGI(TAG, "percent_green=%f duty_green=%"PRIu32, percent_green, duty_green);
			ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, duty_green));
			// Update duty to apply the new value
			ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1));

			// Set duty value for blue
			percent_blue = urlBuf.long_value_blue / 100.0;
			duty_blue = maxduty * percent_blue;
			ESP_LOGI(TAG, "percent_blue=%f duty_blue=%"PRIu32, percent_blue, duty_blue);
			ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_2, duty_blue));
			// Update duty to apply the new value
			ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_2));
		}
	}

	// Never reach here
	ESP_LOGI(TAG, "finish");
	vTaskDelete(NULL);
#endif

}

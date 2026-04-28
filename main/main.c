#include <stdio.h>
#include <stdlib.h>
#include <string.h>  //Requires by memset
#include <dirent.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"

#include "esp_log.h"
#include <sys/stat.h>
#include <mbedtls/base64.h>
#include "spi_flash_mmap.h"

#ifdef  LEGACY_I2C
#include "driver/i2c.h"
#else
#include "driver/i2c_master.h"
#endif

#include "esp_netif.h"
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/api.h>
#include <lwip/netdb.h>
#include "driver/gpio.h"
#include "mcp9808.h"

#include "Incubator.h"

QueueHandle_t xQueueHttp;

#define ENABLE_STATIC_IP

#define DO_SPIFFS

static const char *TAG = "Incubator";	//TAG for debug

//static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;

// Essential tasks and interface functions that are external
extern void http_server(void *pvParameters) ;
extern void wifi_init_sta(void);
extern void TempControl(void *pvParameters);
extern void http_server_task(void *pvParameters);


/*----------------------------------------------------------------*/

// MOSFET switches for heating element and fan
extern int   PWR5V_PIN;
extern int	 PWR12V_PIN;

/*----------------------------------------------------------------*/

// Environment handling

extern float CurrentTemp;
extern float desiredTemperature; // Target temperature
extern float hysteresisBand; 
extern float turnOnThreshold;  // = desiredTemperature - hysteresisBand;
extern float turnOffThreshold; 	// = desiredTemperature + hysteresisBand;

/*----------------------------------------------------------------*/


/**
 * @brief I2C Handling - parameters for I2C control.
 * 
 */
#define I2C_MASTER_SCL_IO           CONFIG_I2C_MASTER_SCL       /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           CONFIG_I2C_MASTER_SDA       /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_0                   /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ          CONFIG_I2C_MASTER_FREQUENCY /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000

#ifndef  LEGACY_I2C
	i2c_master_bus_handle_t bus_handle;
	i2c_master_dev_handle_t dev_handle;
#endif

/**
 * @brief i2c master initialization
 */
static void i2c_master_init(void)
{
	ESP_LOGI(TAG, "%s: called", __FILE__);

#ifdef  LEGACY_I2C
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
#else

	i2c_master_bus_config_t i2c_bus_config = {
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.i2c_port = I2C_NUM_0,
		.sda_io_num = GPIO_NUM_21,
		.scl_io_num = GPIO_NUM_22,
		.glitch_ignore_cnt = 7,
	};
	i2c_new_master_bus(&i2c_bus_config, &bus_handle);

	i2c_device_config_t dev_cfg = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = MCP9808_I2CADDR_DEFAULT, 
		.scl_speed_hz = 100000,
		.scl_wait_us = 20000,							// the MCP9808 might need clock stretching.
	};
	i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);

#endif
}


static void listSPIFFS(char * path) {
	ESP_LOGI(TAG,"%s: called, path is %s ",__FILE__,path);
	DIR* dir = opendir(path);
	assert(dir != NULL);
	while (true) {
		struct dirent*pe = readdir(dir);
		if (!pe) break;
		ESP_LOGI(TAG,"d_name=%s/%s d_ino=%d d_type=%x", path, pe->d_name,pe->d_ino, pe->d_type);
	}
	closedir(dir);
}

esp_err_t mountSPIFFS(char * path, char * label, int max_files) {
	esp_vfs_spiffs_conf_t conf = {
		.base_path = path,
		.partition_label = label,
		.max_files = max_files,
		.format_if_mount_failed =true
	};

	ESP_LOGI(TAG,"%s: called",__FILE__);

	// Use settings defined above toinitialize and mount SPIFFS filesystem.
	// Note: esp_vfs_spiffs_register is anall-in-one convenience function.
	esp_err_t ret = esp_vfs_spiffs_register(&conf);

	ESP_LOGI(TAG,"%s: esp_vfs_spiffs_register, ret = %d",__FILE__,ret);

	if (ret != ESP_OK) {
		if (ret ==ESP_FAIL) {
			ESP_LOGE(TAG, "Failed to mount or format filesystem");
		} else if (ret== ESP_ERR_NOT_FOUND) {
			ESP_LOGE(TAG, "Failed to find SPIFFS partition");
		} else {
			ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)",esp_err_to_name(ret));
		}
		return ret;
	}

	size_t total = 0, used = 0;
	ret = esp_spiffs_info(conf.partition_label, &total, &used);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG,"Failed to get SPIFFS partition information (%s)",esp_err_to_name(ret));
	} else {
		ESP_LOGI(TAG,"Partition size: total: %d, used: %d", total, used);
	}

	if (ret == ESP_OK) {
		ESP_LOGI(TAG, "Mount %s to %s success", path, label);
		listSPIFFS(path);
	}
	return ret;
}


/**
 * @brief app_main --- kicks off the NVS Flash initialization,the WiFi initialization, I2C bus init,
 * mDNS, GPIO pins, Temperature controller, and the HTTP Server application
 * 
 */
void app_main() {

	ESP_LOGI(TAG, "%s: called", __FILE__);

	// ESP_ERROR_CHECK(nvs_flash_init());
	// Initialize NVS
	esp_err_t ret = nvs_flash_init();
	ESP_LOGI(TAG,"nvs_flash_init was called");
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_LOGI(TAG,"nvs_flash_init failed - do erase, then flash init");
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_LOGI(TAG, " all done with nvs stuff");
	ESP_ERROR_CHECK(ret);

		
	ESP_LOGI(TAG,"calling wifi_init_sta...");

	wifi_init_sta();		// NEW

	// Init I2C driver
	ESP_LOGI(TAG, "%s: calling i2c_master_init", __FILE__);
    i2c_master_init();

	// Create Queue
	xQueueHttp = xQueueCreate( 10, sizeof(Incubator_URL) );
	configASSERT( xQueueHttp );

#ifdef	DO_SPIFFS
	// Initialize SPIFFS
	ESP_ERROR_CHECK(mountSPIFFS("/html", "storage", 6));
	ESP_ERROR_CHECK(start_server("/spiffs", CONFIG_WEB_PORT));
#endif 

	// TODO: Init ntp protocol (time server) ?? 
	//ntp_app_main();
	
	//GPIO initialization
	ESP_LOGI(TAG, "%s: setting up GPIO", __FILE__);
	esp_rom_gpio_pad_select_gpio(PWR5V_PIN);
	gpio_set_direction(PWR5V_PIN, GPIO_MODE_OUTPUT);
	esp_rom_gpio_pad_select_gpio(PWR12V_PIN);
	gpio_set_direction(PWR12V_PIN, GPIO_MODE_OUTPUT);

	ESP_LOGI(TAG, "%s: starting TempControl ", __FILE__);
	
	// Run the temperature control task
	xTaskCreate(&TempControl, "TempControl", 2048*2, NULL, 4, NULL);

	// Get the local IP address
	esp_netif_ip_info_t ip_info;
	ESP_ERROR_CHECK(esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info));

	char cparam0[64];
	sprintf(cparam0, IPSTR, IP2STR(&ip_info.ip));
	xTaskCreate(http_server_task, "HTTP", 1024*6, (void *)cparam0, 2, NULL);

	// Wait for the task to start, because cparam0 is discarded.
	vTaskDelay(10);

	ESP_LOGI(TAG, "- App is running ... ...\n");

	while(1)
	{
        vTaskDelay(pdMS_TO_TICKS(5000));
	}

}

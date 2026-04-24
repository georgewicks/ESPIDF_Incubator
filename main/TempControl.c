/**
 * @brief Temperature Control.
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  //Requires by memset
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include "esp_log.h"

#ifdef  LEGACY_I2C
#include "driver/i2c.h"
#else
#include "driver/i2c_master.h"
#endif

#include "driver/gpio.h"
#include "mcp9808.h"

#define MIN_TEMP    (10)
#define MAX_TEMP    (30)
#define MIN_RANGE   (1)
#define MAX_RANGE   (10)

float   CurrentTemp;
float   desiredTemperature = 25.0; // Target temperature
float   hysteresisBand = 2.0; 

// User requests from browser
esp_err_t   Temp_set_target_temp( int val );
esp_err_t   Temp_set_target_range( int val );

float turnOnThreshold;  // = desiredTemperature - hysteresisBand;
float turnOffThreshold; 	// = desiredTemperature + hysteresisBand;

int PWR5V_PIN = 16;
int	PWR12V_PIN = 17;

static const char *TAG = "TempControl";	//TAG for debug

// Define states for the heater
typedef enum _HeaterState {
    OFF,
    ON
} HeaterState;

HeaterState heaterState = OFF; // Initial heater state

void 	TurnOnHeater(void)
{
	ESP_LOGI(TAG,"TurnOnHeater");
	gpio_set_level(PWR5V_PIN,1);
}

void 	TurnOffHeater(void)
{
	ESP_LOGI(TAG,"TurnOffHeater");
	gpio_set_level(PWR5V_PIN,0);
}

// request for new value
esp_err_t   Temp_set_target_temp( int val )
{
    if(val < MIN_TEMP || val > MAX_TEMP)
    {
        ESP_LOGI(TAG, "Invalid target temp! %d", val);
        return ESP_ERR_INVALID_ARG;
    }
    desiredTemperature = val;
    return ESP_OK;
}

esp_err_t   Temp_set_target_range( int val )
{
    if(val < MIN_RANGE || val > MAX_RANGE)
    {
        ESP_LOGI(TAG, "Invalid target temp range! %d", val);
        return ESP_ERR_INVALID_ARG;
    }
    hysteresisBand = val;
    return ESP_OK;
}

esp_err_t   Temp_get_current_temp( int *val)
{
    ESP_LOGI(TAG,"Temp_get_current_temp called");
    *val = (int)CurrentTemp;
    return ESP_OK;
}

void TempControl(void *pvParameters) 
{
    MCP9808_config_t    cfg;  
	MCP9808_handle_t* handle = NULL;
    cfg.i2c_address = MCP9808_I2CADDR_DEFAULT;
    cfg.i2c_num = I2C_NUM_0;
    uint16_t    manuf_id;
    uint16_t    dev_id;

    ESP_LOGI(TAG, "%s: called", __FILE__);

    esp_err_t  ret = MCP9808_init(&cfg, handle, &manuf_id, &dev_id);
    if(ret != ESP_OK)
    {
        ESP_LOGD(TAG, "Error from MCP9808_init = %s", esp_err_to_name(ret));
        for(;;) 
        { 
            vTaskDelay(1000);
        }
    }

	turnOnThreshold = desiredTemperature - hysteresisBand;
	turnOffThreshold = desiredTemperature + hysteresisBand;

    ESP_LOGI(TAG, "turnOnThreshold = %f", turnOnThreshold);
	ESP_LOGI(TAG, "turnOffThreshold = %f", turnOffThreshold);

    ESP_LOGI(TAG, "Starting temp loop...");
    while(1)
    {
        float   res;
        esp_err_t ret = MCP9808_ambient_temp(handle, &res);
        if(ret != ESP_OK)
        {
            ESP_LOGI(TAG,"Error from MCP9808_ambient_temp = %s",esp_err_to_name(ret));
        }
        else
        {
            ESP_LOGI(TAG, "Temperature is %f C (%f F)", res, (res*9)/5 + 32);

        }

        CurrentTemp = ret;

		// Hysteresis logic
		if (heaterState == OFF)
		{
			if (CurrentTemp < turnOnThreshold)
			{
				heaterState = ON;
				ESP_LOGI(TAG, "Temperature (%.2f C) dropped below %.2f C Heater ON.", CurrentTemp, turnOnThreshold);
				TurnOnHeater();
			}
		}
		else
		{ // heaterState == ON
			if (CurrentTemp > turnOffThreshold)
			{
				heaterState = OFF;
				ESP_LOGI(TAG, "Temperature (%.2f C) rose above %.2f C Heater OFF.", CurrentTemp, turnOffThreshold);
				TurnOffHeater();
			}
		}

        vTaskDelay(pdMS_TO_TICKS(5000));

    }


}

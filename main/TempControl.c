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

#include "driver/ledc.h"            // need PWM control of heating element, using LEDC driver.
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

const int HEATING_ELEMENT_PIN = 16;
int	HEAT_IND_PIN = 17;

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
	gpio_set_level(HEATING_ELEMENT_PIN,1);
    gpio_set_level(HEAT_IND_PIN,1);
}

void 	TurnOffHeater(void)
{
	ESP_LOGI(TAG,"TurnOffHeater");
	gpio_set_level(HEATING_ELEMENT_PIN,0);
    gpio_set_level(HEAT_IND_PIN,0);
}

// --------------------------------------------------------------------------------
// LEDC PWM applied to Heating Control.
// --------------------------------------------------------------------------------

ledc_timer_config_t ledc_timer = {
    .speed_mode       = LEDC_LOW_SPEED_MODE,
    .timer_num        = LEDC_TIMER_0,
    .duty_resolution  = LEDC_TIMER_10_BIT, // 10-bit resolution (0 to 1023)
    .freq_hz          = 5000,              // 5 kHz frequency
    .clk_cfg          = LEDC_AUTO_CLK
};

ledc_channel_config_t ledc_channel = {
    .speed_mode     = LEDC_LOW_SPEED_MODE,
    .channel        = LEDC_CHANNEL_0,
    .timer_sel      = LEDC_TIMER_0,
    .intr_type      = LEDC_INTR_DISABLE,
    .gpio_num       = HEATING_ELEMENT_PIN,  // Adjust to your desired GPIO
    .duty           = 0,                    // Initial duty cycle (0 = OFF)
    .hpoint         = 0
};

// Setup of the LEDC PWM handling
esp_err_t   LEDC_PWM_Temp_Setup( void )
{
    esp_err_t   retval = ESP_OK;

    retval = ledc_timer_config(&ledc_timer);
    if(retval != ESP_OK)
    {
        ESP_LOGE(TAG, " error from ledc_timer_config = %d(%s)", retval, esp_err_to_name(retval));
        while(1)
        {
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    }
    retval = ledc_channel_config(&ledc_channel);
    if(retval != ESP_OK)
    {
        ESP_LOGE(TAG, " error from ledc_channel_config = %d(%s)", retval, esp_err_to_name(retval));
        while(1)
        {
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    }

    return retval;
}

// Define states for the heater
typedef enum _LEDC_PWM_HeaterState
{
    INIT_HEAT = 0,
    RAMPUP_HEAT,
    PLATEAU_HEAT,
    RAMPDOWN_HEAT
} LEDC_PWM_HeaterState;

#define DO_NOTHING      (-1)

LEDC_PWM_HeaterState LEDCPWM_heaterState = INIT_HEAT;     // Initial heater state

int    RampupCount = 0;
int    RampdownCount = 4;

void LEDCPWN_startRampUp(void)
{
    ESP_LOGI(TAG," LEDCPWN_startRampUp");
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 128));   //  12.5% Duty cycle 
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
    RampupCount++;
}

void LEDCPWN_startRampDown(void)
{
    ESP_LOGI(TAG," LEDCPWN_startRampDown");
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 128*RampdownCount));   //  12.5% Duty cycle 
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
    RampdownCount--;
}

// In the code, we will have to find what action to take upon new temperature sensor reading. 
// Based on that, we will need to determine what actions do we take? We'll need to know what 
// PWM heating state the state machine says (e..g., INIT_HEAT, RAMPUP_HEAT, etc.).
void NextTemperatureAction( float temperature )
{

    switch(LEDCPWM_heaterState)
    {
        case  INIT_HEAT:
            ESP_LOGI(TAG, "LEDCPWM_heaterState == INIT_HEAT, temperature = %f", temperature);
            // If the current temp is less than turnOnThreshold, then switch
            // the LEDCPWM_heaterState to RAMPUP_HEAT
            if(temperature < turnOnThreshold)
            {
                ESP_LOGI(TAG, "temperature=%f,turnOnThreshold=%f",temperature,turnOnThreshold);
                TurnOnHeater();                         // signal with the LED
                LEDCPWM_heaterState = RAMPUP_HEAT;
                LEDCPWN_startRampUp();
            }
            break;
        case  RAMPUP_HEAT:
            ESP_LOGI(TAG, "LEDCPWM_heaterState == RAMPUP_HEAT, temperature = %f", temperature);
            // Check if current temp is still less than turnOnThreshold, and if so
            // check if RampupCount < 4, continue ramp up, increment RampupCount
            if(temperature < turnOnThreshold)
            {
                ESP_LOGI(TAG, "temperature=%f,turnOnThreshold=%f",temperature,turnOnThreshold);
                if (RampupCount < 4)
                {
                    ESP_LOGI(TAG,"RampupCount=%d",RampupCount);
                    RampupCount++;
                    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 128 * RampupCount)); 
                    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
                }
                else if(RampupCount == 4)
                {
                    // we've reached the Plateau - leave
                    LEDCPWM_heaterState = PLATEAU_HEAT;
                }
            }
            else if(temperature >= turnOnThreshold && temperature < turnOffThreshold)
            {
                // the incubator has reached sufficient internal temperature - change state to Plateau
                // until temperature becomes greater than turnOffThreshold
                LEDCPWM_heaterState = PLATEAU_HEAT;
            }
            else if(temperature >= turnOnThreshold && temperature > turnOffThreshold)
            {
                // The temperature has surpassed both turnOnThreshold & turnOffThreshold, so now
                // we flip back to a Rampdown procedure. This also indicates that the heating element
                // PWM values are to high, and should be adjusted.
                RampdownCount = 4;              // reset 
                LEDCPWM_heaterState = RAMPDOWN_HEAT;
            }
            break;
        case  PLATEAU_HEAT:
            ESP_LOGI(TAG, "LEDCPWM_heaterState == PLATEAU_HEAT, temperature = %f,turnOffThreshold=%f ", temperature,turnOffThreshold);
            // If the temperature is greater than the turnOffThreshold, switch to RAMPDOWN
            if(temperature > turnOffThreshold)
            {
                LEDCPWM_heaterState = RAMPDOWN_HEAT;
            }
            break;
        case  RAMPDOWN_HEAT:
            ESP_LOGI(TAG, "LEDCPWM_heaterState == RAMPDOWN_HEAT, temperature = %f,turnOffThreshold=%f ", temperature,turnOffThreshold);
            if(RampdownCount == 4)
            {
                LEDCPWN_startRampDown();
            }
            else
            {
                ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 128 * RampdownCount)); 
                ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));   
            }
            RampdownCount--;
            ESP_LOGI(TAG,"RampdownCount=%d",RampdownCount);
            if(!RampdownCount)
            {
                TurnOffHeater();
                // NO!! Do not reset RampdownCount! 
                //  RampdownCount = 4;                  // Reset
                ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0));     // No power for PWM is what we need. 
                ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));   
                LEDCPWM_heaterState = INIT_HEAT;    // reset to start.
                RampupCount = 0;                    // Reset
            }
            // ??
            break;
        default:
            ESP_LOGE(TAG, "invalid state = %d", LEDCPWM_heaterState);
            break;

    }
}

// --------------------------------------------------------------------------------


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
    //ESP_LOGI(TAG,"Temp_get_current_temp called");
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

    esp_err_t retv = LEDC_PWM_Temp_Setup();
    if(retv != ESP_OK)
    {
        ESP_LOGE(TAG, "LEDC_PWM_Temp_Setup return %d(%s) ", retv, esp_err_to_name(retv));
        while(1)
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
            ESP_LOGE(TAG,"Error from MCP9808_ambient_temp = %s",esp_err_to_name(ret));
        }
        else
        {
            // ESP_LOGI(TAG, "Temperature is %f C (%f F)", res, (res*9)/5 + 32);
        }

        CurrentTemp = res;

        // the following can be change by the user in the browser,
        // so we need to  update these 
	    turnOnThreshold = desiredTemperature - hysteresisBand;
	    turnOffThreshold = desiredTemperature + hysteresisBand;

#ifdef  USE_SIMPLE_HYSTERESIS
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
#else
        NextTemperatureAction(CurrentTemp);
#endif
        vTaskDelay(pdMS_TO_TICKS(10000));

    }


}

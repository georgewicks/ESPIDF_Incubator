/**
 * @file ina219.c
 * @author gwicks (george.r.wicks@gmail.com)
 * @brief  ESP-IDF component for the INA219 current sensor. Based in large part on the UncleRus/esp-idf-lib, but
 *         the low level I2C access uses the driver/i2c_master.h functionality introduced in version 5.0 of
 *         ESP-IDF.
 * @version 0.1
 * @date 2026-06-24
 * 
 * @copyright 
 * Copyright (c) 2019 Ruslan V. Uss <unclerus@gmail.com>
 *
 * BSD Licensed as described in the file LICENSE
 * 
 */

#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "esp_system.h"
#include "esp_log.h"

#ifdef  LEGACY_I2C
#include "driver/i2c.h"
#else
#include "driver/i2c_master.h"
extern SemaphoreHandle_t	xMutex;
extern i2c_master_bus_handle_t bus_handle;
extern i2c_master_dev_handle_t dev_handle;
#endif

#include "ina219.h"
 
static const char *TAG = "ina219";

static esp_err_t INA219_read16(  const INA219_config_t* conf, uint8_t reg, 
                            uint16_t* res)
{ 
    uint8_t write_buf[2];
    uint8_t read_buf[2];
    ESP_LOGI(TAG, "%s: called", __func__);

    // copy the 8 bit register to the first byte of the write_buf
    write_buf[0] = reg;
    
    // return i2c_master_transmit_receive( conf->i2c_dev, (uint8_t *)write_buf, 1, 
    //                                    (uint8_t *)read_buf, 2, CONFIG_BMX280_TIMEOUT);
    return i2c_master_transmit_receive( dev_handle, (uint8_t *)write_buf, 1, 
                                        (uint8_t *)read_buf, 2, CONFIG_BMX280_TIMEOUT);
}

static esp_err_t INA219_write(  const INA219_config_t* conf, uint8_t addr, 
                                const uint8_t *din, size_t size)
{
    esp_err_t   retval = ESP_OK;
    ESP_LOGI(TAG, "%s: called", __func__);
    for(uint8_t i = 0; i < size; i++)
    {
        uint8_t dat[2] = {(addr + i), din[i]};
        //if ((retval = i2c_master_transmit(conf->i2c_dev, dat, 2, CONFIG_BMX280_TIMEOUT)) != ESP_OK)
        if ((retval = i2c_master_transmit(dev_handle, dat, 2, CONFIG_BMX280_TIMEOUT)) != ESP_OK)
            return retval;
    }

    return( retval );
}

/**
 * @brief The following are the INA219 API calls
 * 
 */

// Initialize the INA219
esp_err_t INA219_init(INA219_config_t* conf, i2c_master_bus_handle_t bus_handle)
{
    esp_err_t   retval = ESP_OK;

    ESP_LOGI(TAG, "%s: called", __func__);

    // Need the following in order to communicate on the i2c
    conf->i2c_address = INA219_I2C_ADDR;
    conf->i2c_num = INA219_I2C_NUM;
    conf->bus_handle = bus_handle;

    return(retval);
}

// This is the minimum work that we need to do. 
esp_err_t   INA219_SetMaxCurrentShunt( INA219_config_t* conf, float current, float shunt)
{
    esp_err_t   retval = ESP_OK;
    uint16_t    calibration_val;
    ESP_LOGI(TAG, "%s: called", __func__);

    assert(current >= 0.001);
    assert(shunt >= 0.001);

    // TODO: find calculation procedure from datasheet
    conf->current_LSB = current/32768.0;
    conf->max_current = current;
    conf->shunt = shunt;
    calibration_val = (uint16_t)(0.04096/(conf->current_LSB * shunt));

    retval = INA219_write(conf, INA219REG_CALIBRATION, 
                (const uint8_t *)&calibration_val, sizeof(uint16_t));
    return(retval);
}

esp_err_t  INA219_GetBusVoltage( INA219_config_t* conf, float *busvoltage )
{
    uint16_t    val;
    ESP_LOGI(TAG, "%s: called", __func__);

    // Read the current bus voltage INA219REG_BUS
    esp_err_t readv = INA219_read16(  conf, INA219REG_BUS, &val );
    return readv;
}

esp_err_t  INA219_GetShuntVoltage( INA219_config_t* conf, float *shuntvoltage )
{
    uint16_t    val;
    ESP_LOGI(TAG, "%s: called", __func__);

    // Read the current shunt voltage INA219REG_SHUNT
    esp_err_t readv = INA219_read16(  conf, INA219REG_SHUNT, &val );
    return readv;

}

esp_err_t  INA219_GetCurrent( INA219_config_t* conf, float *current )
{
    uint16_t    val;
    ESP_LOGI(TAG, "%s: called", __func__);

    // Read the "current" current INA219REG_CURRENT
    esp_err_t readv = INA219_read16(  conf, INA219REG_CURRENT, &val );
    return readv;
}

esp_err_t  INA219_GetPower( INA219_config_t* conf, float *power )
{

    uint16_t    val;
    ESP_LOGI(TAG, "%s: called", __func__);

    // Read the current current INA219REG_POWER
    esp_err_t readv = INA219_read16(  conf, INA219REG_POWER, &val );
    return readv;
}




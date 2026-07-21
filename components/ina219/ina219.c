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
    //ESP_LOGI(TAG, "%s: called", __func__);

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
    //ESP_LOGI(TAG, "%s: called", __func__);
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

    //ESP_LOGI(TAG, "%s: called", __func__);

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
    //ESP_LOGI(TAG, "%s: called", __func__);

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
    //ESP_LOGI(TAG, "%s: called", __func__);

    // Read the current bus voltage INA219REG_BUS
    esp_err_t readv = INA219_read16(  conf, INA219REG_BUS, &val );
    return readv;
}

esp_err_t  INA219_GetShuntVoltage( INA219_config_t* conf, float *shuntvoltage )
{
    uint16_t    val;
    //ESP_LOGI(TAG, "%s: called", __func__);

    // Read the current shunt voltage INA219REG_SHUNT
    esp_err_t readv = INA219_read16(  conf, INA219REG_SHUNT, &val );
    return readv;

}

esp_err_t  INA219_GetCurrent( INA219_config_t* conf, float *current )
{
    uint16_t    val;
    //ESP_LOGI(TAG, "%s: called", __func__);

    // Read the "current" current INA219REG_CURRENT
    esp_err_t readv = INA219_read16(  conf, INA219REG_CURRENT, &val );
    return readv;
}

esp_err_t  INA219_GetPower( INA219_config_t* conf, float *power )
{

    uint16_t    val;
    //ESP_LOGI(TAG, "%s: called", __func__);

    // Read the current current INA219REG_POWER
    esp_err_t readv = INA219_read16(  conf, INA219REG_POWER, &val );
    return readv;
}


// ****************************************************************************
// the following items are from Uncle Ruslan's code base
//  Check to see if the read_16 & write_16 functions are equivalent to the 
//  current INA219 functions - if not, change accordingly.
// ****************************************************************************

#define CHECK(x) do { esp_err_t __; if ((__ = x) != ESP_OK) return __; } while (0)
#define CHECK_ARG(VAL) do { if (!(VAL)) return ESP_ERR_INVALID_ARG; } while (0)

static const float u_shunt_max[] = {
    [INA219_GAIN_1]     = 0.04,
    [INA219_GAIN_0_5]   = 0.08,
    [INA219_GAIN_0_25]  = 0.16,
    [INA219_GAIN_0_125] = 0.32,
};

static esp_err_t read_conf_bits(INA219_config_t* conf, uint16_t mask, uint8_t bit, uint16_t *res)
{
    esp_err_t  retval;
    uint16_t raw;
    //CHECK(read_reg_16(conf, REG_CONFIG, &raw));
    retval = INA219_read16(conf, REG_CONFIG,  &raw);
    if(retval != ESP_OK)
    {
        ESP_LOGE(TAG, "%s: error from INA219_read16 = %d(%s)", __func__, retval, esp_err_to_name(retval));
    }

    *res = (raw & mask) >> bit;

    return ESP_OK;
}

esp_err_t ina219_get_gain(INA219_config_t* conf, ina219_gain_t *gain)
{
    CHECK_ARG(conf && gain);
    *gain = 0;
    return read_conf_bits(conf, MASK_PG, BIT_PG0, (uint16_t *)gain);
}

// slightly modified from the original.
esp_err_t ina219_configure(INA219_config_t* conf, ina219_bus_voltage_range_t u_range,
        ina219_gain_t gain, ina219_resolution_t u_res,
        ina219_resolution_t i_res, ina219_mode_t mode)
{
    CHECK_ARG(conf);
    CHECK_ARG(u_range <= INA219_BUS_RANGE_32V);
    CHECK_ARG(gain <= INA219_GAIN_0_125);
    CHECK_ARG(u_res <= INA219_RES_12BIT_128S);
    CHECK_ARG(i_res <= INA219_RES_12BIT_128S);
    CHECK_ARG(mode <= INA219_MODE_CONT_SHUNT_BUS);

    conf->config = (u_range << BIT_BRNG) |
                  (gain << BIT_PG0) |
                  (u_res << BIT_BADC0) |
                  (i_res << BIT_SADC0) |
                  (mode << BIT_MODE);

    ESP_LOGD(TAG, "Config: 0x%04x", conf->config);

    //return write_reg_16(conf, REG_CONFIG, conf->config);
    return INA219_write(conf, INA219REG_CALIBRATION, 
                (const uint8_t *)&conf->config, sizeof(uint16_t)); 
}

esp_err_t ina219_calibrate(INA219_config_t* conf, float r_shunt)
{
    CHECK_ARG(conf);

    ina219_gain_t gain;
    CHECK(ina219_get_gain(conf, &gain));

    conf->i_lsb = (uint16_t)(u_shunt_max[gain] / r_shunt / 32767 * 100000000);
    conf->i_lsb /= 100000000;
    conf->i_lsb /= 0.0001;
    conf->i_lsb = ceil(conf->i_lsb);
    conf->i_lsb *= 0.0001;

    conf->p_lsb = conf->i_lsb * 20;

    uint16_t cal = (uint16_t)((0.04096) / (conf->i_lsb * r_shunt));

    ESP_LOGD(TAG, "Calibration: %.04f Ohm, 0x%04x", r_shunt, cal);

    //return write_reg_16(dev, REG_CALIBRATION, cal);
    return INA219_write(conf, REG_CALIBRATION, (const uint8_t *)&cal, sizeof(uint16_t));
}



/**
 * @file        MCP9808.c
 * @author      gwicks
 * @brief       Functions for communicating with the MCP9808 temperature sensor
 * @version     0.1
 * @date        2026-04-02
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#ifdef  LEGACY_I2C
#include "driver/i2c.h"
#else
#include "driver/i2c_master.h"
extern i2c_master_bus_handle_t bus_handle;
extern i2c_master_dev_handle_t dev_handle;
#endif

#include "MCP9808.h"

static const char *TAG = "MCP9808";	//TAG for debug

// Let the user create the MCP9808_config_t structure! The user should know how to setup the I2C address & bus
esp_err_t MCP9808_init(const MCP9808_config_t* conf, MCP9808_handle_t* handle, uint16_t* manuf_id, uint16_t* dev_id)
{
    if(conf == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "MCP9808_init called ");

    // Get the manufacturer's and device ID
    uint16_t Manufact_id;
    uint16_t Device_id;

    if (MCP9808_read16(conf, MCP9808_REG_MANUF_ID, &Manufact_id) != ESP_OK)
    {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "MCP9808_init Manufact_id = %X ", Manufact_id);

    if (MCP9808_read16(conf, MCP9808_REG_DEVICE_ID, &Device_id) != ESP_OK)
    {
        return ESP_FAIL;
    }

    uint8_t     real_dev_id = (Device_id & 0xFF00) >> 8;
    uint8_t     ver_dev_id = (Device_id & 0xFF);
    ESP_LOGI(TAG, "MCP9808_init Device_id = %X. Version ID = %X ", real_dev_id, ver_dev_id);

    //*handle = (MCP9808_handle_t*) conf;
    //ESP_LOGI(TAG, "MCP9808_init handle = %p ", handle);
    //*manuf_id = Manufact_id;
    //ESP_LOGI(TAG, "MCP9808_init Manufact_id = %d ", Manufact_id);
    //*dev_id = Device_id;
    ESP_LOGI(TAG, "done.");

    return ESP_OK;
}

 
esp_err_t MCP9808_read16(const MCP9808_config_t* desc, uint8_t reg, uint16_t* res)
{
    int ret;

    ESP_LOGI(TAG, "MCP9808_read16 called ");

#ifdef  LEGACY_I2C
    uint8_t upper;
    uint8_t lower;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 0x30, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 0x31, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, &upper, ACK_VAL);
    i2c_master_read_byte(cmd, &lower, NACK_VAL);
    i2c_master_stop(cmd);

    ESP_LOGI(TAG, "calling i2c_master_cmd_begin ");

    ret = i2c_master_cmd_begin(desc->i2c_num, cmd, 1000 / portTICK_PERIOD_MS );

    i2c_cmd_link_delete(cmd);

    ESP_LOGI(TAG, "MCP9808_read16 adjusting upper and lower ");

    *res = (upper << 8 ) | (lower & 0xff);

#else

    uint8_t write_buf[2];
    uint8_t read_buf[2];

    write_buf[0] = reg;

    // the -1 will mean block forever
    ret = i2c_master_transmit_receive(dev_handle, write_buf, 1, read_buf, 2, -1);
    if(ret != ESP_OK)
    {
        ESP_LOGD(TAG, "error from i2c_master_transmit_receive = %s", esp_err_to_name(ret));
    }

    *res = (read_buf[0] << 8 ) | (read_buf[1] & 0xff);

#endif

    if (ret != ESP_OK)
    {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "MCP9808_read16 done");

    return ESP_OK;
}

esp_err_t MCP9808_read8(const MCP9808_config_t* desc, uint8_t reg, uint8_t* res)
{
    int ret;

#ifdef  LEGACY_I2C
    uint8_t result;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 0x30, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 0x31, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, &result, NACK_VAL);
    i2c_master_stop(cmd);

    ret = i2c_master_cmd_begin(desc->i2c_num, cmd, 1000 / portTICK_PERIOD_MS);

    i2c_cmd_link_delete(cmd);
    *res = result;

#else
    uint8_t write_buf[2];
    uint8_t read_buf[2];

    write_buf[0] = reg;

    // the -1 will mean block forever
    ret = i2c_master_transmit_receive(dev_handle, write_buf, 1, read_buf, 1, -1);
    if(ret != ESP_OK)
    {
        ESP_LOGD(TAG, "error from i2c_master_transmit_receive = %s", esp_err_to_name(ret));
    }

    *res = read_buf[0];

#endif

    if (ret != ESP_OK)
    {
        return ESP_FAIL;
    }

    return ESP_OK;
}


esp_err_t MCP9808_ambient_temp(MCP9808_handle_t handle, float* res)
{
    MCP9808_config_t* desc = (MCP9808_config_t*) handle;

    float temp = 0.0;
    uint16_t val;

    if (MCP9808_read16(desc, MCP9808_REG_AMBIENT_TEMP, &val) != ESP_OK)
    {
        return ESP_FAIL;
    }

    if (val == 0xFFFF)
    {
        return ESP_FAIL;
    }

    temp = val & 0x0FFF;
    temp /= 16.0;

    if (val & 0x1000)
    {
        temp -= 256;
    }

    *res = temp;

    return ESP_OK;
}

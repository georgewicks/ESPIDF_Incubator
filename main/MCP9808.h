/**
 * @file MCP9808.h
 * @author gwicks
 * @brief   Header file for the MCP9808 temperature sensor (ESP-IDF compatible)
 * @version 0.1
 * @date 2026-04-02
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#ifdef  LEGACY_I2C
#include "driver/i2c.h"
#else
#include "driver/i2c_master.h"
#endif

#include "mcp9808commands.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint8_t         i2c_address;
    i2c_port_t      i2c_num;
} MCP9808_config_t;

typedef void* MCP9808_handle_t;

esp_err_t MCP9808_read16(const MCP9808_config_t* desc, uint8_t reg, uint16_t* res);

esp_err_t MCP9808_read8(const MCP9808_config_t* desc, uint8_t reg, uint8_t* res);

void MCP9808_delete(MCP9808_handle_t handle);                           // get rid of

esp_err_t MCP9808_init(const MCP9808_config_t* conf, MCP9808_handle_t* handle, uint16_t* manuf_id, uint16_t* dev_id);

esp_err_t MCP9808_ambient_temp(MCP9808_handle_t handle, float* res);



#ifdef __cplusplus
}
#endif

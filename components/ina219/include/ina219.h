/**
 * @file ina219.h
 * @author gwicks (george.r.wicks@gmail.com)
 * @brief  ESP-IDF component for the INA219 current sensor. 
 * @version 0.1
 * @date 2026-06-24
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#pragma     once


#include <string.h>
#include <assert.h>
#include <math.h>

#define INA219REG_CONFIG        (0)
#define INA219REG_SHUNT         (1)
#define INA219REG_BUS           (2)
#define INA219REG_POWER         (3)
#define INA219REG_CURRENT       (4)
#define INA219REG_CALIBRATION   (5)

#define BIT_RST   15
#define BIT_BRNG  13
#define BIT_PG0   11
#define BIT_BADC0 7
#define BIT_SADC0 3
#define BIT_MODE  0

#define MASK_PG   (3 << BIT_PG0)
#define MASK_BADC (0xf << BIT_BADC0)
#define MASK_SADC (0xf << BIT_SADC0)
#define MASK_MODE (7 << BIT_MODE)
#define MASK_BRNG (1 << BIT_BRNG)

#define DEF_CONFIG 0x399f
#define INA219_I2C_ADDR     (0x40)
#define INA219_I2C_NUM      I2C_NUM_0

typedef struct
{
    uint8_t         i2c_address;
    i2c_port_t      i2c_num;
    
    // I2C master handle via port with configuration
    i2c_master_dev_handle_t i2c_dev;

    i2c_master_bus_handle_t bus_handle;     // need for I2C master handling...

    // Internal values for the INA219
    float           current_LSB;
    float           max_current;
    float           shunt;

} INA219_config_t;

typedef void* INA219_handle_t;

// Initialize the INA219
esp_err_t INA219_init(INA219_config_t* conf, i2c_master_bus_handle_t bus_handle);

// This is the minimum work that we need to do. 
esp_err_t   INA219_SetMaxCurrentShunt( INA219_config_t* conf, float current, 
                                        float shunt);

esp_err_t  INA219_GetBusVoltage( INA219_config_t* conf, float *busvoltage );

esp_err_t  INA219_GetShuntVoltage( INA219_config_t* conf, float *shuntvoltage );

esp_err_t  INA219_GetCurrent( INA219_config_t* conf, float *current );

esp_err_t  INA219_GetPower( INA219_config_t* conf, float *power );






                                        
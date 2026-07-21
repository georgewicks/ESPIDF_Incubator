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

// ****************************************************************************
// the following items are included from Uncle Ruslan's code base
// ****************************************************************************

#define REG_CONFIG      0
#define REG_SHUNT_U     1
#define REG_BUS_U       2
#define REG_POWER       3
#define REG_CURRENT     4
#define REG_CALIBRATION 5

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

// ****************************************************************************

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

    // for compatibility with Uncle Ruslan's code base.
    uint16_t        config;
    float           i_lsb, p_lsb;

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

// ****************************************************************************
// the following items are from Uncle Ruslan's code base
// ****************************************************************************
#define     CONFIG_EXAMPLE_SHUNT_RESISTOR_MILLI_OHM  (100)          // Shunt resistor in milliohms --- 
/**
 * Bus voltage range
 */
typedef enum {
    INA219_BUS_RANGE_16V = 0, //!< 16V FSR
    INA219_BUS_RANGE_32V      //!< 32V FSR (default)
} ina219_bus_voltage_range_t;

/**
 * PGA gain for shunt voltage
 */
typedef enum {
    INA219_GAIN_1 = 0, //!< Gain: 1, Range: +-40 mV
    INA219_GAIN_0_5,   //!< Gain: 1/2, Range: +-80 mV
    INA219_GAIN_0_25,  //!< Gain: 1/4, Range: +-160 mV
    INA219_GAIN_0_125  //!< Gain: 1/8, Range: +-320 mV (default)
} ina219_gain_t;

/**
 * ADC resolution/averaging
 */
typedef enum {
    INA219_RES_9BIT_1S    = 0,  //!< 9 bit, 1 sample, conversion time 84 us
    INA219_RES_10BIT_1S   = 1,  //!< 10 bit, 1 sample, conversion time 148 us
    INA219_RES_11BIT_1S   = 2,  //!< 11 bit, 1 sample, conversion time 276 us
    INA219_RES_12BIT_1S   = 3,  //!< 12 bit, 1 sample, conversion time 532 us (default)
    INA219_RES_12BIT_2S   = 9,  //!< 12 bit, 2 samples, conversion time 1.06 ms
    INA219_RES_12BIT_4S   = 10, //!< 12 bit, 4 samples, conversion time 2.13 ms
    INA219_RES_12BIT_8S   = 11, //!< 12 bit, 8 samples, conversion time 4.26 ms
    INA219_RES_12BIT_16S  = 12, //!< 12 bit, 16 samples, conversion time 8.51 ms
    INA219_RES_12BIT_32S  = 13, //!< 12 bit, 32 samples, conversion time 17.02 ms
    INA219_RES_12BIT_64S  = 14, //!< 12 bit, 64 samples, conversion time 34.05 ms
    INA219_RES_12BIT_128S = 15, //!< 12 bit, 128 samples, conversion time 68.1 ms
} ina219_resolution_t;

/**
 * Operating mode
 */
typedef enum {
    INA219_MODE_POWER_DOWN = 0, //!< Power-done
    INA219_MODE_TRIG_SHUNT,     //!< Shunt voltage, triggered
    INA219_MODE_TRIG_BUS,       //!< Bus voltage, triggered
    INA219_MODE_TRIG_SHUNT_BUS, //!< Shunt and bus, triggered
    INA219_MODE_DISABLED,       //!< ADC off (disabled)
    INA219_MODE_CONT_SHUNT,     //!< Shunt voltage, continuous
    INA219_MODE_CONT_BUS,       //!< Bus voltage, continuous
    INA219_MODE_CONT_SHUNT_BUS  //!< Shunt and bus, continuous (default)
} ina219_mode_t;

esp_err_t ina219_configure(INA219_config_t* conf, ina219_bus_voltage_range_t u_range,
        ina219_gain_t gain, ina219_resolution_t u_res,
        ina219_resolution_t i_res, ina219_mode_t mode);
    
esp_err_t ina219_calibrate(INA219_config_t* conf, float r_shunt);





                                        
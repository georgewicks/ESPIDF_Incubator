/**
 * @file        mcp9808commands.h
 * @author      gwicks
 * @brief       Commands for MCP9808
 * @version     0.1
 * @date        2026-04-02
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once


/**
 * @brief The following are copied from the Adafruit version of the mcp9808 library (arduino version)
 * 
 */
#define MCP9808_I2CADDR_DEFAULT         0x18    ///< I2C address
#define MCP9808_REG_CONFIG              0x01    ///< MCP9808 config register

#define MCP9808_REG_CONFIG_SHUTDOWN     0x0100  ///< shutdown config
#define MCP9808_REG_CONFIG_CRITLOCKED   0x0080  ///< critical trip lock
#define MCP9808_REG_CONFIG_WINLOCKED    0x0040  ///< alarm window lock
#define MCP9808_REG_CONFIG_INTCLR       0x0020  ///< interrupt clear
#define MCP9808_REG_CONFIG_ALERTSTAT    0x0010  ///< alert output status
#define MCP9808_REG_CONFIG_ALERTCTRL    0x0008  ///< alert output control
#define MCP9808_REG_CONFIG_ALERTSEL     0x0004  ///< alert output select
#define MCP9808_REG_CONFIG_ALERTPOL     0x0002  ///< alert output polarity
#define MCP9808_REG_CONFIG_ALERTMODE    0x0001  ///< alert output mode

#define MCP9808_REG_UPPER_TEMP          0x02    ///< upper alert boundary
#define MCP9808_REG_LOWER_TEMP          0x03    ///< lower alert boundery
#define MCP9808_REG_CRIT_TEMP           0x04    ///< critical temperature
#define MCP9808_REG_AMBIENT_TEMP        0x05    ///< ambient temperature
#define MCP9808_REG_MANUF_ID            0x06    ///< manufacture ID
#define MCP9808_REG_DEVICE_ID           0x07    ///< device ID
#define MCP9808_REG_RESOLUTION          0x08    ///< resolutin

#define ACK_CHECK_EN                    0x1     /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS                   0x0     /*!< I2C master will not check ack from slave */
#define ACK_VAL                         0x0     /*!< I2C ack value */
#define NACK_VAL                        0x1     /*!< I2C nack value */



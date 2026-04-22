/**
 * @file Incubator.h
 * @author      george.r.wicks@gmail.com
 * @brief       Any special defines, structs, constants we need
 * @version     0.1
 * @date        2026-04-20
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma     once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>  //Requires by memset
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_log.h"

#define CONFIG_WEB_PORT     80

typedef struct _incubator_URL
{
    uint8_t     data[20];           // TBD will define

} Incubator_URL;

extern esp_err_t start_server(const char *base_path, int port);

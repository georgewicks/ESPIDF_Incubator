/**
 * @file TempControl.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2026-04-19
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  //Requires by memset
#include "esp_log.h"
#include "esp_err.h"

#pragma once

extern esp_err_t   Temp_set_target_temp( int val );
extern esp_err_t   Temp_set_target_range( int val );

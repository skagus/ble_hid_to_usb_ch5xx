/********************************** (C) COPYRIGHT *******************************
 * File Name          : CONFIG.h
 * Author             : WCH
 * Version            : V1.2
 * Date               : 2022/01/18
 * Description        : Configuration description and default value, 
 *                      it is recommended to modify the current value in the preprocessing in the project configuration
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *******************************************************************************/

/******************************************************************************/
#ifndef __CONFIG_H
#define __CONFIG_H

#define	ID_CH583							0x83

#define CHIP_ID								ID_CH583

#ifdef CH58xBLE_ROM
#include "CH58xBLE_ROM.H"
#else
#include "CH58xBLE_LIB.H"
#endif

#include "CH58x_common.h"

/*********************************************************************
 【MAC】
 BLE_MAC                                    - Customize the Bluetooth Mac Address (default:FALSE - use the chip Mac Address),
                                              you need to modify the Mac Address definition in main.c
 【DCDC】
 DCDC_ENABLE                                - DCDC ( default:FALSE )

 【SLEEP】
 HAL_SLEEP                                  - Enable the sleep function ( default:FALSE )
 SLEEP_RTC_MIN_TIME                         - Minimum time to sleep in non-idle mode（unit：625us）
 WAKE_UP_RTC_MAX_TIME                       - Wait for 32M crystal oscillator stabilization time（unit：625us）
                                            According to the value of different sleep types, it can be divided into：
                                                    Sleep Mode/Power Down Mode  - 45(default)
                                                    pause mode    - 45
                                                    idle mode    - 5
 【TEMPERATION】
 TEM_SAMPLE                                 - turn on the function of calibrating according to temperature changes, 
                                                a single calibration takes less than 10ms( default:TRUE )
 
 【CALIBRATION】
 BLE_CALIBRATION_ENABLE                     - enable the function of timing calibration, 
                                                a single calibration takes less than 10ms( default:TRUE )
 BLE_CALIBRATION_PERIOD                     - Period of timing calibration, unit ms( default:120000 )
 
 【SNV】
 BLE_SNV                                    - enable the SNV function to store binding information( default:TRUE )
 BLE_SNV_ADDR                               - SNV information saves Address, use data flash at last ( default:0x77E00 )
                                            - If the SNVNum parameter is configured, 
                                            you need to modify the flash size erased in the Lib_Write_Flash function accordingly,
                                            and the size is SNVBlock*SNVNum

 【RTC】
 CLK_OSC32K                                 - RTC clock selection, if the host role is included, the external 32K must be used
                                              ( 0 external (32768Hz), default: 1: internal (32000Hz), 2: internal (32768Hz) )

 【MEMORY】
 BLE_MEMHEAP_SIZE                           - RAM size used by the Bluetooth protocol stack，min 6K ( default:(1024*6) )

 【DATA】
 BLE_BUFF_MAX_LEN                           - Maximum packet length for a single connection(default:27 (ATT_MTU=23)，Range[27~516])
 BLE_BUFF_NUM                               - Number of packets cached by the controller ( default:5 )
 BLE_TX_NUM_EVENT                           - How many data packets can be sent at most for a single connection event (default:1)
 BLE_TX_POWER                               - TX power( default:LL_TX_POWEER_0_DBM (0dBm) )
 
 【MULTICONN】
 PERIPHERAL_MAX_CONNECTION                  - How many slave roles can be played at the same time( default:1 )
 CENTRAL_MAX_CONNECTION                     - How many host roles can be played at the same time( default:3 )

 **********************************************************************/

/*********************************************************************
 * default config
 */
#ifndef BLE_MAC
#define BLE_MAC                             FALSE
#endif
#ifndef DCDC_ENABLE
#define DCDC_ENABLE                         FALSE
#endif
#ifndef HAL_SLEEP
#define HAL_SLEEP                           FALSE
#endif
#ifndef SLEEP_RTC_MIN_TIME                   
#define SLEEP_RTC_MIN_TIME                  (30U)
#endif
#ifndef WAKE_UP_RTC_MAX_TIME
#define WAKE_UP_RTC_MAX_TIME                (45U)
#endif
#ifndef HAL_KEY
#define HAL_KEY                             FALSE
#endif
#ifndef HAL_LED
#define HAL_LED                             TRUE
#endif
#ifndef TEM_SAMPLE
#define TEM_SAMPLE                          TRUE
#endif
#ifndef BLE_CALIBRATION_ENABLE
#define BLE_CALIBRATION_ENABLE              TRUE
#endif
#ifndef BLE_CALIBRATION_PERIOD
#define BLE_CALIBRATION_PERIOD              120000
#endif
#ifndef BLE_SNV
#define BLE_SNV                             TRUE
#endif
#ifndef BLE_SNV_ADDR
#define BLE_SNV_ADDR                        0x77E00-FLASH_ROM_MAX_SIZE
#endif
#ifndef CLK_OSC32K
#define CLK_OSC32K                          1   // Do not modify this item here. It must be modified in the preprocessing of the project configuration. If the host role is included, the external 32K must be used
#endif
#ifndef BLE_MEMHEAP_SIZE
#define BLE_MEMHEAP_SIZE                    (1024*6)
#endif
#ifndef BLE_BUFF_MAX_LEN
#define BLE_BUFF_MAX_LEN                    27
#endif
#ifndef BLE_BUFF_NUM
#define BLE_BUFF_NUM                        5
#endif
#ifndef BLE_TX_NUM_EVENT
#define BLE_TX_NUM_EVENT                    1
#endif
#ifndef BLE_TX_POWER
#define BLE_TX_POWER                        LL_TX_POWEER_0_DBM
#endif
#ifndef PERIPHERAL_MAX_CONNECTION
#define PERIPHERAL_MAX_CONNECTION           1
#endif
#ifndef CENTRAL_MAX_CONNECTION
#define CENTRAL_MAX_CONNECTION              3
#endif

extern uint32_t MEM_BUF[BLE_MEMHEAP_SIZE / 4];
extern const uint8_t MacAddr[6];

#endif


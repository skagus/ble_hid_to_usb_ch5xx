/********************************** (C) COPYRIGHT *******************************
 * File Name          : Main.c
 * Author             : WCH
 * Version            : V1.1
 * Date               : 2022/01/25
 * Description        : USB composite device simulation, keyboard/mouse, class command 지원
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *******************************************************************************/

#include "com.h"
#include "app_ble.h"
#include "app_usb.h"

#define BLE_MEMHEAP_SIZE                    (1024*6)
__attribute__((aligned(4))) uint32_t MEM_BUF[BLE_MEMHEAP_SIZE / 4];

/*********************************************************************
 * @fn      DevWakeup
 *
 * @brief   Device mode에서 host 깨우기
 *
 * @return  none
 */
void DevWakeup(void)
{
    R16_PIN_ANALOG_IE &= ~(RB_PIN_USB_DP_PU);
    R8_UDEV_CTRL |= RB_UD_LOW_SPEED;
    mDelaymS(2);
    R8_UDEV_CTRL &= ~RB_UD_LOW_SPEED;
    R16_PIN_ANALOG_IE |= RB_PIN_USB_DP_PU;
}

/*********************************************************************
 * @fn      DebugInit
 *
 * @brief   Debug 초기화
 *
 * @return  none
 */
void DebugInit(void)
{
    GPIOA_SetBits(GPIO_Pin_9);
    GPIOA_ModeCfg(GPIO_Pin_8, GPIO_ModeIN_PU);
    GPIOA_ModeCfg(GPIO_Pin_9, GPIO_ModeOut_PP_5mA);
    UART1_DefInit();
}

/*********************************************************************
 * @fn      main
 *
 * @brief   Main 함수
 *
 * @return  none
 */
int main(void)
{
    SetSysClock(CLK_SOURCE_PLL_60MHz);

    DebugInit();
    
    PRINT("Start\n");

    AppBLE_Init();          // Initialize BLE Central Profile & Discovery
    AppUSB_Init();            // Initialize USB Device

    while(1)
    {
        //mDelaymS(5000);
        TMOS_SystemProcess();
#if 0
        PRINT("Mouse Report\n");
        // 마우스 왼쪽 버튼
        DevHIDMouseReport(0x01);
        mDelaymS(10);
        DevHIDMouseReport(0x00);
        mDelaymS(200);

        mDelaymS(1000);
        PRINT("Keyboard Report\n");
        // 키보드 "wch" 입력
        DevHIDKeyReport(0x1A);
        mDelaymS(10);
        DevHIDKeyReport(0x00);
        mDelaymS(10);
        DevHIDKeyReport(0x06);
        mDelaymS(10);
        DevHIDKeyReport(0x00);
        mDelaymS(10);
        DevHIDKeyReport(0x0B);
        mDelaymS(10);
        DevHIDKeyReport(0x00);
#endif
    }
}

#if 0
/*********************************************************************
 * @fn      DevEP1_OUT_Deal
 *
 * @brief   Endpoint 1 데이터 처리
 *
 * @return  none
 */
void DevEP1_OUT_Deal(uint8_t l)
{ /* 사용자 정의 가능 */
    uint8_t i;

    for(i = 0; i < l; i++)
    {
        pEP1_IN_DataBuf[i] = ~pEP1_OUT_DataBuf[i];
    }
    DevEP1_IN_Deal(l);
}

/*********************************************************************
 * @fn      DevEP2_OUT_Deal
 *
 * @brief   Endpoint 2 데이터 처리
 *
 * @return  none
 */
void DevEP2_OUT_Deal(uint8_t l)
{ /* 사용자 정의 가능 */
    uint8_t i;

    for(i = 0; i < l; i++)
    {
        pEP2_IN_DataBuf[i] = ~pEP2_OUT_DataBuf[i];
    }
    DevEP2_IN_Deal(l);
}

/*********************************************************************
 * @fn      DevEP3_OUT_Deal
 *
 * @brief   Endpoint 3 데이터 처리
 *
 * @return  none
 */
void DevEP3_OUT_Deal(uint8_t l)
{ /* 사용자 정의 가능 */
    uint8_t i;

    for(i = 0; i < l; i++)
    {
        pEP3_IN_DataBuf[i] = ~pEP3_OUT_DataBuf[i];
    }
    DevEP3_IN_Deal(l);
}

/*********************************************************************
 * @fn      DevEP4_OUT_Deal
 *
 * @brief   Endpoint 4 데이터 처리
 *
 * @return  none
 */
void DevEP4_OUT_Deal(uint8_t l)
{ /* 사용자 정의 가능 */
    uint8_t i;

    for(i = 0; i < l; i++)
    {
        pEP4_IN_DataBuf[i] = ~pEP4_OUT_DataBuf[i];
    }
    DevEP4_IN_Deal(l);
}
#endif


/********************************** (C) COPYRIGHT *******************************
 * File Name          : central.h
 * Author             : WCH
 * Version            : V1.0
 * Date               : 2018/11/12
 * Description        : Central(중앙/마스터) 애플리케이션의 헤더 파일
 *                      태스크 이벤트 비트 정의와 초기화/이벤트 처리 함수 원형 선언
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *******************************************************************************/

#ifndef CENTRAL_H
#define CENTRAL_H

#ifdef __cplusplus
extern "C" {
#endif

    /*********************************************************************
     * INCLUDES
     */

     /*********************************************************************
      * CONSTANTS
      */

      /*
       * BLE 애플리케이션은 이벤트 기반(Event-driven)으로 동작한다.
       * TMOS(Task Management Operating System)가 각 태스크에 16비트 이벤트 비트맵을
       * 전달하면, 태스크는 해당 비트가 set 되어있는지 검사하여 처리한다.
       * 아래 매크로는 각 이벤트 비트의 정의이다. (1비트씩 shift 하여 사용)
       *
       * START_DEVICE_EVT          : BLE 스택 초기화 후 디바이스(스캔) 시작
       * START_DISCOVERY_EVT       : (예약) 디바이스 탐색 시작
       * START_SCAN_EVT            : (예약) 스캔 시작
       * START_SVC_DISCOVERY_EVT   : 연결 후 서비스(Service) 탐색 시작
       * START_PARAM_UPDATE_EVT    : 연결 파라미터 갱신 요청
       * START_PHY_UPDATE_EVT      : PHY(물리계층) 갱신 요청 (1M → 2M 등)
       * START_READ_OR_WRITE_EVT   : 특성(Characteristic) 값 읽기/쓰기 주기적 수행
       * START_WRITE_CCCD_EVT      : CCCD(Notify 설정 기술자) 쓰기
       * START_READ_RSSI_EVT       : 주기적으로 RSSI(수신 신호 세기) 읽기
       * ESTABLISH_LINK_TIMEOUT_EVT: 연결 시도 타임아웃 처리
       */
#define START_DEVICE_EVT              0x0001
#define START_DISCOVERY_EVT           0x0002
#define START_SCAN_EVT                0x0004
#define START_SVC_DISCOVERY_EVT       0x0008
#define START_PARAM_UPDATE_EVT        0x0010
#define START_PHY_UPDATE_EVT          0x0020
#define START_READ_OR_WRITE_EVT       0x0040
#define START_WRITE_CCCD_EVT          0x0080
#define START_READ_RSSI_EVT           0x0100
#define ESTABLISH_LINK_TIMEOUT_EVT    0x0200
#define START_PAIRING_MODE_EVT        0x0400   // ★ B22 눌림
#define B22_DEBOUNCE_EVT              0x0800   // ★ 버튼 디바운스
#define SEND_KEY_RELEASE_EVT          0x1000 

       /* Appearance 값들 */
#define APPEARANCE_HID_MOUSE    0x03C2   /* HID Mouse */
#define APPEARANCE_HID_KEYBOARD 0x03C1

/* Advertising AD Type */
#define AD_TYPE_UUID16_LIST     0x02     /* Incomplete List of 16-bit UUIDs */
#define AD_TYPE_UUID16_LIST_CMP 0x03     /* Complete   List of 16-bit UUIDs */
#define AD_TYPE_APPEARANCE      0x19
#define AD_TYPE_NAME_SHORT      0x08
#define AD_TYPE_NAME_COMPLETE   0x09

/* HID over GATT */
#define HID_SERVICE_UUID        0x1812
#define HID_REPORT_CHAR_UUID    0x2A4D
#define HID_PROTOCOL_MODE_UUID  0x2A4E

/* Report Reference Descriptor (0x2908): Input(1)/Output(2)/Feature(3) */
#define HID_REPORT_REF_UUID     0x2908

/*********************************************************************
 * MACROS
 */

 /*********************************************************************
  * FUNCTIONS
  */

  /*
   * BLE 애플리케이션 태스크 초기화 함수.
   * - TMOS에 이벤트 처리 함수(Central_ProcessEvent)를 등록하고,
   *   GAP/GATT/Bond Manager 등 하위 계층 파라미터를 설정한다.
   * - main()에서 한 번 호출된다.
   */
    extern void Central_Init(void);

    /*
     * BLE 애플리케이션 태스크 이벤트 처리 함수.
     * - TMOS가 이벤트 발생 시 호출한다.
     * - task_id : TMOS가 부여한 태스크 ID
     * - events  : 처리해야 할 이벤트 비트맵
     * - 반환값  : 아직 처리하지 못한 이벤트 비트맵
     */
    extern uint16_t Central_ProcessEvent(uint8_t task_id, uint16_t events);

    extern void AppBLE_Init(void);
    /*********************************************************************
    *********************************************************************/

#ifdef __cplusplus
}
#endif

#endif

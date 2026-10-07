/********************************** (C) COPYRIGHT *******************************
* File Name          : central.c
* Author             : WCH
* Version            : V1.1
* Date               : 2020/08/06
* Description        : Central(마스터) 예제.
*                      주변 BLE 디바이스를 스캔한 뒤, 지정한 주소의 슬레이브에 연결하고
*                      사용자 정의 서비스/특성을 찾아 읽기/쓰기를 수행한다.
*                      이 예제는 반드시 peripheral(슬레이브) 예제와 함께 사용해야 하며,
*                      슬레이브의 MAC 주소를 아래 PeerAddrDef 값과 일치시켜야 한다.
*                      (기본값 : 84:C2:E4:03:02:02)
*******************************************************************************/

/*********************************************************************
 * INCLUDES
 */
#include "CONFIG.h"
#include "com.h"
#include "app_ble.h"
#include "app_usb.h"

 /*********************************************************************
  * MACROS
  */

  // BD Address를 문자열로 표시할 때 사용하는 길이(사용되진 않음)
#define B_ADDR_STR_LEN                      15

/*********************************************************************
 * CONSTANTS
 */
 // 스캔 결과를 최대로 저장할 수 있는 개수
#define DEFAULT_MAX_SCAN_RES                10

// 스캔 지속 시간 (단위 : 0.625ms → 2400 * 0.625ms = 1.5초)
#define DEFAULT_SCAN_DURATION               2400

// 연결 최소 간격 (단위 : 1.25ms → 20 * 1.25 = 25ms)
#define DEFAULT_MIN_CONNECTION_INTERVAL     6 // 20

// 연결 최대 간격 (단위 : 1.25ms → 100 * 1.25 = 125ms)
#define DEFAULT_MAX_CONNECTION_INTERVAL     12 // 100

// 연결 감독 타임아웃 (단위 : 10ms → 100 * 10 = 1000ms = 1초)
// 이 시간 동안 패킷 교환이 없으면 연결이 끊긴 것으로 판단
#define DEFAULT_CONNECTION_TIMEOUT          200 // 100

// 탐색 모드 (제한/일반/전체) - 여기선 전체
#define DEFAULT_DISCOVERY_MODE              DEVDISC_MODE_ALL

// TRUE : 능동 스캔(Scan Response 요청), FALSE : 수동 스캔
#define DEFAULT_DISCOVERY_ACTIVE_SCAN       TRUE

// TRUE : 화이트리스트 사용(특정 디바이스만 스캔)
#define DEFAULT_DISCOVERY_WHITE_LIST        FALSE

// 연결 시 고속 스캔 듀티 사이클 사용 여부
#define DEFAULT_LINK_HIGH_DUTY_CYCLE        FALSE

// 연결 시 화이트리스트 사용 여부
#define DEFAULT_LINK_WHITE_LIST             FALSE

// RSSI 주기적 읽기 주기 (단위 : 0.625ms → 2400 * 0.625 = 1.5초)
#define DEFAULT_RSSI_PERIOD                 2400

// 연결 파라미터 갱신용 최소/최대 연결 간격 (1.25ms 단위)
#define DEFAULT_UPDATE_MIN_CONN_INTERVAL    6 // 20
#define DEFAULT_UPDATE_MAX_CONN_INTERVAL    12 // 100

// 슬레이브 레이턴시 : 슬레이브가 응답을 건너뛸 수 있는 횟수
#define DEFAULT_UPDATE_SLAVE_LATENCY        0

// 감독 타임아웃 (10ms 단위)
#define DEFAULT_UPDATE_CONN_TIMEOUT         200 // 600

// 페어링 시 사용할 기본 패스코드 (0이면 Just Works)
#define DEFAULT_PASSCODE                    0

// 페어링 모드 : 상대방 요청 시에만 페어링
#define DEFAULT_PAIRING_MODE                GAPBOND_PAIRING_MODE_INITIATE

// MITM(Man-In-The-Middle) 보호 여부 (TRUE : 인증 필요)
#define DEFAULT_MITM_MODE                   TRUE

// 본딩 여부 (TRUE : 본딩 정보 저장, 최대 6개)
#define DEFAULT_BONDING_MODE                TRUE

// GAP 입출력 능력 (No Input No Output → Just Works)
#define DEFAULT_IO_CAPABILITIES             GAPBOND_IO_CAP_NO_INPUT_NO_OUTPUT

// 서비스 탐색 지연 (0.625ms 단위 → 1600 * 0.625 = 1초)
#define DEFAULT_SVC_DISCOVERY_DELAY         1600

// 연결 파라미터 갱신 지연 (0.625ms 단위 → 2초)
#define DEFAULT_PARAM_UPDATE_DELAY          3200

// PHY 갱신 지연 (0.625ms 단위 → 1.5초)
#define DEFAULT_PHY_UPDATE_DELAY            2400

// 읽기/쓰기 지연 (0.625ms 단위 → 1초)
#define DEFAULT_READ_OR_WRITE_DELAY         1600

// CCCD 쓰기 지연 (0.625ms 단위 → 1초)
#define DEFAULT_WRITE_CCCD_DELAY            1600

// 링크 연결 시도 타임아웃 (0.625ms 단위 → 3200 * 0.625 = 2초)
#define ESTABLISH_LINK_TIMEOUT              3200


/* ★ 동작 모드 */
#define CENTRAL_MODE_PAIRED_ONLY            0   // 본딩된 디바이스만 접속
#define CENTRAL_MODE_PAIRING                1   // 신규 페어링 모드

/* ★ B22 디바운스 지연 (0.625ms 단위 → 200ms) */
#define B22_DEBOUNCE_DELAY                  320

/*
 * 애플리케이션(BLE 링크) 상태
 * IDLE          : 유휴(스캔 중이거나 미연결)
 * CONNECTING    : 연결 시도 중
 * CONNECTED     : 연결 완료
 * DISCONNECTING : 연결 해제 중
 */
enum
{
    BLE_STATE_IDLE,
    BLE_STATE_CONNECTING,
    BLE_STATE_CONNECTED,
    BLE_STATE_DISCONNECTING
};

/*
 * GATT 탐색(Discovery) 상태
 * IDLE : 탐색 안 함
 * SVC  : Primary Service 탐색 중
 * CHAR : Characteristic 탐색 중
 * CCCD : Client Characteristic Configuration Descriptor 탐색 중
 */
enum
{
    BLE_DISC_STATE_IDLE, // Idle
    BLE_DISC_STATE_SVC,  // Service discovery
    BLE_DISC_STATE_CHAR, // Characteristic discovery
    BLE_DISC_STATE_CCCD  // client characteristic configuration discovery
};
/*********************************************************************
 * TYPEDEFS
 */

 /*********************************************************************
  * GLOBAL VARIABLES
  */

  /*********************************************************************
   * EXTERNAL VARIABLES
   */

   /*********************************************************************
    * EXTERNAL FUNCTIONS
    */

    /*********************************************************************
     * LOCAL VARIABLES
     */

     // 이 애플리케이션 태스크의 TMOS Task ID
     // (이벤트 등록/전송 시 사용)
static uint8_t centralTaskId;

// 현재까지 저장한 스캔 결과 개수
static uint8_t centralScanRes;

// 스캔 결과 리스트 (최대 DEFAULT_MAX_SCAN_RES 개)
static gapDevRec_t centralDevList[DEFAULT_MAX_SCAN_RES];

// 연결 대상(Peer) 디바이스의 BLE 주소 (리틀엔디안 순서)
// 슬레이브 예제에서 설정한 MAC 주소와 일치해야 함
static uint8_t PeerAddrDef[B_ADDR_LEN] = {0x02, 0x02, 0x03, 0xE4, 0xC2, 0x84};

// RSSI 주기적 읽기 사용 여부
static uint8_t centralRssi = TRUE;

// 연결 파라미터 갱신 사용 여부
static uint8_t centralParamUpdate = TRUE;

// PHY 갱신 사용 여부 (2M PHY 등)
static uint8_t centralPhyUpdate = FALSE;

// 현재 연결의 Connection Handle (BLE 커넥션 식별자)
static uint16_t centralConnHandle = GAP_CONNHANDLE_INIT;

// 애플리케이션 상태 (BLE_STATE_xxx)
static uint8_t centralState = BLE_STATE_IDLE;

// GATT 탐색 상태 (BLE_DISC_STATE_xxx)
static uint8_t centralDiscState = BLE_DISC_STATE_IDLE;

// 탐색한 서비스의 시작/끝 Handle
// (Handle은 GATT 서버에서 각 속성(Attribute)을 가리키는 16비트 주소)
static uint16_t centralSvcStartHdl = 0;
static uint16_t centralSvcEndHdl = 0;

// 탐색한 Characteristic의 값(Value) Handle
static uint16_t centralCharHdl = 0;

// 탐색한 CCCD(Client Characteristic Configuration Descriptor)의 Handle
// - 이 기술자(Descriptor)에 값을 쓰면 슬레이브가 Notify/Indicate를 보낸다.
static uint16_t centralCCCDHdl = 0;

// Characteristic에 쓸 값(임의 데이터)
static uint8_t centralCharVal = 0x5A;

// 다음 동작이 쓰기(TRUE)인지 읽기(FALSE)인지 토글
static uint8_t centralDoWrite = TRUE;

// GATT 프로시저(읽기/쓰기/MTU 교환 등)가 진행 중인지 여부
// TRUE 이면 새로운 요청을 보내지 않는다.
static uint8_t centralProcedureInProgress = FALSE;


/* 첫 번째 Notify 가능 Report(0x2A4D) 를 저장하기 위한 플래그 */
static uint8_t centralReportFound = FALSE;

/* 페어링(암호화) 완료 여부 */
static uint8_t centralBonded = FALSE;

/* ★ 현재 동작 모드 */
static uint8_t centralMode = CENTRAL_MODE_PAIRED_ONLY;

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static void centralProcessGATTMsg(gattMsgEvent_t* pMsg);
static void centralRssiCB(uint16_t connHandle, int8_t rssi);
static void centralEventCB(gapRoleEvent_t* pEvent);
static void centralHciMTUChangeCB(uint16_t connHandle, uint16_t maxTxOctets, uint16_t maxRxOctets);
static void centralPasscodeCB(uint8_t* deviceAddr, uint16_t connectionHandle,
                              uint8_t uiInputs, uint8_t uiOutputs);
static void centralPairStateCB(uint16_t connHandle, uint8_t state, uint8_t status);
static void central_ProcessTMOSMsg(tmos_event_hdr_t* pMsg);
static void centralGATTDiscoveryEvent(gattMsgEvent_t* pMsg);
static void centralStartDiscovery(void);
static void centralAddDeviceInfo(uint8_t* pAddr, uint8_t addrType);

static uint8_t centralIsBleMouse(uint8_t* pData, uint8_t dataLen);
static uint8_t centralIsBleKeyboard(uint8_t* pData, uint8_t dataLen);

static uint8_t centralIsBondedDevice(uint8_t* pAddr, uint8_t addrType);

/*********************************************************************
 * PROFILE CALLBACKS
 */

 /*
  * GAP Role 콜백.
  * - RSSI 갱신 시 centralRssiCB 호출
  * - GAP 이벤트(광고 수신, 연결 성립, 해제 등) 발생 시 centralEventCB 호출
  * - MTU 변경 시 centralHciMTUChangeCB 호출
  */
static gapCentralRoleCB_t centralRoleCB = {
    centralRssiCB,        // RSSI callback
    centralEventCB,       // Event callback
    centralHciMTUChangeCB // MTU change callback
};

/*
 * Bond Manager 콜백.
 * - Passcode 필요 시 centralPasscodeCB
 * - 페어링/본딩 상태 변화 시 centralPairStateCB
 */
static gapBondCBs_t centralBondCB = {
    centralPasscodeCB,
    centralPairStateCB
};

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

 /*********************************************************************
  * @fn      Central_Init
  *
  * @brief   Initialization function for the Central App Task.
  *          This is called during initialization and should contain
  *          any application specific initialization (ie. hardware
  *          initialization/setup, table initialization, power up
  *          notification).
  *
  * @param   task_id - the ID assigned by TMOS.  This ID should be
  *                    used to send messages and set timers.
  *
  * @return  none
  */
void Central_Init()
{
    GPIOB_ModeCfg(GPIO_Pin_22, GPIO_ModeIN_PU);
#if defined(CH582)
    GPIOPinRemap(ENABLE, RB_PIN_INTX);
#endif
    GPIOB_ITModeCfg(GPIO_Pin_22, GPIO_ITMode_FallEdge);
    PFIC_EnableIRQ(GPIO_B_IRQn);

    centralTaskId = TMOS_ProcessEventRegister(Central_ProcessEvent);

    // Setup GAP
    GAP_SetParamValue(TGAP_DISC_SCAN, DEFAULT_SCAN_DURATION);
    GAP_SetParamValue(TGAP_CONN_EST_INT_MIN, DEFAULT_MIN_CONNECTION_INTERVAL);
    GAP_SetParamValue(TGAP_CONN_EST_INT_MAX, DEFAULT_MAX_CONNECTION_INTERVAL);
    GAP_SetParamValue(TGAP_CONN_EST_SUPERV_TIMEOUT, DEFAULT_CONNECTION_TIMEOUT);

    // Setup the GAP Bond Manager
    {
        uint32_t passkey = DEFAULT_PASSCODE;
        uint8_t  pairMode = DEFAULT_PAIRING_MODE;
        uint8_t  mitm = DEFAULT_MITM_MODE;
        uint8_t  ioCap = DEFAULT_IO_CAPABILITIES;
        uint8_t  bonding = DEFAULT_BONDING_MODE;

        GAPBondMgr_SetParameter(GAPBOND_CENT_DEFAULT_PASSCODE, sizeof(uint32_t), &passkey);
        GAPBondMgr_SetParameter(GAPBOND_CENT_PAIRING_MODE, sizeof(uint8_t), &pairMode);
        GAPBondMgr_SetParameter(GAPBOND_CENT_MITM_PROTECTION, sizeof(uint8_t), &mitm);
        GAPBondMgr_SetParameter(GAPBOND_CENT_IO_CAPABILITIES, sizeof(uint8_t), &ioCap);
        GAPBondMgr_SetParameter(GAPBOND_CENT_BONDING_ENABLED, sizeof(uint8_t), &bonding);
    }

    /* GATT Client 초기화 (Central은 GATT Client 역할 수행) */
    GATT_InitClient();

    /* Indication/Notification 수신을 위한 등록 (수신 시 GATT_MSG_EVENT 발생) */
    GATT_RegisterForInd(centralTaskId);

    /* 디바이스 시작 이벤트를 예약 → 이벤트 루프에서 START_DEVICE_EVT 처리 */
    tmos_set_event(centralTaskId, START_DEVICE_EVT);
}

/*********************************************************************
 * @fn      Central_ProcessEvent
 *
 * @brief   Central 애플리케이션 태스크 이벤트 처리기.
 *          TMOS가 이벤트 발생 시 호출한다. 각 비트를 검사하여 처리한다.
 *
 * @param   task_id  - TMOS가 부여한 태스크 ID
 * @param   events   - 이벤트 비트맵 (여러 이벤트가 동시에 set 될 수 있음)
 *
 * @return  처리하지 못한 이벤트 비트맵 (events ^ 처리한 비트)
 */
uint16_t Central_ProcessEvent(uint8_t task_id, uint16_t events)
{
    /* 시스템 메시지 이벤트 : TMOS가 전달한 메시지 큐 처리 */
    if(events & SYS_EVENT_MSG)
    {
        uint8_t* pMsg;

        if((pMsg = tmos_msg_receive(centralTaskId)) != NULL)
        {
            central_ProcessTMOSMsg((tmos_event_hdr_t*)pMsg);
            /* 메시지 사용 완료 → 반환 */
            tmos_msg_deallocate(pMsg);
        }
        /* SYS_EVENT_MSG 비트 클리어 */
        return (events ^ SYS_EVENT_MSG);
    }

    /* 디바이스 시작 이벤트 : GAP Role Central 역할 시작(광고 스캔 준비) */
    if(events & START_DEVICE_EVT)
    {
        GAPRole_CentralStartDevice(centralTaskId, &centralBondCB, &centralRoleCB);
        return (events ^ START_DEVICE_EVT);
    }

    /* 링크 연결 타임아웃 : 연결 시도가 일정 시간 내 성립하지 않으면 해제 */
    if(events & ESTABLISH_LINK_TIMEOUT_EVT)
    {
        GAPRole_TerminateLink(INVALID_CONNHANDLE);
        return (events ^ ESTABLISH_LINK_TIMEOUT_EVT);
    }

    /* 서비스 탐색 시작 이벤트 */
    if(events & START_SVC_DISCOVERY_EVT)
    {
        centralStartDiscovery();
        return (events ^ START_SVC_DISCOVERY_EVT);
    }

    /* 연결 파라미터 갱신 이벤트 */
    if(events & START_PARAM_UPDATE_EVT)
    {
        GAPRole_UpdateLink(centralConnHandle,
                           DEFAULT_UPDATE_MIN_CONN_INTERVAL,
                           DEFAULT_UPDATE_MAX_CONN_INTERVAL,
                           DEFAULT_UPDATE_SLAVE_LATENCY,
                           DEFAULT_UPDATE_CONN_TIMEOUT);
        return (events ^ START_PARAM_UPDATE_EVT);
    }

#if defined(CH582)
    /* PHY 갱신 이벤트 (예: 1M PHY → 2M PHY 로 고속 통신) */
    if(events & START_PHY_UPDATE_EVT)
    {
        PRINT("PHY Update %x...\n", GAPRole_UpdatePHY(centralConnHandle, 0,
                                                      GAP_PHY_BIT_LE_2M, GAP_PHY_BIT_LE_2M, GAP_PHY_OPTIONS_NOPRE));
        return (events ^ START_PHY_UPDATE_EVT);
    }
#endif

    /* 특성 값 읽기/쓰기 이벤트 (주기적으로 반복 실행됨) */
    if(events & START_READ_OR_WRITE_EVT)
    {
        if(centralProcedureInProgress == FALSE)
        {
            if(centralDoWrite)
            {
                /* 쓰기 요청 (ATT Write Request) */
                attWriteReq_t req;

                req.cmd = FALSE;    // FALSE : Write Request(응답 필요), TRUE : Write Command(응답 없음)
                req.sig = FALSE;    // Signed Write 여부
                req.handle = centralCharHdl;
                req.len = 1;
                /* ATT 쓰기용 메모리를 BLE 힙에서 할당 */
                req.pValue = GATT_bm_alloc(centralConnHandle, ATT_WRITE_REQ, req.len, NULL, 0);
                if(req.pValue != NULL)
                {
                    *req.pValue = centralCharVal;

                    if(GATT_WriteCharValue(centralConnHandle, &req, centralTaskId) == SUCCESS)
                    {
                        centralProcedureInProgress = TRUE;
                        /* 다음엔 읽기로 토글 */
                        centralDoWrite = !centralDoWrite;
                        /* 일정 시간 뒤 다시 읽기/쓰기 수행 예약 */
                        tmos_start_task(centralTaskId, START_READ_OR_WRITE_EVT, DEFAULT_READ_OR_WRITE_DELAY);
                    }
                    else
                    {
                        /* 전송 실패 시 할당된 메모리 반환 */
                        GATT_bm_free((gattMsg_t*)&req, ATT_WRITE_REQ);
                    }
                }
            }
            else
            {
                /* 읽기 요청 (ATT Read Request) */
                attReadReq_t req;

                req.handle = centralCharHdl;
                if(GATT_ReadCharValue(centralConnHandle, &req, centralTaskId) == SUCCESS)
                {
                    centralProcedureInProgress = TRUE;
                    /* 다음엔 쓰기로 토글 */
                    centralDoWrite = !centralDoWrite;
                }
            }
        }
        return (events ^ START_READ_OR_WRITE_EVT);
    }

    /* CCCD 쓰기 이벤트 : 슬레이브의 Notify 를 켜기 위해 CCCD 에 1을 기록 */
    if(events & START_WRITE_CCCD_EVT)
    {
        if(centralProcedureInProgress == FALSE)
        {
            attWriteReq_t req;

            req.cmd = FALSE;
            req.sig = FALSE;
            req.handle = centralCCCDHdl;
            req.len = 2;
            req.pValue = GATT_bm_alloc(centralConnHandle, ATT_WRITE_REQ, req.len, NULL, 0);
            if(req.pValue != NULL)
            {
                /* CCCD 값 = 0x0001 (Notification Enable) */
                req.pValue[0] = 1;
                req.pValue[1] = 0;

                if(GATT_WriteCharValue(centralConnHandle, &req, centralTaskId) == SUCCESS)
                {
                    centralProcedureInProgress = TRUE;
                }
                else
                {
                    GATT_bm_free((gattMsg_t*)&req, ATT_WRITE_REQ);
                }
            }
        }
        return (events ^ START_WRITE_CCCD_EVT);
    }

    /* RSSI 주기적 읽기 이벤트 */
    if(events & START_READ_RSSI_EVT)
    {
        GAPRole_ReadRssiCmd(centralConnHandle);
        tmos_start_task(centralTaskId, START_READ_RSSI_EVT, DEFAULT_RSSI_PERIOD);
        return (events ^ START_READ_RSSI_EVT);
    }

    /* ★ B22 디바운스 통과 → 실제 페어링 모드 진입 */
    if(events & B22_DEBOUNCE_EVT)
    {
        if(GPIOB_ReadPortPin(GPIO_Pin_22) == 0)   // 여전히 눌려 있으면 유효
        {
            PRINT("B22 pressed → Enter pairing mode\n");
            tmos_set_event(centralTaskId, START_PAIRING_MODE_EVT);
        }
        return (events ^ B22_DEBOUNCE_EVT);
    }


    /* ★ 페어링 모드 진입 */
    if(events & START_PAIRING_MODE_EVT)
    {
        /* 1) 기존 본딩 정보 전체 삭제 */
        GAPBondMgr_SetParameter(GAPBOND_ERASE_ALLBONDS, 0, NULL);
        PRINT("All bonds erased\n");

        /* 2) 모드 전환 */
        centralMode = CENTRAL_MODE_PAIRING;

        /* 3) 상태 초기화 */
        centralScanRes = 0;

        /* 4) 현재 스캔/연결 중이면 중단 후 재시작 */
        if(centralState == BLE_STATE_CONNECTED)
        {
            GAPRole_TerminateLink(centralConnHandle);
        }
        else
        {
            /* 연결 중이 아니면 이미 스캔 중이므로 그대로 둠.
             * PAIRING 모드로 바뀌었으니 다음 스캔 결과부터 필터 없이 동작. */
            PRINT("Scanning for any HID device...\n");
        }

        return (events ^ START_PAIRING_MODE_EVT);
    }
    /* 알 수 없는 이벤트는 무시 */
    return 0;
}

/*********************************************************************
 * @fn      central_ProcessTMOSMsg
 *
 * @brief   TMOS로부터 수신한 메시지 처리.
 *          GATT_MSG_EVENT(GATT 응답/이벤트)만 이 애플리케이션에 전달된다.
 *
 * @param   pMsg - 수신한 메시지
 *
 * @return  none
 */
static void central_ProcessTMOSMsg(tmos_event_hdr_t* pMsg)
{
    switch(pMsg->event)
    {
        case GATT_MSG_EVENT:
        centralProcessGATTMsg((gattMsgEvent_t*)pMsg);
        break;
    }
}

static uint8_t centralIsBleKeyboard(uint8_t* pData, uint8_t dataLen)
{
    uint8_t  i = 0;
    uint8_t  hasHidSvc = FALSE;
    uint8_t  isKeyboardAppear = FALSE;
    uint8_t  nameIsKeyboard = FALSE;

    if(pData == NULL || dataLen == 0)
        return FALSE;

    while(i < dataLen)
    {
        uint8_t len = pData[i];
        if(len == 0) break;
        if((uint16_t)i + len + 1 > dataLen) break;

        uint8_t type = pData[i + 1];

        switch(type)
        {
            case AD_TYPE_UUID16_LIST:
            case AD_TYPE_UUID16_LIST_CMP:
            {
                for(uint8_t j = 0; j + 1 < (len - 1); j += 2)
                {
                    uint16_t uuid = pData[i + 2 + j] | (pData[i + 3 + j] << 8);
                    if(uuid == HID_SERVICE_UUID)
                        hasHidSvc = TRUE;
                }
                break;
            }

            case AD_TYPE_APPEARANCE:
            {
                if(len >= 3)
                {
                    uint16_t appear = pData[i + 2] | (pData[i + 3] << 8);
                    if(appear == APPEARANCE_HID_KEYBOARD)   // 0x03C1
                        isKeyboardAppear = TRUE;
                }
                break;
            }

            case AD_TYPE_NAME_SHORT:
            case AD_TYPE_NAME_COMPLETE:
            {
                // "Keyboard" 또는 "keyboard" 포함 여부
                for(uint8_t j = 0; j + 8 <= (len - 1); j++)
                {
                    if((pData[i + 2 + j] == 'K' || pData[i + 2 + j] == 'k') &&
                       (pData[i + 3 + j] == 'e' || pData[i + 3 + j] == 'E') &&
                       (pData[i + 4 + j] == 'y' || pData[i + 4 + j] == 'Y') &&
                       (pData[i + 5 + j] == 'b' || pData[i + 5 + j] == 'B') &&
                       (pData[i + 6 + j] == 'o' || pData[i + 6 + j] == 'O') &&
                       (pData[i + 7 + j] == 'a' || pData[i + 7 + j] == 'A') &&
                       (pData[i + 8 + j] == 'r' || pData[i + 8 + j] == 'R') &&
                       (pData[i + 9 + j] == 'd' || pData[i + 9 + j] == 'D'))
                    {
                        nameIsKeyboard = TRUE;
                    }
                }
                break;
            }
            default: break;
        }

        i += len + 1;
    }

    return (isKeyboardAppear || (hasHidSvc && nameIsKeyboard)) ? TRUE : FALSE;
}

/*********************************************************************
 * @fn      centralIsBleMouse
 *
 * @brief   Advertising / Scan Response 데이터를 파싱해서
 *          "BLE HID Mouse" 인지 판단한다.
 *          - Appearance == 0x03C2 (Mouse) 이거나
 *          - HID Service(0x1812) 를 광고하면서 이름에 "Mouse" 포함 시 TRUE
 *
 * @param   pData   - AD structure 버퍼
 * @param   dataLen - 버퍼 길이
 *
 * @return  TRUE : 마우스 / FALSE : 그 외
 */
static uint8_t centralIsBleMouse(uint8_t* pData, uint8_t dataLen)
{
    uint8_t  i = 0;
    uint8_t  hasHidSvc = FALSE;
    uint8_t  isMouseAppear = FALSE;
    uint8_t  nameIsMouse = FALSE;

    if(pData == NULL || dataLen == 0)
        return FALSE;

    while(i < dataLen)
    {
        uint8_t len = pData[i];                    /* 이 AD structure 의 길이 */
        if(len == 0) break;                        /* 종료 */
        if((uint16_t)i + len + 1 > dataLen) break; /* 오버플로 방지 */

        uint8_t type = pData[i + 1];

        switch(type)
        {
            /* 16-bit Service UUID 리스트 */
            case AD_TYPE_UUID16_LIST:
            case AD_TYPE_UUID16_LIST_CMP:
            {
                for(uint8_t j = 0; j + 1 < (len - 1); j += 2)
                {
                    uint16_t uuid = pData[i + 2 + j] | (pData[i + 3 + j] << 8);
                    if(uuid == HID_SERVICE_UUID)
                        hasHidSvc = TRUE;
                }
                break;
            }

            /* Appearance */
            case AD_TYPE_APPEARANCE:
            {
                if(len >= 3)
                {
                    uint16_t appear = pData[i + 2] | (pData[i + 3] << 8);
                    if(appear == APPEARANCE_HID_MOUSE)
                        isMouseAppear = TRUE;
                }
                break;
            }

            /* 이름에 "Mouse"/"mouse" 포함 여부 (선택) */
            case AD_TYPE_NAME_SHORT:
            case AD_TYPE_NAME_COMPLETE:
            {
                for(uint8_t j = 0; j + 4 < len; j++)
                {
                    if((pData[i + 2 + j] == 'M' || pData[i + 2 + j] == 'm') &&
                       (pData[i + 3 + j] == 'o' || pData[i + 3 + j] == 'O') &&
                       (pData[i + 4 + j] == 'u' || pData[i + 4 + j] == 'U') &&
                       (pData[i + 5 + j] == 's' || pData[i + 5 + j] == 'S') &&
                       (pData[i + 6 + j] == 'e' || pData[i + 6 + j] == 'E'))
                    {
                        nameIsMouse = TRUE;
                    }
                }
                break;
            }
            default: break;
        }

        i += len + 1;   /* 다음 AD structure 로 이동 */
    }

    /* 조건 : Appearance == Mouse  OR  (HID 서비스 + 이름에 Mouse) */
    return (isMouseAppear || (hasHidSvc && nameIsMouse)) ? TRUE : FALSE;
}

/*********************************************************************
 * @fn      centralIsBondedDevice
 *
 * @brief   스캔된 디바이스가 SNV에 저장된 본딩 리스트에 있는지 확인.
 *          GAPBondMgr_ResolveAddr()는 RPA를 identity address로 변환하며,
 *          본딩되지 않은 주소는 실패를 반환한다.
 *
 * @return  TRUE : 본딩된 디바이스 / FALSE : 아님
 */
static uint8_t centralIsBondedDevice(uint8_t* pAddr, uint8_t addrType)
{
    uint8_t resolvedAddr[B_ADDR_LEN];

    if(GAPBondMgr_ResolveAddr(addrType, pAddr, resolvedAddr) == SUCCESS)
    {
        return TRUE;
    }
    return FALSE;
}

/*********************************************************************
 * @fn      centralProcessGATTMsg
 *
 * @brief   GATT 메시지 처리.
 *          - MTU 교환 응답
 *          - 읽기 응답 / 쓰기 응답 / 에러 응답
 *          - Notification 수신
 *          - Discovery 응답(서비스/특성/CCCD)
 *
 * @return  none
 */
static void centralProcessGATTMsg(gattMsgEvent_t* pMsg)
{
    /* 연결이 끊긴 뒤 도착한 GATT 메시지는 무시하고 버퍼만 반환 */
    if(centralState != BLE_STATE_CONNECTED)
    {
        GATT_bm_free(&pMsg->msg, pMsg->method);
        return;
    }

    /* MTU 교환 응답 또는 실패 처리 */
    if((pMsg->method == ATT_EXCHANGE_MTU_RSP) ||
       ((pMsg->method == ATT_ERROR_RSP) &&
        (pMsg->msg.errorRsp.reqOpcode == ATT_EXCHANGE_MTU_REQ)))
    {
        if(pMsg->method == ATT_ERROR_RSP)
        {
            uint8_t status = pMsg->msg.errorRsp.errCode;
            PRINT("Exchange MTU Error: %x\n", status);
        }
        centralProcedureInProgress = FALSE;
    }

    /* MTU 변경 이벤트 (스택에서 자동으로 발생) */
    if(pMsg->method == ATT_MTU_UPDATED_EVENT)
    {
        PRINT("MTU: %x\n", pMsg->msg.mtuEvt.MTU);
    }

    /* 읽기 응답 또는 읽기 에러 처리 */
    if((pMsg->method == ATT_READ_RSP) ||
       ((pMsg->method == ATT_ERROR_RSP) &&
        (pMsg->msg.errorRsp.reqOpcode == ATT_READ_REQ)))
    {
        if(pMsg->method == ATT_ERROR_RSP)
        {
            uint8_t status = pMsg->msg.errorRsp.errCode;
            PRINT("Read Error: %x\n", status);
        }
        else
        {
            /* 읽기 성공 → 첫 바이트 출력 */
            PRINT("Read rsp: %x\n", *pMsg->msg.readRsp.pValue);
        }
        centralProcedureInProgress = FALSE;
    }
    /* 쓰기 응답 또는 쓰기 에러 처리 */
    else if((pMsg->method == ATT_WRITE_RSP) ||
            ((pMsg->method == ATT_ERROR_RSP) &&
             (pMsg->msg.errorRsp.reqOpcode == ATT_WRITE_REQ)))
    {
        if(pMsg->method == ATT_ERROR_RSP)
        {
            uint8_t status = pMsg->msg.errorRsp.errCode;
            PRINT("Write Error: %x\n", status);
        }
        else
        {
            PRINT("Write success \n");
        }
        centralProcedureInProgress = FALSE;
    }
    /* 슬레이브에서 보낸 Notification 수신 */
    else if(pMsg->method == ATT_HANDLE_VALUE_NOTI)
    {
        uint8_t len = pMsg->msg.handleValueNoti.len;
        uint8_t* p = pMsg->msg.handleValueNoti.pValue;
#if 0
        PRINT("[hdl=%04X] len=%d : ", pMsg->msg.handleValueNoti.handle, len);
        for(uint8_t k = 0; k < len; k++) PRINT("%02X ", p[k]);
        PRINT("\n");
#endif
        DevHIDKeyReport(p);
    }
    /* Discovery 응답이면 별도 처리 함수로 위임 */
    else if(centralDiscState != BLE_DISC_STATE_IDLE)
    {
        centralGATTDiscoveryEvent(pMsg);
    }

    /* GATT 응답 버퍼 반환 */
    GATT_bm_free(&pMsg->msg, pMsg->method);
}

/*********************************************************************
 * @fn      centralRssiCB
 *
 * @brief   RSSI(수신 신호 강도) 콜백.
 *          - GAPRole_ReadRssiCmd() 호출에 대한 응답으로 호출됨.
 *
 * @param   connHandle - 연결 핸들
 * @param   rssi       - RSSI (dBm, 음수)
 *
 * @return  none
 */
static void centralRssiCB(uint16_t connHandle, int8_t rssi)
{
    //    PRINT("RSSI : -%d dB \n", -rssi);
}

/*********************************************************************
 * @fn      centralHciMTUChangeCB
 *
 * @brief   MTU 크기 변경 시 호출되는 콜백.
 *          - 여기서는 MTU 교환 절차(Exchange MTU)를 시작한다.
 *          - MTU 는 ATT 레벨에서 한 번에 주고받을 수 있는 최대 바이트 수.
 *
 * @param   maxTxOctets - 최대 송신 옥텟
 * @param   maxRxOctets - 최대 수신 옥텟
 *
 * @return  none
 */
static void centralHciMTUChangeCB(uint16_t connHandle, uint16_t maxTxOctets, uint16_t maxRxOctets)
{
    attExchangeMTUReq_t req;

    req.clientRxMTU = maxRxOctets;
    GATT_ExchangeMTU(connHandle, &req, centralTaskId);
    PRINT("exchange mtu:%d\n", maxRxOctets);
    centralProcedureInProgress = TRUE;
}

/*********************************************************************
 * @fn      centralEventCB
 *
 * @brief   GAP 이벤트 콜백.
 *          - 초기화 완료, 광고 수신, 스캔 완료, 연결 성립/해제,
 *            파라미터 갱신, PHY 갱신 등 처리.
 *
 * @param   pEvent - GAP 이벤트 구조체
 *
 * @return  none
 */
static void centralEventCB(gapRoleEvent_t* pEvent)
{
    switch(pEvent->gap.opcode)
    {
        /* 스택 초기화 완료 → 스캔 시작 */
        case GAP_DEVICE_INIT_DONE_EVENT:
        {
            PRINT("Discovering...\n");
            GAPRole_CentralStartDiscovery(DEFAULT_DISCOVERY_MODE,
                                          DEFAULT_DISCOVERY_ACTIVE_SCAN,
                                          DEFAULT_DISCOVERY_WHITE_LIST);
        }
        break;

        /* 스캔 중 디바이스 하나를 발견했을 때 */
        case GAP_DEVICE_INFO_EVENT:
        {
            /* ★ PAIRED_ONLY 모드에서는 본딩된 디바이스만 후보로 등록 */
            if(centralMode == CENTRAL_MODE_PAIRED_ONLY)
            {
                if(!centralIsBondedDevice(pEvent->deviceInfo.addr,
                                          pEvent->deviceInfo.addrType))
                {
                    break;   // 본딩 안 된 디바이스는 무시
                }
            }
#if 1
            if(centralIsBleKeyboard(pEvent->deviceInfo.pEvtData, pEvent->deviceInfo.dataLen))
#else
            if(centralIsBleMouse(pEvent->deviceInfo.pEvtData, pEvent->deviceInfo.dataLen))
#endif
            {
                PRINT("Recv Dev Info for HID\n");
                centralAddDeviceInfo(pEvent->deviceInfo.addr,
                                     pEvent->deviceInfo.addrType);
            }
        }
        break;

        /* 스캔이 완료되었을 때 (한 번 스캔 사이클 종료) */
        case GAP_DEVICE_DISCOVERY_EVENT:
        {
            if(centralScanRes > 0)
            {
                /* 리스트의 첫 번째 = 이번 스캔에서 필터를 통과한 마우스 */
                PRINT("HID found...\n");
                GAPRole_CentralEstablishLink(DEFAULT_LINK_HIGH_DUTY_CYCLE,
                                             DEFAULT_LINK_WHITE_LIST,
                                             centralDevList[0].addrType,
                                             centralDevList[0].addr);
                tmos_start_task(centralTaskId, ESTABLISH_LINK_TIMEOUT_EVT, ESTABLISH_LINK_TIMEOUT);
                PRINT("Connecting...\n");
            }
            else
            {
                /* ★ 발견 없으면 그냥 재스캔 (모드와 무관하게 항상 스캔 유지) */
                PRINT(centralMode == CENTRAL_MODE_PAIRED_ONLY
                      ? "No bonded device. Rescanning...\n"
                      : "No HID found. Rescanning...\n");
                centralScanRes = 0;
                GAPRole_CentralStartDiscovery(DEFAULT_DISCOVERY_MODE,
                                              DEFAULT_DISCOVERY_ACTIVE_SCAN,
                                              DEFAULT_DISCOVERY_WHITE_LIST);
            }
        }
        break;

        /* 연결이 성립(또는 실패)했을 때 */
        case GAP_LINK_ESTABLISHED_EVENT:
        {
            /* 연결 완료 → 타임아웃 타이머 중지 */
            tmos_stop_task(centralTaskId, ESTABLISH_LINK_TIMEOUT_EVT);

            if(pEvent->gap.hdr.status == SUCCESS)
            {
                centralState = BLE_STATE_CONNECTED;
                centralConnHandle = pEvent->linkCmpl.connectionHandle;
                centralProcedureInProgress = TRUE;

                /* 서비스 탐색을 일정 시간 후 시작 */
                tmos_start_task(centralTaskId, START_SVC_DISCOVERY_EVT, DEFAULT_SVC_DISCOVERY_DELAY);

                /* 연결 파라미터 갱신 예약 */
                if(centralParamUpdate)
                {
                    tmos_start_task(centralTaskId, START_PARAM_UPDATE_EVT, DEFAULT_PARAM_UPDATE_DELAY);
                }
#if defined(CH582)
                /* PHY 갱신 예약 */
                if(centralPhyUpdate)
                {
                    tmos_start_task(centralTaskId, START_PHY_UPDATE_EVT, DEFAULT_PHY_UPDATE_DELAY);
                }
#endif
                /* 주기적 RSSI 읽기 예약 */
                if(centralRssi)
                {
                    tmos_start_task(centralTaskId, START_READ_RSSI_EVT, DEFAULT_RSSI_PERIOD);
                }

                PRINT("Connected...\n");
            }
            else
            {
                /* 연결 실패 → 다시 스캔 시작 */
                PRINT("Connect Failed...Reason:%X\n", pEvent->gap.hdr.status);
                PRINT("Discovering...\n");
                centralScanRes = 0;
                GAPRole_CentralStartDiscovery(DEFAULT_DISCOVERY_MODE,
                                              DEFAULT_DISCOVERY_ACTIVE_SCAN,
                                              DEFAULT_DISCOVERY_WHITE_LIST);
            }
        }
        break;

        /* 연결이 끊겼을 때 */
        case GAP_LINK_TERMINATED_EVENT:
        {
            centralState = BLE_STATE_IDLE;
            centralConnHandle = GAP_CONNHANDLE_INIT;
            centralDiscState = BLE_DISC_STATE_IDLE;
            centralCharHdl = 0;
            
            centralReportFound = FALSE;
            centralCCCDHdl = 0;
            centralSvcStartHdl = 0;
            centralSvcEndHdl = 0;
            centralBonded = FALSE;

            centralScanRes = 0;
            centralProcedureInProgress = FALSE;

            tmos_stop_task(centralTaskId, START_READ_RSSI_EVT);

            PRINT("Disconnected...Reason:%x\n", pEvent->linkTerminate.reason);
            PRINT("Discovering...\n");

            /* ★ 항상 재스캔 */
            GAPRole_CentralStartDiscovery(DEFAULT_DISCOVERY_MODE,
                                          DEFAULT_DISCOVERY_ACTIVE_SCAN,
                                          DEFAULT_DISCOVERY_WHITE_LIST);
            /* PAIRING 모드는 B22 이벤트에서 스캔을 시작하므로 여기선 대기 */
        }
        break;

        /* 연결 파라미터 갱신 완료 이벤트 */
        case GAP_LINK_PARAM_UPDATE_EVENT:
        {
            PRINT("Param Update...\n");
        }
        break;
#if defined(CH582)
        /* PHY 갱신 완료 이벤트 */
        case GAP_PHY_UPDATE_EVENT:
        {
            PRINT("PHY Update...\n");
        }
        break;

        /* 확장 광고(Extended Advertising) 수신 */
        case GAP_EXT_ADV_DEVICE_INFO_EVENT:
        {
            PRINT("Recv ext adv \n");

            /* ★ PAIRED_ONLY 모드에서는 본딩된 디바이스만 후보로 등록 */
            if(centralMode == CENTRAL_MODE_PAIRED_ONLY)
            {
                if(!centralIsBondedDevice(pEvent->deviceInfo.addr,
                                          pEvent->deviceInfo.addrType))
                {
                    break;   // 본딩 안 된 디바이스는 무시
                }
            }

            /* 마우스일 때만 리스트에 추가 */
            if(centralIsBleKeyboard(pEvent->deviceInfo.pEvtData,
                                    pEvent->deviceInfo.dataLen))
            {
                centralAddDeviceInfo(pEvent->deviceInfo.addr,
                                     pEvent->deviceInfo.addrType);
            }
        }
        break;

        /* Directed Advertising(직접 광고) 수신 */
        case GAP_DIRECT_DEVICE_INFO_EVENT:
        {
            PRINT("Recv direct adv \n");
            centralAddDeviceInfo(pEvent->deviceDirectInfo.addr, pEvent->deviceDirectInfo.addrType);
        }
        break;
#endif

        default:
        break;
    }
}

/*********************************************************************
 * @fn      pairStateCB
 *
 * @brief   페어링/본딩 상태 콜백.
 *          - 페어링 시작/완료, 본딩 완료, 본딩 정보 저장 결과를 로그로 출력.
 *
 * @return  none
 */
static void centralPairStateCB(uint16_t connHandle, uint8_t state, uint8_t status)
{
    if(state == GAPBOND_PAIRING_STATE_STARTED)
    {
        PRINT("Pairing started:%d\n", status);
    }
    else if(state == GAPBOND_PAIRING_STATE_COMPLETE)
    {
        if(status == SUCCESS)
        {
            PRINT("Pairing success\n");

            /* ★ 페어링(=암호화) 완료 → 이제 CCCD Write 가능 */
            centralBonded = TRUE;
            centralProcedureInProgress = FALSE;

            if(centralCCCDHdl != 0)
            {
                tmos_start_task(centralTaskId, START_WRITE_CCCD_EVT,
                                DEFAULT_WRITE_CCCD_DELAY);
                PRINT("Schedule CCCD write after pairing\n");
            }
        }
        else
        {
            PRINT("Pairing fail\n");
        }
    }
    else if(state == GAPBOND_PAIRING_STATE_BONDED)
    {
        if(status == SUCCESS)
        {
            PRINT("Bonding success\n");

            /* ★ 재접속(기존 본딩) 시에도 CCCD Write 예약 */
            centralBonded = TRUE;
            centralProcedureInProgress = FALSE;

            if(centralCCCDHdl != 0)
            {
                tmos_start_task(centralTaskId, START_WRITE_CCCD_EVT,
                                DEFAULT_WRITE_CCCD_DELAY);
            }
        }
    }
    else if(state == GAPBOND_PAIRING_STATE_BOND_SAVED)
    {
        if(status == SUCCESS)
        {
            PRINT("Bond save success\n");

            /* ★ 신규 페어링 완료 → PAIRED_ONLY 모드로 복귀 */
            if(centralMode == CENTRAL_MODE_PAIRING)
            {
                centralMode = CENTRAL_MODE_PAIRED_ONLY;
                PRINT("Switch to PAIRED_ONLY mode\n");
            }
        }
        else
        {
            PRINT("Bond save failed: %d\n", status);
        }
    }
}

/*********************************************************************
 * @fn      centralPasscodeCB
 *
 * @brief   페어링 중 패스코드가 필요할 때 호출되는 콜백.
 *          - 여기서는 6자리 난수를 생성해 응답한다.
 *          - 실제 제품에서는 사용자에게 표시하거나 입력받아야 한다.
 *
 * @return  none
 */
static void centralPasscodeCB(uint8_t* deviceAddr, uint16_t connectionHandle,
                              uint8_t uiInputs, uint8_t uiOutputs)
{
    uint32_t passcode;

    /* 0 ~ 999999 사이의 난수 생성 */
    passcode = tmos_rand();
    passcode %= 1000000;

    /* 출력 능력이 있으면 표시(로그) */
    if(uiOutputs != 0)
    {
        PRINT("Passcode:%06d\n", (int)passcode);
    }

    /* 스택에 패스코드 응답 */
    GAPBondMgr_PasscodeRsp(connectionHandle, SUCCESS, passcode);
}

/*********************************************************************
 * @fn      centralStartDiscovery
 *
 * @brief   서비스 탐색 시작.
 *          - Simple Profile Service UUID 로 Primary Service 탐색을 요청한다.
 *          - 응답은 centralGATTDiscoveryEvent()에서 단계별로 처리된다.
 *
 * @return  none
 */
static void centralStartDiscovery(void)
{
    /* SimpleProfile 대신 HID Service UUID 사용 */
    uint8_t uuid[ATT_BT_UUID_SIZE] = {LO_UINT16(HID_SERVICE_UUID),
        HI_UINT16(HID_SERVICE_UUID)};

    centralSvcStartHdl = centralSvcEndHdl = centralCharHdl = 0;
    centralCCCDHdl = 0;
    centralReportFound = FALSE;    // ★ 추가
    centralBonded = FALSE;         // ★ 추가 (페어링 상태 재판정)

    centralDiscState = BLE_DISC_STATE_SVC;

    GATT_DiscPrimaryServiceByUUID(centralConnHandle,
                                  uuid,
                                  ATT_BT_UUID_SIZE,
                                  centralTaskId);
}

/*********************************************************************
 * @fn      centralGATTDiscoveryEvent
 *
 * @brief   GATT Discovery 응답 처리.
 *          - SVC  : 서비스 Handle 범위 획득 → Characteristic 탐색
 *          - CHAR : Characteristic 값 Handle 획득 → CCCD 탐색
 *          - CCCD : CCCD Handle 획득 → CCCD 에 Notify 설정 기록
 *
 * @return  none
 */
static void centralGATTDiscoveryEvent(gattMsgEvent_t* pMsg)
{
    attReadByTypeReq_t req;

    /* ---------- 1단계 : HID Service 찾기 ---------- */
    if(centralDiscState == BLE_DISC_STATE_SVC)
    {
        if(pMsg->method == ATT_FIND_BY_TYPE_VALUE_RSP &&
           pMsg->msg.findByTypeValueRsp.numInfo > 0)
        {
            /* start handle 만 신뢰. end handle 은 신뢰 불가 → 0xFFFF 로 두고
             * 어차피 HID 는 마지막 서비스라 문제 없음 */
            centralSvcStartHdl = ATT_ATTR_HANDLE(pMsg->msg.findByTypeValueRsp.pHandlesInfo, 0);
            centralSvcEndHdl = 0xFFFF;

            PRINT("HID Service start : %x\n", centralSvcStartHdl);
        }

        if((pMsg->method == ATT_FIND_BY_TYPE_VALUE_RSP &&
            pMsg->hdr.status == bleProcedureComplete) ||
           (pMsg->method == ATT_ERROR_RSP))
        {
            if(centralSvcStartHdl != 0)
            {
                centralDiscState = BLE_DISC_STATE_CHAR;

                /* ★ 모든 Characteristic Declaration(0x2803) 을 나열 */
                GATT_DiscAllChars(centralConnHandle,
                                  centralSvcStartHdl,
                                  centralSvcEndHdl,
                                  centralTaskId);
                PRINT("Discovering chars...\n");
            }
            else
            {
                PRINT("HID Service not found\n");
                centralDiscState = BLE_DISC_STATE_IDLE;
            }
        }
    }
    /* ---------- 2단계 : Characteristic 파싱 → Report(0x2A4D) 찾기 ---------- */
    else if(centralDiscState == BLE_DISC_STATE_CHAR)
    {
        if(pMsg->method == ATT_READ_BY_TYPE_RSP &&
           pMsg->msg.readByTypeRsp.numPairs > 0)
        {
            uint8_t pairLen = pMsg->msg.readByTypeRsp.len;
            if(pairLen == 0) pairLen = 7;

            for(uint8_t i = 0; i < pMsg->msg.readByTypeRsp.numPairs; i++)
            {
                uint8_t* p = pMsg->msg.readByTypeRsp.pDataList + (i * pairLen);

                uint16_t charHdl = BUILD_UINT16(p[0], p[1]);
                uint8_t  props = p[2];
                uint16_t valueHdl = BUILD_UINT16(p[3], p[4]);
                uint16_t uuid = BUILD_UINT16(p[5], p[6]);

                PRINT("  Char: decl=%x props=%02X val=%x uuid=%04X\n",
                      charHdl, props, valueHdl, uuid);

                /* ★ 첫 번째 Notify 지원 Report 만 선택, 이후 Report 는 무시 */
                if(!centralReportFound &&
                   uuid == HID_REPORT_CHAR_UUID &&
                   (props & 0x10))               /* Notify bit */
                {
                    centralCharHdl = valueHdl;
                    centralReportFound = TRUE;
                    PRINT("  → Selected Report val handle = %x (props=%02X)\n",
                          valueHdl, props);
                }
            }
        }

        if((pMsg->method == ATT_READ_BY_TYPE_RSP &&
            pMsg->hdr.status == bleProcedureComplete) ||
           (pMsg->method == ATT_ERROR_RSP))
        {
            if(!centralReportFound)
            {
                PRINT("Report char not found\n");
                centralDiscState = BLE_DISC_STATE_IDLE;
            }
            else
            {
                /* ★ CCCD 는 Report value handle 바로 뒤 2~3 개 handle 안에 있음.
                 *   (Report Reference 0x2908 + CCCD 0x2902) */
                centralDiscState = BLE_DISC_STATE_CCCD;

                req.startHandle = centralCharHdl + 1;
                req.endHandle = centralCharHdl + 3;
                req.type.len = ATT_BT_UUID_SIZE;
                req.type.uuid[0] = LO_UINT16(GATT_CLIENT_CHAR_CFG_UUID);
                req.type.uuid[1] = HI_UINT16(GATT_CLIENT_CHAR_CFG_UUID);

                GATT_ReadUsingCharUUID(centralConnHandle, &req, centralTaskId);
            }
        }
    }
    /* ---------- 3단계 : CCCD 찾기 ---------- */
    else if(centralDiscState == BLE_DISC_STATE_CCCD)
    {
        if(pMsg->method == ATT_READ_BY_TYPE_RSP &&
           pMsg->msg.readByTypeRsp.numPairs > 0)
        {
            centralCCCDHdl = BUILD_UINT16(pMsg->msg.readByTypeRsp.pDataList[0],
                                          pMsg->msg.readByTypeRsp.pDataList[1]);
            PRINT("Report CCCD handle : %x\n", centralCCCDHdl);

            /* ★ 여기서는 바로 쓰지 않는다.
             *   페어링이 이미 끝난 상태(재연결)라면 바로 쓰고,
             *   아니면 centralPairStateCB 에서 쓰도록 미룬다. */
            if(centralBonded)
            {
                centralProcedureInProgress = FALSE;
                tmos_start_task(centralTaskId, START_WRITE_CCCD_EVT,
                                DEFAULT_WRITE_CCCD_DELAY);
            }
            else
            {
                PRINT("Wait pairing before CCCD write...\n");
            }
        }
        centralDiscState = BLE_DISC_STATE_IDLE;
    }
}

/*********************************************************************
 * @fn      centralAddDeviceInfo
 *
 * @brief   스캔 결과 리스트에 디바이스를 추가.
 *          - 이미 등록된 주소는 중복 추가하지 않는다.
 *          - 최대 DEFAULT_MAX_SCAN_RES 개까지만 저장.
 *
 * @return  none
 */
static void centralAddDeviceInfo(uint8_t* pAddr, uint8_t addrType)
{
    uint8_t i;

    if(centralScanRes < DEFAULT_MAX_SCAN_RES)
    {
        /* 중복 검사 */
        for(i = 0; i < centralScanRes; i++)
        {
            if(tmos_memcmp(pAddr, centralDevList[i].addr, B_ADDR_LEN))
            {
                return;
            }
        }

        /* 새 디바이스 등록 */
        tmos_memcpy(centralDevList[centralScanRes].addr, pAddr, B_ADDR_LEN);
        centralDevList[centralScanRes].addrType = addrType;
        centralScanRes++;

        /* 로그 출력 (주소는 6바이트, LSB first 순서) */
        PRINT("Device %d - Addr %x %x %x %x %x %x \n", centralScanRes,
              centralDevList[centralScanRes - 1].addr[0],
              centralDevList[centralScanRes - 1].addr[1],
              centralDevList[centralScanRes - 1].addr[2],
              centralDevList[centralScanRes - 1].addr[3],
              centralDevList[centralScanRes - 1].addr[4],
              centralDevList[centralScanRes - 1].addr[5]);
    }
}


/*********************************************************************
 * @fn      GPIOB_IRQHandler
 *
 * @brief   B22 버튼 눌림 → 페어링 모드 진입 이벤트 예약
 */
__INTERRUPT
__HIGH_CODE
void GPIOB_IRQHandler(void)
{
    if(GPIOB_ReadITFlagBit(GPIO_Pin_22))
    {
        GPIOB_ClearITFlagBit(GPIO_Pin_22);
        /* 디바운스: 짧은 시간 후 이벤트 발생 */
        tmos_start_task(centralTaskId, B22_DEBOUNCE_EVT, B22_DEBOUNCE_DELAY);
    }
}


void AppBLE_Init(void)
{

    /* CH58x BLE 스택 초기화 (라디오, 프로토콜 스택, 메모리 힙 등) */
#if defined(CH582)
    CH58X_BLEInit();
#elif defined(CH573)
    CH57X_BLEInit();
#else
    #error "CH58x/CH57x BLE 칩이 아님"
#endif

    /* HAL(하드웨어 추상화 계층) 초기화 */
    HAL_Init();

    /* GAP Role을 Central(마스터) 역할로 초기화 */
    GAPRole_CentralInit();

    /* 애플리케이션 태스크 초기화 (이벤트 등록, 콜백 등록 등) */
    Central_Init();

}

/************************ endfile @ central **************************/



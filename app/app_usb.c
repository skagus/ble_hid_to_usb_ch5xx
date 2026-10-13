
#include "com.h"
#include "app_usb.h"

#define DevEP0SIZE    0x40
// Device descriptor
const uint8_t MyDevDescr[] = {0x12, 0x01, 0x10, 0x01, 0x00, 0x00, 0x00, DevEP0SIZE, 0x3d, 0x41, 0x07, 0x21, 0x00, 0x00,
0x00, 0x00, 0x00, 0x01};
// Configuration descriptor
const uint8_t MyCfgDescr[] = {
    0x09, 0x02, 0x22, 0x00, 0x01, 0x01, 0x00, 0xA0, 0x32, // Configuration descriptor (총 0x22, 1 interface)
    0x09, 0x04, 0x00, 0x00, 0x01, 0x03, 0x01, 0x01, 0x00, // Interface descriptor, keyboard
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, 0x3e, 0x00, // HID class descriptor
    0x07, 0x05, 0x81, 0x03, 0x08, 0x00, 0x0a              // Endpoint descriptor (EP1 IN)
};

/* USB speed matching descriptor */
const uint8_t My_QueDescr[] = {0x0A, 0x06, 0x00, 0x02, 0xFF, 0x00, 0xFF, 0x40, 0x01, 0x00};

/* USB full-speed mode, other speed configuration descriptor */
uint8_t USB_FS_OSC_DESC[sizeof(MyCfgDescr)] = {
    0x09, 0x07, /* 나머지 부분은 프로그램에서 복사 */
};

// Language descriptor
const uint8_t MyLangDescr[] = {0x04, 0x03, 0x09, 0x04};
// Manufacturer info
const uint8_t MyManuInfo[] = {0x0E, 0x03, 'w', 0, 'c', 0, 'h', 0, '.', 0, 'c', 0, 'n', 0};
// Product info
const uint8_t MyProdInfo[] = {0x0C, 0x03, 'C', 0, 'H', 0, '5', 0, '7', 0, 'x', 0};
/* HID class report descriptor */
const uint8_t KeyRepDesc[] = {0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x05, 0x07, 0x19, 0xe0, 0x29, 0xe7, 0x15, 0x00, 0x25,
0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02, 0x95, 0x01, 0x75, 0x08, 0x81, 0x01, 0x95, 0x03,
0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x03, 0x91, 0x02, 0x95, 0x05, 0x75, 0x01, 0x91,
0x01, 0x95, 0x06, 0x75, 0x08, 0x26, 0xff, 0x00, 0x05, 0x07, 0x19, 0x00, 0x29, 0x91, 0x81,
0x00, 0xC0};

/**********************************************************/
uint8_t        DevConfig, Ready;
uint8_t        SetupReqCode;
uint16_t       SetupReqLen; // Left length of data stage
const uint8_t* pDescr;      // Pointer to descriptor data for next transfer
uint8_t        Report_Value = 0x00; // Keep track of the current protocol (0=Boot, 1=Report)
uint8_t        Idle_Value = 0x00; // Keep track of the current idle rate (in 4ms units)
uint8_t        USB_SleepStatus = 0x00; // USB sleep 상태

/* 마우스/키보드 데이터 */
uint8_t HIDKey[8] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

#define KEY_REPORT_QUEUE_SIZE 16
typedef struct {
    uint8_t report[HID_KEYBOARD_REPORT_LEN];
} key_report_t;

static key_report_t sKeyReportQueue[KEY_REPORT_QUEUE_SIZE];
static volatile uint8_t sKeyQueueHead = 0;
static volatile uint8_t sKeyQueueTail = 0;
static volatile uint8_t sKeyEP1Busy = 0;

static void DevHIDKeyQueueReset(void)
{
    sKeyQueueHead = 0;
    sKeyQueueTail = 0;
    sKeyEP1Busy = 0;
}

/******** 사용자 정의 Endpoint RAM 할당 ****************************************/
__attribute__((aligned(4))) uint8_t EP0_Databuf[64 + 64 + 64]; //ep0(64)+ep4_out(64)+ep4_in(64)
__attribute__((aligned(4))) uint8_t EP1_Databuf[64 + 64];      //ep1_out(64)+ep1_in(64)
//__attribute__((aligned(4))) uint8_t EP2_Databuf[64 + 64];      //ep2_out(64)+ep2_in(64)

/*********************************************************************
 * @fn      USB_DevTransProcess
 *
 * @brief   USB transfer 처리 함수
 *
 * @return  none
 */
void USB_DevTransProcess(void)
{
    uint8_t len, bmReqType;
    uint8_t bmIntFlag, bError = 0;

    bmIntFlag = R8_USB_INT_FG;
    if(bmIntFlag & RB_UIF_TRANSFER)
    {
        if((R8_USB_INT_ST & MASK_UIS_TOKEN) != MASK_UIS_TOKEN) // non-idle
        {
            switch(R8_USB_INT_ST & (MASK_UIS_TOKEN | MASK_UIS_ENDP)) // token과 endpoint 번호 분석
            {
                case UIS_TOKEN_IN:
                {
                    switch(SetupReqCode)
                    {
                        case USB_GET_DESCRIPTOR:
                        len = (SetupReqLen >= DevEP0SIZE) ? DevEP0SIZE : SetupReqLen; // 이번 transfer 길이
                        memcpy(pEP0_DataBuf, pDescr, len);                          /* 업로드 데이터 로드 */
                        SetupReqLen -= len;
                        pDescr += len;
                        R8_UEP0_T_LEN = len;
                        R8_UEP0_CTRL ^= RB_UEP_T_TOG; // toggle
                        break;

                        case USB_SET_ADDRESS:
                        R8_USB_DEV_AD = (R8_USB_DEV_AD & RB_UDA_GP_BIT) | SetupReqLen;
                        R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
                        break;

                        case USB_SET_FEATURE:
                        break;

                        default:
                        R8_UEP0_T_LEN = 0; // status stage 완료 또는 강제 0-length packet으로 control transfer 종료
                        R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
                        break;
                    }
                    break;
                }

                case UIS_TOKEN_OUT:
                {
                    len = R8_USB_RX_LEN;
                    char aLed[4] = {'_', '_', '_', 0};
                    if(SetupReqCode == 0x09)
                    {
                        if(pEP0_DataBuf[0] & 0x1) aLed[0] = 'N'; // Num Lock LED
                        if(pEP0_DataBuf[0] & 0x2) aLed[1] = 'C'; // Caps Lock LED
                        if(pEP0_DataBuf[0] & 0x4) aLed[2] = 'S'; // Scroll Lock LED
                    }
                    PRINT("LED: %s\n", aLed);
                    break;
                }


                case UIS_TOKEN_IN | 1:   // Keyboard IN
                {
                    R8_UEP1_CTRL ^= RB_UEP_T_TOG;
                    R8_UEP1_CTRL = (R8_UEP1_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;

                    if(sKeyQueueHead != sKeyQueueTail)
                    {
                        memcpy(pEP1_IN_DataBuf, sKeyReportQueue[sKeyQueueTail].report, HID_KEYBOARD_REPORT_LEN);
                        sKeyQueueTail = (sKeyQueueTail + 1) % KEY_REPORT_QUEUE_SIZE;
                        DevEP1_IN_Deal(HID_KEYBOARD_REPORT_LEN);
                    }
                    else
                    {
                        sKeyEP1Busy = 0;
                    }
                    break;
                }
#if 0
                case UIS_TOKEN_IN | 2:  // Mouse IN
                {
                    R8_UEP2_CTRL ^= RB_UEP_T_TOG;
                    R8_UEP2_CTRL = (R8_UEP2_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
                    break;
                }
#endif
                default:
                break;
            }
            R8_USB_INT_FG = RB_UIF_TRANSFER;
        }

        if(R8_USB_INT_ST & RB_UIS_SETUP_ACT) // Setup packet 처리
        {
            R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;
            SetupReqLen = pSetupReqPak->wLength;
            SetupReqCode = pSetupReqPak->bRequest;
            bmReqType = pSetupReqPak->bRequestType;

            len = 0;
            bError = 0;
            if((bmReqType & USB_REQ_TYP_MASK) != USB_REQ_TYP_STANDARD)
            {
                /* 비표준 요청 */
                /* 기타 요청, class request, vendor request 등 */
                if(bmReqType & 0x40)
                {
                    /* Vendor request */
                }
                else if(bmReqType & 0x20)
                {
                    switch(SetupReqCode)
                    {
                        case DEF_USB_SET_IDLE: /* 0x0A: SET_IDLE */
                        Idle_Value = EP0_Databuf[3];
                        break; // 반드시 필요

                        case DEF_USB_SET_REPORT: /* 0x09: SET_REPORT */
                        break;

                        case DEF_USB_SET_PROTOCOL: /* 0x0B: SET_PROTOCOL */
                        Report_Value = EP0_Databuf[2];
                        break;

                        case DEF_USB_GET_IDLE: /* 0x02: GET_IDLE */
                        EP0_Databuf[0] = Idle_Value;
                        len = 1;
                        break;

                        case DEF_USB_GET_PROTOCOL: /* 0x03: GET_PROTOCOL */
                        EP0_Databuf[0] = Report_Value;
                        len = 1;
                        break;

                        default:
                        bError = 0xFF;
                    }
                }
            }
            else /* 표준 요청 */
            {
                switch(SetupReqCode)
                {
                    case USB_GET_DESCRIPTOR:
                    {
                        switch(((pSetupReqPak->wValue) >> 8))
                        {
                            case USB_DESCR_TYP_DEVICE:
                            {
                                pDescr = MyDevDescr;
                                len = MyDevDescr[0];
                                break;
                            }

                            case USB_DESCR_TYP_CONFIG:
                            {
                                pDescr = MyCfgDescr;
                                len = MyCfgDescr[2];
                                break;
                            }

                            case USB_DESCR_TYP_HID:
                            {
                                switch((pSetupReqPak->wIndex) & 0xff)
                                {
                                    /* Interface 선택 */
                                    case 0:
                                    pDescr = (uint8_t*)(&MyCfgDescr[18]);
                                    len = 9;
                                    break;
#if 0
                                    case 1:
                                    pDescr = (uint8_t*)(&MyCfgDescr[43]);
                                    len = 9;
                                    break;
#endif
                                    default:
                                    /* 지원하지 않는 string descriptor */
                                    bError = 0xff;
                                    break;
                                }
                                break;
                            }
                            case USB_DESCR_TYP_REPORT:
                            {
                                if(((pSetupReqPak->wIndex) & 0xff) == 0) // Interface 0 report descriptor
                                {
                                    pDescr = KeyRepDesc; // 업로드 데이터 준비
                                    len = sizeof(KeyRepDesc);
                                }
#if 0
                                else if(((pSetupReqPak->wIndex) & 0xff) == 1) // Interface 1 report descriptor
                                {
                                    pDescr = MouseRepDesc; // 업로드 데이터 준비
                                    len = sizeof(MouseRepDesc);
                                    Ready = 1; // 추가 interface가 있다면 마지막 interface 설정 완료 후 유효해야 함
                                }
#endif
                                else
                                {
                                    bError = 0xff; // 지원하지 않는 인터페이스는 STALL 처리
                                }
                                break;
                            }

                            case USB_DESCR_TYP_STRING:
                            {
                                switch((pSetupReqPak->wValue) & 0xff)
                                {
                                    case 1:
                                    pDescr = MyManuInfo;
                                    len = MyManuInfo[0];
                                    break;
                                    case 2:
                                    pDescr = MyProdInfo;
                                    len = MyProdInfo[0];
                                    break;
                                    case 0:
                                    pDescr = MyLangDescr;
                                    len = MyLangDescr[0];
                                    break;
                                    default:
                                    bError = 0xFF; // 지원하지 않는 string descriptor
                                    break;
                                }
                                break;
                            }

                            case 0x06:
                            pDescr = (uint8_t*)(&My_QueDescr[0]);
                            len = sizeof(My_QueDescr);
                            break;

                            case 0x07:
                            memcpy(&USB_FS_OSC_DESC[2], &MyCfgDescr[2], sizeof(MyCfgDescr) - 2);
                            pDescr = (uint8_t*)(&USB_FS_OSC_DESC[0]);
                            len = sizeof(USB_FS_OSC_DESC);
                            break;

                            default:
                            bError = 0xff;
                            break;
                        }
                        if(bError != 0xff)
                        {
                            if(SetupReqLen > len) SetupReqLen = len; // 실제 업로드 총 길이
                            len = (SetupReqLen >= DevEP0SIZE) ? DevEP0SIZE : SetupReqLen;
                            memcpy(pEP0_DataBuf, pDescr, len);
                            pDescr += len;
                        }
                        break;
                    }

                    case USB_SET_ADDRESS:
                    SetupReqLen = (pSetupReqPak->wValue) & 0xff;
                    break;

                    case USB_GET_CONFIGURATION:
                    pEP0_DataBuf[0] = DevConfig;
                    if(SetupReqLen > 1) SetupReqLen = 1;
                    break;

                    case USB_SET_CONFIGURATION:
                    DevConfig = (pSetupReqPak->wValue) & 0xff;
                    break;

                    case USB_CLEAR_FEATURE:
                    {
                        if((pSetupReqPak->bRequestType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_ENDP) // Endpoint
                        {
                            switch((pSetupReqPak->wIndex) & 0xff)
                            {
#if 0
                                case 0x82:
                                R8_UEP2_CTRL = (R8_UEP2_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) | UEP_T_RES_NAK;
                                break;
#endif
                                case 0x81:
                                R8_UEP1_CTRL = (R8_UEP1_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) | UEP_T_RES_NAK;
                                break;
#if 0                                    
                                case 0x02:
                                R8_UEP2_CTRL = (R8_UEP2_CTRL & ~(RB_UEP_R_TOG | MASK_UEP_R_RES)) | UEP_R_RES_ACK;
                                break;
                                case 0x83:
                                R8_UEP3_CTRL = (R8_UEP3_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) | UEP_T_RES_NAK;
                                break;
                                case 0x03:
                                R8_UEP3_CTRL = (R8_UEP3_CTRL & ~(RB_UEP_R_TOG | MASK_UEP_R_RES)) | UEP_R_RES_ACK;
                                break;
                                case 0x01:
                                R8_UEP1_CTRL = (R8_UEP1_CTRL & ~(RB_UEP_R_TOG | MASK_UEP_R_RES)) | UEP_R_RES_ACK;
                                break;
#endif
                                default:
                                bError = 0xFF; // 지원하지 않는 endpoint
                                break;
                            }
                        }
                        else if((pSetupReqPak->bRequestType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_DEVICE)
                        {
                            if(pSetupReqPak->wValue == 1)
                            {
                                USB_SleepStatus &= ~0x01;
                            }
                        }
                        else
                        {
                            bError = 0xFF;
                        }
                        break;
                    }

                    case USB_SET_FEATURE:
                    if((pSetupReqPak->bRequestType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_ENDP)
                    {
                        /* Endpoint */
                        switch(pSetupReqPak->wIndex)
                        {
#if 0                            
                            case 0x82:
                            R8_UEP2_CTRL = (R8_UEP2_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) | UEP_T_RES_STALL;
                            break;
#endif
                            case 0x81:
                            R8_UEP1_CTRL = (R8_UEP1_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) | UEP_T_RES_STALL;
                            break;
#if 0
                            case 0x83:
                            R8_UEP3_CTRL = (R8_UEP3_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) | UEP_T_RES_STALL;
                            break;
                            case 0x03:
                            R8_UEP3_CTRL = (R8_UEP3_CTRL & ~(RB_UEP_R_TOG | MASK_UEP_R_RES)) | UEP_R_RES_STALL;
                            break;
                            case 0x02:
                            R8_UEP2_CTRL = (R8_UEP2_CTRL & ~(RB_UEP_R_TOG | MASK_UEP_R_RES)) | UEP_R_RES_STALL;
                            break;
                            case 0x01:
                            R8_UEP1_CTRL = (R8_UEP1_CTRL & ~(RB_UEP_R_TOG | MASK_UEP_R_RES)) | UEP_R_RES_STALL;
                            break;
#endif
                            default:
                            /* 지원하지 않는 endpoint */
                            bError = 0xFF;
                            break;
                        }
                    }
                    else if((pSetupReqPak->bRequestType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_DEVICE)
                    {
                        if(pSetupReqPak->wValue == 1)
                        {
                            /* Sleep 설정 */
                            USB_SleepStatus |= 0x01;
                        }
                    }
                    else
                    {
                        bError = 0xFF;
                    }
                    break;

                    case USB_GET_INTERFACE:
                    pEP0_DataBuf[0] = 0x00;
                    if(SetupReqLen > 1) SetupReqLen = 1;
                    break;

                    case USB_SET_INTERFACE:
                    break;

                    case USB_GET_STATUS:
                    if((pSetupReqPak->bRequestType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_ENDP)
                    {
                        /* Endpoint */
                        pEP0_DataBuf[0] = 0x00;
                        switch(pSetupReqPak->wIndex)
                        {
#if 0                            
                            case 0x82:
                            if((R8_UEP2_CTRL & (RB_UEP_T_TOG | MASK_UEP_T_RES)) == UEP_T_RES_STALL)
                            {
                                pEP0_DataBuf[0] = 0x01;
                            }
                            break;
#endif
                            case 0x81:
                            if((R8_UEP1_CTRL & (RB_UEP_T_TOG | MASK_UEP_T_RES)) == UEP_T_RES_STALL)
                            {
                                pEP0_DataBuf[0] = 0x01;
                            }
                            break;
#if 0
                            case 0x83:
                            if((R8_UEP3_CTRL & (RB_UEP_T_TOG | MASK_UEP_T_RES)) == UEP_T_RES_STALL)
                            {
                                pEP0_DataBuf[0] = 0x01;
                            }
                            break;

                            case 0x03:
                            if((R8_UEP3_CTRL & (RB_UEP_R_TOG | MASK_UEP_R_RES)) == UEP_R_RES_STALL)
                            {
                                pEP0_DataBuf[0] = 0x01;
                            }
                            break;

                            case 0x02:
                            if((R8_UEP2_CTRL & (RB_UEP_R_TOG | MASK_UEP_R_RES)) == UEP_R_RES_STALL)
                            {
                                pEP0_DataBuf[0] = 0x01;
                            }
                            break;

                            case 0x01:
                            if((R8_UEP1_CTRL & (RB_UEP_R_TOG | MASK_UEP_R_RES)) == UEP_R_RES_STALL)
                            {
                                pEP0_DataBuf[0] = 0x01;
                            }
                            break;
#endif
                        }
                    }
                    else if((pSetupReqPak->bRequestType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_DEVICE)
                    {
                        pEP0_DataBuf[0] = 0x00;
                        if(USB_SleepStatus) pEP0_DataBuf[0] = 0x02;
                        else pEP0_DataBuf[0] = 0x00;
                    }
                    pEP0_DataBuf[1] = 0;
                    if(SetupReqLen >= 2) SetupReqLen = 2;
                    break;

                    default:
                    bError = 0xff;
                    break;
                }
            }

            if(bError == 0xff) // 오류 또는 미지원
            {
                //                  SetupReqCode = 0xFF;
                R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_STALL | UEP_T_RES_STALL; // STALL
            }
            else
            {
                if(bmReqType & 0x80) // 업로드
                {
                    len = (SetupReqLen > DevEP0SIZE) ? DevEP0SIZE : SetupReqLen;
                    SetupReqLen -= len;
                }
                else
                    len = 0; // 다운로드
                R8_UEP0_T_LEN = len;
                R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_ACK; // 기본 packet은 DATA1
            }

            R8_USB_INT_FG = RB_UIF_TRANSFER;
        }
    }
    else if(bmIntFlag & RB_UIF_BUS_RST)
    {
        R8_USB_DEV_AD = 0;
        R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
        R8_UEP1_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
//        R8_UEP2_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
//        R8_UEP3_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
        DevHIDKeyQueueReset();
        R8_USB_INT_FG = RB_UIF_BUS_RST;
    }
    else if(bmIntFlag & RB_UIF_SUSPEND)
    {
        if(R8_USB_MIS_ST & RB_UMS_SUSPEND)
        {
            ;
        } // Suspend
        else
        {
            ;
        } // Wakeup
        R8_USB_INT_FG = RB_UIF_SUSPEND;
    }
    else
    {
        R8_USB_INT_FG = bmIntFlag;
    }
}

#if 0
/*********************************************************************
 * @fn      DevHIDMouseReport
 *
 * @brief   마우스 데이터 보고
 *
 * @return  none
 */
void DevHIDMouseReport(uint8_t mouse)
{
    HIDMouse[0] = mouse;
    memcpy(pEP2_IN_DataBuf, HIDMouse, sizeof(HIDMouse));
    DevEP2_IN_Deal(sizeof(HIDMouse));
}
#endif

/*********************************************************************
 * @fn      DevHIDKeyReport
 *
 * @brief   키보드 데이터 보고 (전송 큐를 통해 호스트 수신 전 덮어쓰기 방지)
 *
 * @return  none
 */
void DevHIDKeyReport(uint8_t* aKey, uint8_t nLen)
{
    uint8_t tempReport[HID_KEYBOARD_REPORT_LEN];
    uint8_t nextHead;

#if 1
    PRINT("[hdl=%04X] len=%d : ", aKey, nLen);
    for(uint8_t k = 0; k < nLen; k++) PRINT("%02X ", aKey[k]);
    PRINT("\n");
#endif


    memset(tempReport, 0, HID_KEYBOARD_REPORT_LEN);
    if(aKey != NULL && nLen > 0)
    {
        if(nLen > HID_KEYBOARD_REPORT_LEN) nLen = HID_KEYBOARD_REPORT_LEN;
        memcpy(tempReport, aKey, nLen);
    }

    PFIC_DisableIRQ(USB_IRQn);

    if(!sKeyEP1Busy)
    {
        /* 전송 중이 아니면 즉시 DMA 버퍼로 복사 후 전송 */
        memcpy(pEP1_IN_DataBuf, tempReport, HID_KEYBOARD_REPORT_LEN);
        sKeyEP1Busy = 1;
        DevEP1_IN_Deal(HID_KEYBOARD_REPORT_LEN);
    }
    else
    {
        /* 이미 전송 중이면 큐에 적재하여 다음 완료 인터럽트에서 전송 */
        nextHead = (sKeyQueueHead + 1) % KEY_REPORT_QUEUE_SIZE;
        if(nextHead != sKeyQueueTail)
        {
            memcpy(sKeyReportQueue[sKeyQueueHead].report, tempReport, HID_KEYBOARD_REPORT_LEN);
            sKeyQueueHead = nextHead;
        }
        else
        {
            /* 큐 오버플로우 시 최신 상태 보존을 위해 마지막 슬롯 갱신 */
            uint8_t lastIdx = (sKeyQueueHead + KEY_REPORT_QUEUE_SIZE - 1) % KEY_REPORT_QUEUE_SIZE;
            memcpy(sKeyReportQueue[lastIdx].report, tempReport, HID_KEYBOARD_REPORT_LEN);
        }
    }

    PFIC_EnableIRQ(USB_IRQn);
}

/*********************************************************************
 * @fn      USB_IRQHandler
 *
 * @brief   USB 인터럽트 함수
 *
 * @return  none
 */
__INTERRUPT
__HIGH_CODE
void USB_IRQHandler(void) /* USB interrupt service routine, register bank 1 사용 */
{
    USB_DevTransProcess();
}

void AppUSB_Init(void)
{
    DevHIDKeyQueueReset();
    pEP0_RAM_Addr = EP0_Databuf;
    pEP1_RAM_Addr = EP1_Databuf;
//    pEP2_RAM_Addr = EP2_Databuf;
//    pEP3_RAM_Addr = EP3_Databuf;

    USB_DeviceInit();

    PFIC_EnableIRQ(USB_IRQn);
}


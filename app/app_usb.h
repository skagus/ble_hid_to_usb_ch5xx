#pragma once

#define HID_KEYBOARD_REPORT_LEN     (8)
#define HID_CONSUMER_REPORT_LEN     (2)

#define HID_REPORT_ID_KBD           (1)
#define HID_REPORT_ID_CONSUMER      (2)

/* 키보드 리포트 전송 (Report ID 1 + 8바이트 키보드 데이터) */
void DevHIDKeyReport(uint8_t* aKey, uint8_t nLen);

/* Consumer Control 리포트 전송 (Report ID 2 + 2바이트 Usage 코드) */
void DevHIDConsumerReport(uint8_t* aConsumer, uint8_t nLen);

void AppUSB_Init(void);

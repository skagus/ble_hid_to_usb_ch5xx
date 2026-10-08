#pragma once

#define HID_KEYBOARD_REPORT_LEN     (8)

//void DevHIDMouseReport(uint8_t mouse);
void DevHIDKeyReport(uint8_t* aKey, uint8_t nLen);

void AppUSB_Init(void);

# BLE 미디어 키(Consumer Control) 지원 구현 계획

## 1. 문제 분석

### 현재 구조
- **BLE 수신** (`app_ble.c`): HID Service에서 첫 번째 `Notify` 가능한 Characteristic(0x2A4D 또는 0x2A22)만 `centralCharHdl`에 저장하고 해당 핸들의 Notification만 처리함.
- **USB 전송** (`app_usb.c`): HID Report Descriptor에 일반 키보드(Usage Page 0x01, Usage 0x06) 만 정의되어 있으며, 8바이트 리포트만 EP1 IN으로 전송함.

### 미디어 키(Consumer Control)가 안 되는 이유

| 원인 | 위치 | 현재 코드 |
|------|------|-----------|
| Consumer Control Report Characteristic을 탐색하지 않음 | `app_ble.c` L1597 | 첫 번째 Notify Char만 저장하고 나머지 무시 |
| Consumer Report Characteristic의 CCCD에 Notify 활성화 안 함 | `app_ble.c` L1647 | `centralCCCDHdl` 하나만 관리 |
| Consumer 리포트 수신 시 길이 불일치로 드랍 | `app_ble.c` L1090 | 8바이트 미만 리포트는 무시 |
| USB HID Descriptor에 Consumer Control Usage Page 없음 | `app_usb.c` L32 | `KeyRepDesc`에 0x0C(Consumer) 정의 없음 |
| USB에 Consumer Control 전송 함수 없음 | `app_usb.c` | `DevHIDKeyReport()`만 존재 |

---

## 2. 구현 계획

### Phase 1: USB HID Descriptor 확장 (`app_usb.c`)

#### 1-1. HID Report Descriptor 수정
현재 키보드 전용 Descriptor(0x3e바이트)에 **Consumer Control** 섹션 추가:
```
Report ID 1 → 기존 키보드 (8바이트)
Report ID 2 → Consumer Control (2바이트: 16비트 usage code)
```
- `KeyRepDesc[]` 배열에 Report ID 및 Consumer Control Collection 추가
- Configuration Descriptor의 HID Descriptor 길이(`wDescriptorLength`) 필드 갱신

#### 1-2. EP1 전송 함수 분리
- `DevHIDKeyReport()` → Report ID 1 + 8바이트 키보드 리포트 전송
- `DevHIDConsumerReport()` (신규) → Report ID 2 + 2바이트 Consumer Control 전송

#### 1-3. `app_usb.h` 업데이트
- `HID_KEYBOARD_REPORT_LEN` 유지
- `HID_CONSUMER_REPORT_LEN` 상수 추가 (2)
- `DevHIDConsumerReport()` 함수 선언 추가

> [!NOTE]
> Report ID를 사용하는 구조로 바꾸면, 키보드 리포트도 앞에 Report ID(1바이트)가 붙어 9바이트가 됨. EP1 IN 전송 길이를 `HID_KEYBOARD_REPORT_LEN + 1`, Consumer는 `HID_CONSUMER_REPORT_LEN + 1`로 조정 필요.

### Phase 2: BLE Discovery 확장 (`app_ble.c`)

#### 2-1. 다중 Report Characteristic 탐색
`sDiscReports[]` 구조체 배열이 이미 정의되어 있으나 현재 미사용.  
`centralGATTDiscoveryEvent()` 의 CHAR 탐색 단계를 개선하여:
- 모든 `HID_REPORT_CHAR_UUID(0x2A4D)` Characteristic을 `sDiscReports[]`에 누적 저장
- 기존 "첫 번째만 저장" 로직 제거

```c
// 변경 전: 첫 번째 Notify Char만 저장
if(!centralReportFound && (uuid == HID_REPORT_CHAR_UUID ...) && (props & 0x10))

// 변경 후: Notify 가능한 모든 Report Char를 배열에 저장
if((uuid == HID_REPORT_CHAR_UUID || uuid == BOOT_KEY_INPUT_UUID) && (props & 0x10))
    sDiscReports[sDiscReportCount++] = { declHandle, valueHdl, props, ... }
```

#### 2-2. Report Map 읽기 단계 추가 (선택적 Simple 방식 우선)
간단 구현: **Report Reference Descriptor(0x2908)** 를 각 Characteristic에서 읽어 Report ID와 Type(Input=1)을 확인.  
→ `sKbdReportId`, `sConsumerReportId` 전역 변수로 키보드/Consumer Report를 식별.

탐색 순서:
1. `BLE_DISC_STATE_CHAR` → 모든 Report Char 열거
2. `BLE_DISC_STATE_REPORT_REF` → 각 Char의 Report Reference(0x2908) 읽기 → reportId/Type 파악
3. `BLE_DISC_STATE_CCCD` → 식별된 Input Report Char들의 CCCD에 Notify Enable

#### 2-3. 다중 CCCD 관리
- `centralCCCDHdl` 단일 핸들 대신 `sDiscReports[].cccdHandle` 배열 사용
- 발견된 모든 Input Report의 CCCD에 Notify Enable 전송 (순차적으로)

#### 2-4. Notification 수신 처리 확장
`centralProcessGATTMsg()` 의 `ATT_HANDLE_VALUE_NOTI` 처리를:
- `centralCharHdl`(단일) 비교 → `sDiscReports[]` 배열 순회로 변경
- `reportId == sKbdReportId` → `DevHIDKeyReport()` 호출
- `reportId == sConsumerReportId` → `DevHIDConsumerReport()` 호출
- Report ID 없이 2바이트인 경우 → Consumer Control로 간주(휴리스틱)

#### 2-5. 연결 해제 시 Consumer Release 전송
`centralSendKeyRelease()` 에 Consumer Control all-zero 전송 추가.

---

## 3. 변경 파일 요약

| 파일 | 변경 내용 |
|------|-----------|
| `app/app_usb.c` | ① `KeyRepDesc` 확장 (Report ID 1/2), ② `DevHIDConsumerReport()` 추가 |
| `app/app_usb.h` | ③ `HID_CONSUMER_REPORT_LEN` 상수, ④ `DevHIDConsumerReport()` 선언 |
| `app/app_ble.c` | ⑤ 다중 Report Char 탐색, ⑥ Report Reference 읽기, ⑦ 다중 CCCD Enable, ⑧ Consumer Notification 라우팅 |

---

## 4. 주의 사항

> [!WARNING]
> Report ID를 추가하면 기존 키보드 리포트 포맷이 바뀜(선두에 Report ID 1바이트 추가). BLE 슬레이브 기기 따라 Report ID 포함/미포함 양쪽을 모두 처리해야 함.

> [!IMPORTANT]
> USB HID Descriptor를 변경하면 호스트 PC가 장치를 재열거(re-enumerate)해야 인식함. 테스트 시 USB 재연결 필요.

> [!TIP]
> 일부 BLE 키보드(예: Apple Magic Keyboard)는 Consumer Control과 키보드 Report가 같은 Characteristic에서 Report ID로 분리되어 전송됨. 이 경우 Report ID별 분기 처리로 자동 대응 가능.

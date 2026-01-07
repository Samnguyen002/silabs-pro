# BLE Central - USART to BLE String Reception with Pairing

A Bluetooth Low Energy central device that scans for a peripheral advertising a custom USART service, connects, performs Numeric Comparison pairing and bonding, enables Indication on the USART characteristic, receives fragmented strings from the peripheral, reassembles them, validates them with checksum, and outputs the complete payload to a computer via USART (Virtual COM).

---

## Overview

This project implements a BLE Central device with the following features:

- **Scanning & Connection**: Scans for advertising devices that include the custom `gattdb_usart_service_0` UUID and connects when found
- **Reliable Reception**: Receives packets (fragments) via **Indication** from a Peripheral and reassembles them into the original payload; the Central is a receiver and does not fragment outgoing data.
- **USART Output**: Completed payloads are printed to the Virtual COM (VCOM) / USART for viewing on a connected PC
- **Secure Pairing & Bonding**: Implements Numeric Comparison pairing (MITM protection) with user confirmation via push-buttons and optional LCD display
- **Fragment Queueing**: Incoming fragments are queued to handle variations in indication timing

## Table of Contents

- [Key Components](#key-components)
- [Hardware for Demo](#hardware-for-demo)
- [Software Requirements](#software-requirements)
- [Project Structure](#project-structure)
- [Defragment packet](#defragment-packet)
- [Pairing & Security](#pairing--security)
- [Usage](#usage)
- [Troubleshooting](#troubleshooting)
- [References](#references)
- [License](#license)

## Key Components

The project is built with modular, reusable components:

| Component | Purpose |
|-----------|---------|
| `app.c` | Main application logic: scanning, connection, service discovery/characteristic, enabling indications, security configuration, pairing state machine, GATT event handling and LCD display managemen|
| `ble_defragment_rxdata.c/.h` | Defragmentation (reassembly) queue and logic; reassembles incoming fragments into complete payloads and performs checksum validation |
| `app_iostream_usart.c/.h` | USART (VCOM) initialization, output, and checksum computation |
| `app_button_service.c/h (Reusable)`| Generic button service framework with multiple button support and event callbacks |
| `app_button_pairing_complete.c/.h` | Button-triggered pairing control, an application from app_button_service |

---

## Hardware for Demo

- **EFR32 SoC** (e.g., EFR32MG24)
- **USB Interface** for USART0 (Virtual COM)
- **Optional**: Push-buttons (BTN0 for Yes, BTN1 for No) for Numeric Comparison confirmation
- **Optional**: Memory LCD for passkey display

---

## Software Requirements

- Simplicity SDK **v2025.6.2** or later
- Simplicity Studio v5 IDE
- GCC ARM Embedded Toolchain (v12.2.1 or compatible)

---

## Project Structure

```
central_devices/
├── app.c                                 # Core application logic
├── app.h                                 # Application interface
├── app_iostream_usart.c/.h               # USART I/O and checksum
├── ble_defragment_rxdata.c/.h            # Defragmentation and queue management
├── app_button_pairing_complete.c/.h      # Pairing button handling
├── log.h                                 # Logging macros
├── main.c                                # Entry point
├── config/btconf/
│   └── gatt_configuration.btconf         # GATT database configuration
├── autogen/
│   ├── gatt_db.h/.c                      # Auto-generated GATT database
│   └── [other SDK files]
└── readme.md
```

---

## Defragment packet

**Note:** The Peripheral produces packets according to the protocol below. The Central's role is to receive these packets (fragments), queue them, reassemble them into the original payload, and validate integrity—Central does not perform fragmentation itself.

### First fragment (starts the transmission)
- Minimum length: 2 bytes (length byte + at least 1 payload byte). If shorter, Central logs "First fragment too short".
- Byte 0 is the payload length (expected total payload length).
- If the received fragment length equals 1 + expected_length + 1, the transmission is a single-fragment message (length + payload + checksum).
- Otherwise, the first fragment contains length byte + up to 19 bytes of payload (first fragment payload length = len - 1).

### Subsequent fragments
- Middle fragments carry up to 20 bytes of payload each.
- The final fragment carries the remaining payload (which must match the remaining length) followed by a checksum byte.
- If the final fragment's payload length does not match the remaining expected payload, Central logs "Last fragment size mismatch".
- If a middle fragment is larger than the remaining expected payload, Central logs "Middle fragment too larger".

### Processing & Validation
- Fragments are pushed into a ring queue by the Central (`defrag_push_data`). The Central pops and processes queued fragments (`defrag_process_fragment`) in sequence.
- The Central reassembles fragments into an internal buffer up to `DEFRAG_MAX_PAYLOAD` (see `ble_defragment_rxdata.h`).
- When all payload bytes are collected, the Central reads the checksum byte from the last fragment and validates it using the two's complement of the sum of payload bytes (computed by `app_iostream_checksum()`).
- If checksum matches, the payload is marked valid and can be retrieved via `defrag_get_payload()` (returns payload pointer, length and checksum validity flag). If checksum fails, Central logs a checksum error.

### Error conditions logged by the Central
- `First fragment too short`
- `Invalid length` (payload length 0 or greater than allowed maximum)
- `QUEUE is FULL` / `QUEUE is EMPTY`
- `Empty fragment`
- `Last fragment size mismatch`
- `Middle fragment too larger`
- `Checksum error`

This section mirrors the behavior implemented in `ble_defragment_rxdata.c/.h` and describes the exact packet handling expected by the Central.

---

## Pairing & Security

### Role
- Initiator (increase sercurity) 

### Security Configuration
- **Method**: Numeric Comparison (MITM Protection enabled)
- **I/O Capability**: DISPLAYYESNO (device displays passkey and expects Yes/No confirmation)
- **Authentication**: LE Secure Connections with bonding enabled
- **Bonding**: Long-term keys stored to allow automatic secure reconnection
- **Passkey**: 
  - **Numeric Comparision**: Random passkey 6-digit for each pairing process
  - **PassEntry**: Fixed passkey derived from Bluetooth device address (6 bytes)Formula: Uses device MAC address bytes to generate a consistent 6-digit passkey

### Pairing Flow
1. **Boot**: Central scans for peripherals advertising `usart_service`.
2. **Open connection**: Central opens connection when target is found.
3. **Dicover service/charac**: Based on data type we want, find it into one of adv packets that matchs our data type
4. **Pairing**: Central initiates/enforces security (`sl_bt_sm_increase_security`) and handles pairing events:
   - `sl_bt_evt_sm_passkey_display` shows the passkey on the Central (LCD or logs)
   - `sl_bt_evt_sm_confirm_passkey` prompts the user to confirm
   - User confirms with BTN0 (Yes) or rejects with BTN1 (No)
5. **Bonding**: On success (`sl_bt_evt_sm_bonded`), Central discovers the remote `usart_service` and enables indications

## Optional UI
- **Memory LCD (LS013B7DH03)**: Displays passkey during Numeric Comparison pairing
- **Pushbuttons (BTN0, BTN1)**: 
  - BTN0 = **Confirm** (Yes) pairing
  - BTN1 = **Reject** (No) pairing

---

## Usage

### 1. Build and Flash

```bash
# Build the project (from Simplicity Studio or CLI)
cd <project_path>/GNU ARM v12.2.1 - Default/
make all

# Flash to device (adjust COM port as needed)
commander <project_path>/GNU ARM v12.2.1 - Default/central_devices.s37
```

### 2. Pair with Peripheral

- Start scanning on Central. When a Peripheral advertising the `usart_service` appears, Central will connect and initiate pairing.
- Confirm Numeric Comparison passkey using pushbuttons or the LCD when prompted.

### 3. Receive Strings

When the Peripheral sends strings (fragmented according to the protocol above) the Central will log and output reassembled payloads on VCOM:

```
->Payload Ready:
->Length: 11 bytes
->Data: "Hello World"
```

For multicontent transmissions, logs will show fragment processing and checksum validation messages, for example:

```
[FRAGMENT 1] Data: This is a very lon, len: 19
[MID FRAGMENT] Data: g string that excee, len: 20
[LAST FRAGMENT] Data: ds the single fragmen, len: 12
[CHECKSUM] payload NOT LOST , in subsequent fragment
[TOTAL FRAGMENT] Data: This is a very long string that exceeds the single fragment limit, len: 66
->Payload Ready:
->Length: 66 bytes
->Data: "This is a very long string that exceeds the single fragment limit"
```

---

## Troubleshooting

### Issue: Central does not find Peripheral
- Verify that the Peripheral advertises the custom `usart_service` UUID (AD types 0x06 or 0x07)
- Check that the Peripheral is advertising as connectable

### Issue: No VCOM Output
- Ensure Virtual COM instance (sl_iostream_vcom) and `retarget-stdio` are enabled in software components
- Verify host machine COM port settings (baud = 115200)

### Issue: Checksum or Defragmentation Errors
- Confirm the Peripheral follows the exact framing rules (length byte, payload fragments, final checksum byte)
- Check logs for `>Middle fragment too larger`, `>Last fragment size mismatch` or `Checksum error` to debug fragment boundaries

### Issue: Pairing Fails
- Make sure passkeys displayed on both devices match
- Confirm user input via buttons (BTN0 = Yes / BTN1 = No) if using Numeric Comparison
- Clear old bonds (`sl_bt_sm_delete_bondings()`) and retry pairing

---

## References

- [UG103.14: Bluetooth LE Fundamentals](https://www.silabs.com/documents/public/user-guides/ug103-14-fundamentals-ble.pdf)
- [QSG169: Bluetooth SDK v3.x Quick Start Guide](https://www.silabs.com/documents/public/quick-start-guides/qsg169-bluetooth-sdk-v3x-quick-start-guide.pdf)
- [UG434: Silicon Labs Bluetooth C SoC Developer's Guide](https://www.silabs.com/documents/public/user-guides/ug434-bluetooth-c-soc-dev-guide-sdk-v3x.pdf)
- [UG438: GATT Configurator User's Guide](https://www.silabs.com/documents/public/user-guides/ug438-gatt-configurator-users-guide-sdk-v3x.pdf)
- [Bluetooth API Reference](https://docs.silabs.com/bluetooth/latest/)

---

## License

Copyright 2025 Silicon Laboratories Inc. www.silabs.com

SPDX-License-Identifier: Zlib

This software is provided 'as-is', without any express or implied warranty.

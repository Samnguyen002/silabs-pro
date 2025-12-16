# BLE Peripheral - USART to BLE String Transmission with Pairing

A Bluetooth Low Energy peripheral device that receives unlimited-length strings from a computer via USART (Virtual COM) and transmits them securely to a central device over BLE using **Indication** with automatic frame fragmentation, checksum validation, and **Numeric Comparison Pairing** with optional push-button and LCD display support.

---

## Overview

This project implements a BLE peripheral device with the following features:

- **USART Input**: Receive arbitrary-length strings from a computer via Virtual COM (VCOM)
- **Frame Fragmentation**: Automatically split strings into 20-byte fragments for transmission
- **Frame Encoding**: Each transmission uses the format: `[Length][Value][Checksum]`
- **Reliable Transmission**: Uses BLE Indication (requires central device acknowledgment)
- **Secure Pairing**: Implements Numeric Comparison pairing method with fixed passkey
- **Optional UI**: Push-button and Memory LCD display for passkey confirmation during pairing
- **Device Information**: Stores manufacturer, model, hardware/firmware versions

## Table of Contents

- [Key Components](#key-components)
- [Hardware for Demo](#hardware-for-demo)
- [Software Requirements](#software-requirements)
- [Project Structure](#project-structure)
- [GATT Database](#GATT-database)
- [Data Frame Format](#data-frame-format)
- [Pairing & Security](#pairing--security)
- [Usage](#usage)
- [Troubleshooting](#troubleshooting)
- [References](#references)
- [License](#license)

## Key Components

The project is built with modular, reusable components:

| Component | Purpose |
|-----------|---------|
| [app.c](app.c) | Main application logic, event handlers, security configuration, pairing state machine, and LCD display management |
| [ble_fragment_queue.c](ble_fragment_queue.c) | Fragment queue management for multi-packet transmission with confirmation-based flow control |
| [app_iostream_usart.c](app_iostream_usart.c) | USART/Virtual COM initialization and checksum calculation |
| [app_button_service.c (Reusable)](app_button_service.c) | Generic button service framework with multiple button support and event callbacks |
| [app_button_pairing_complete.c](app_button_pairing_complete.c) | Button-triggered pairing control, an application from app_button_service|

---

## Hardware for Demo

- **EFR32 SoC** (EFR32MG24)
- **USB Interface** for USART0 (Virtual COM)
- **Optional**: Pushbuttons (BTN0, BTN1) for pairing confirmation
- **Optional**: Memory LCD for passkey display (on BRD4001A)

---

## Software Requirements

- Simplicity SDK **v2025.6.2** or later
- Simplicity Studio v5 IDE
- GCC ARM Embedded Toolchain (v12.2.1 or compatible)

---

## Project Structure

```
peripheral_devices/
├── app.c                                 # Core application logic
├── app.h                                 # Application interface
├── app_iostream_usart.c/.h               # USART I/O and checksum
├── ble_fragment_queue.c/.h               # Fragment queue management
├── app_button_service.c/.h               # Button event handling
├── app_button_pairing_complete.c/.h      # Pairing control
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

## GATT Database

The application defines the following GATT services:

### Custom USART Service (UUID: TBD)
- **Characteristic: usart_packet** (20 bytes)
  - **Properties**: Indication + Write
  - **Direction**: Peripheral → Central (indication), Central → Peripheral (write acknowledgment)
  - **Purpose**: Transmit fragments and receive confirmations

### Standard Services
- **Device Information** (0x180A)
  - Manufacturer Name, Model Number, Hardware Revision, Firmware Revision, System ID
- **OTA DFU** (In-Place OTA - Simplicity SDK built-in)

---

## Data Frame Format

### Single Fragment (payload ≤ 18 bytes)
```
[Byte 0: Length] [Bytes 1-N: Payload] [Last Byte: Checksum]
Example: 0x05 'H' 'e' 'l' 'l' 'o' 0x?? (total 7 bytes)
```

### Multi-Fragment (payload > 18 bytes)
```
Fragment 1:  [Length(1)] [Payload(19)]           → 20 bytes
Fragment 2:  [Payload(20)]                       → 20 bytes
...
Fragment N:  [Payload(remaining)] [Checksum(1)] → variable
```

**Checksum**: Two's complement (negation + 1) of the sum of all payload bytes

---

## Pairing & Security

### Security Configuration
- **Method**: Numeric Comparison (MITM Protection enabled)
- **I/O Capability**: Display Yes/No (DISPLAYYESNO)
- **Authentication**: LE Secure pairing with bonding
- **Passkey**: 
  - **Numeric Comparision**: Random passkey 6-digit for each pairing process
  - **PassEntry**: Fixed passkey derived from Bluetooth device address (6 bytes). Formula: Uses device MAC address bytes to generate a consistent 6-digit passkey

### Pairing Flow 
1. **Boot**: Device advertises and awaits connection
2. **Connection**: Central connects and initiates pairing (increase sercurity)
3. **Passkey Display**: 
   - LCD shows 6-digit passkey 
   - User presses **BTN0** (Yes) or **BTN1** (No) to confirm/reject
   - Central user confirms matching passkey on their device
4. **Bonding**: Long-term keys stored persistently (survives reboot)
5. **Subsequent Connections**: Automatic secure connection using stored keys

[Pairing process](https://docs.silabs.com/bluetooth/6.2.0/bluetooth-security-pairing-processes/#example)

### Optional UI
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
commander <project_path>/GNU ARM v12.2.1 - Default/peripheral_devices.s37
```

### 2. Pair with Central Device

- **If LCD + Buttons available**:
  1. Initiate pairing from central
  2. LCD displays 6-digit passkey
  3. Verify passkey matches on central device
  4. Press **BTN0** on peripheral to confirm
  5. Central user confirms on their device

- **Without buttons** (automatic acceptance):
  - Central initiates pairing
  - Passkey displayed in terminal logs
  - Automatic confirmation

### 3. Send Strings via USART

Type any string in the terminal and press **Enter**:

```
> Hello World
Received: 11 bytes:
> Hello World
Total payload: 11 bytes
Total fragments: 1
  Fragment 1: 13 bytes
->Sending fragment 1/1 (13 bytes)...
send Indication OK
```

For longer strings (>18 bytes):

```
> This is a very long string that exceeds the single fragment limit
Received: 66 bytes:
> This is a very long string...
Total payload: 66 bytes
Total fragments: 4
  Fragment 1: 20 bytes
  Fragment 2: 20 bytes
  Fragment 3: 20 bytes
  Fragment 4: 27 bytes
->Sending fragment 1/4 (20 bytes)...
->Sending fragment 2/4 (20 bytes)...
->Sending fragment 3/4 (20 bytes)...
->Sending fragment 4/4 (27 bytes)...
```

---

## Troubleshooting

### Issue: No USART Output

- Verify VCOM instance is enabled in Software Components
- Check Virtual COM port in Device Manager / system
- Ensure baud rate is 115200

### Issue: Pairing Fails

- Verify central device has matching passkey
- Check that LCD displays correct passkey
- If buttons unavailable, manually confirm in terminal (send 'y'/'Y')
- Clear old bondings: restart device or use `sl_bt_sm_delete_bondings()`

### Issue: Fragments Not Transmitted

- Verify indication is enabled on central (write to CCCD = 0x0002)
- Check that central has received previous fragment confirmation
- Monitor logs: look for `->Sending fragment` messages

### Issue: Bootloader Not Present

This project requires a Gecko Bootloader. Flash a compatible bootloader to the device before running the application. Use Simplicity Commander or the IDE's bootloader flashing tool.

---

## References

- [UG103.14: Bluetooth LE Fundamentals](https://www.silabs.com/documents/public/user-guides/ug103-14-fundamentals-ble.pdf)
- [QSG169: Bluetooth SDK v3.x Quick Start Guide](https://www.silabs.com/documents/public/quick-start-guides/qsg169-bluetooth-sdk-v3x-quick-start-guide.pdf)
- [UG434: Silicon Labs Bluetooth C SoC Developer's Guide](https://www.silabs.com/documents/public/user-guides/ug434-bluetooth-c-soc-dev-guide-sdk-v3x.pdf)
- [UG438: GATT Configurator User's Guide](https://www.silabs.com/documents/public/user-guides/ug438-gatt-configurator-users-guide-sdk-v3x.pdf)
- [Bluetooth API Reference](https://docs.silabs.com/bluetooth/latest/)
- [AN1260: Integrating Bluetooth Applications with RTOS](https://www.silabs.com/documents/public/application-notes/an1260-integrating-v3x-bluetooth-applications-with-rtos.pdf)

---

## License

Copyright 2025 Silicon Laboratories Inc. www.silabs.com

SPDX-License-Identifier: Zlib

This software is provided 'as-is', without any express or implied warranty.


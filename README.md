<h1 align="center">T-Display-P4-Air</h1>

## **English** | [Chinese](./README_CN.md)

[![License](https://img.shields.io/github/license/Xinyuan-LilyGO/T-Display-P4-Air?style=flat-square)](./LICENSE)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.5.4%2B-ff6f00?style=flat-square)](https://github.com/espressif/esp-idf)
[![C++](https://img.shields.io/badge/C%2B%2B-17%2B-00599c?style=flat-square)](https://isocpp.org/)

T-Display-P4-Air is a compact development platform built around the **ESP32-P4**. It integrates an **ESP32-C5** wireless coprocessor, an **nRF9151** cellular/GNSS modem, a MIPI-DSI touch display, MIPI-CSI camera support, audio, motion and magnetic sensors, NFC/RFID, infrared, haptic feedback, TF card storage, and USB-UART.

Air is planned for future public sale through the LILYGO website. The release date has not been announced. The current hardware document is an evaluation preview, and the production BOM, mechanical design, pin assignments, and supported features may change.

## Directory

- [Supported Frameworks](#supported-frameworks)
- [Quick Start](#quick-start)
- [Examples](#examples)
- [Hardware Modules](#hardware-modules)
- [Pin Overview](#pin-overview)
- [Project Materials](#project-materials)
- [FAQ](#faq)

## Supported Frameworks

| Framework | Status | Version |
| --- | --- | --- |
| ESP-IDF | Recommended | `>= v5.5.4` |

## Quick Start

### Build With ESP-IDF

Install ESP-IDF first. For environment setup, refer to the official [ESP-IDF Get Started guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/index.html).

Clone this repository together with its submodules:

```bash
git clone --recursive https://github.com/Xinyuan-LilyGO/T-Display-P4-Air.git
cd T-Display-P4-Air
```

If the repository was cloned without `--recursive`, initialize the submodules manually:

```bash
git submodule update --init --recursive
```

Select an Air example in `menuconfig`, then build and flash:

```bash
idf.py set-target esp32p4
idf.py menuconfig
idf.py build
idf.py flash monitor
```

Select the required example under `Example Configuration` > `Select the example to build`.

### Examples

| Example Link | Description |
| --- | --- |
| [`lilygobox-espidf`](https://github.com/Xinyuan-LilyGO/lilygobox-espidf) | Factory application example for T-Display-P4-Air |
| [`lilygo_device_driver_example`](https://github.com/Xinyuan-LilyGO/lilygo_device_driver_example) | Shared examples maintained in a dedicated application project |
| [`main/examples`](./main/examples) | Air board-specific ESP-IDF examples |

Open an example link above for the current source and supported example list.

## Hardware Modules

| Function | Device or circuit | Interface |
| --- | --- | --- |
| Main MCU | ESP32-P4, W25Q128JV flash | — |
| Wireless coprocessor | ESP32-C5HF4 | SDIO |
| Cellular/GNSS modem | nRF9151 | UART, SIM, SWD |
| Sub-GHz radio | LR2021 | SPI / RF |
| TFT display and touch | 4.05-inch TFT, 540 x 1168, HI8561 | MIPI-DSI / I2C |
| AMOLED display and touch | 4.1-inch AMOLED, 568 x 1232, RM69A10 / GT9895 | MIPI-DSI / I2C |
| Camera | OV2710 | MIPI-CSI |
| Audio | ES8390, NS4150B, microphone and headphone jack | I2S / analog |
| Motion and magnetic sensing | BHI260AP, QMC6309 | I2C |
| NFC/RFID | ST25R3916B | I2C |
| Infrared | VSMY14940, TSOP75338 | GPIO |
| Haptic feedback | AW86224AFCR | I2C |
| Storage | TF card slot | SDIO 3.0 |
| Power and USB | AXP517, CH343P, XL9535 | I2C / USB-UART / GPIO |

The component list above is based on the evaluation preview and is subject to change.

## Pin Overview

Board pin definitions are provided in the device-driver configuration files:

- [`config.h`](https://github.com/Xinyuan-LilyGO/lilygo_device_driver/blob/main/src/device/t_display_p4_air/config.h)

## Project Materials

| Document | Description |
| --- | --- |
| [`T-Display-P4-Air v1.0 Evaluation Preview, 2026-10-16`](<./project/T-Display-P4-Air_v1.0(Evaluation Preview)_202610161627.pdf>) | Current Air evaluation schematic |
| [`Air board driver configuration`](https://github.com/Xinyuan-LilyGO/lilygo_device_driver/blob/main/src/device/t_display_p4_air/config.h) | Board GPIO and peripheral configuration |
| [`nRF9151 Serial Modem guide`](./apps/ncs-serial-modem/README.md) | nRF9151 modem build and test information |
| [`Firmware records`](./firmware) | Air firmware versions and test images |

## FAQ

<details>
<summary>Q. Why are submodule headers or components missing?</summary>

A. Initialize all repository submodules:

```bash
git submodule update --init --recursive
```

</details>

<details>
<summary>Q. How do I select a different example?</summary>

A. Run `idf.py menuconfig`, select the Air device under `lilygo_device_driver configuration`, then choose an example under `Example Configuration`.

</details>

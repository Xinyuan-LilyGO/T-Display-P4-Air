<h1 align="center">T-Display-P4-Air</h1>

## [English](./README.md) | **中文**

[![License](https://img.shields.io/github/license/Xinyuan-LilyGO/T-Display-P4-Air?style=flat-square)](./LICENSE)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.5.4%2B-ff6f00?style=flat-square)](https://github.com/espressif/esp-idf)
[![C++](https://img.shields.io/badge/C%2B%2B-17%2B-00599c?style=flat-square)](https://isocpp.org/)

## 项目介绍

T-Display-P4-Air 是一款基于 **ESP32-P4** 的紧凑型开发平台，集成 **ESP32-C5** 无线协处理器、**nRF9151** 蜂窝/GNSS 模块、MIPI-DSI 触摸屏、MIPI-CSI 摄像头接口、音频、运动和磁传感器、NFC/RFID、红外、振动反馈、TF 卡存储和 USB-UART。

Air 计划未来在 LILYGO 官网独立销售，正式发布时间尚未确定。当前硬件资料属于评估预览版，量产 BOM、机械结构、引脚分配和支持的功能仍可能调整。

## 目录

- [项目介绍](#项目介绍)
- [支持的框架](#支持的框架)
- [快速开始](#快速开始)
- [示例](#示例)
- [硬件模块](#硬件模块)
- [引脚说明](#引脚说明)
- [项目资料](#项目资料)
- [常见问题](#常见问题)

## 支持的框架

| 框架 | 状态 | 版本 |
| --- | --- | --- |
| ESP-IDF | 推荐 | `>= v5.5.4` |

## 快速开始

### 使用 ESP-IDF 编译

请先安装 ESP-IDF，环境配置可参考官方 [ESP-IDF 入门指南](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/index.html)。

递归克隆本仓库及其子模块：

```bash
git clone --recursive https://github.com/Xinyuan-LilyGO/T-Display-P4-Air.git
cd T-Display-P4-Air
```

如果没有使用 `--recursive`，手动初始化子模块：

```bash
git submodule update --init --recursive
```

在 `menuconfig` 中选择 Air 示例，然后编译和烧录：

```bash
idf.py set-target esp32p4
idf.py menuconfig
idf.py build
idf.py flash monitor
```

在 `Example Configuration` > `Select the example to build` 中选择需要编译的示例。

### 示例

| 示例链接 | 说明 |
| --- | --- |
| [`lilygobox-espidf`](https://github.com/Xinyuan-LilyGO/lilygobox-espidf) | T-Display-P4-Air 出厂应用示例 |
| [`lilygo_device_driver_example`](https://github.com/Xinyuan-LilyGO/lilygo_device_driver_example) | 在独立应用工程中集中维护的通用示例 |
| [`main/examples`](./main/examples) | Air 板级专用 ESP-IDF 示例 |

请通过上方入口查看最新源码和当前支持的示例。

## 硬件模块

| 功能 | 器件或电路 | 接口 |
| --- | --- | --- |
| 主控 MCU | ESP32-P4、W25Q128JV Flash | - |
| 无线协处理器 | ESP32-C5HF4 | SDIO |
| 蜂窝/GNSS 模块 | nRF9151 | UART、SIM、SWD |
| Sub-GHz 射频 | LR2021 | SPI / 射频 |
| TFT 显示和触摸 | 4.05 英寸 TFT，540 x 1168，HI8561 | MIPI-DSI / I2C |
| AMOLED 显示和触摸 | 4.1 英寸 AMOLED，568 x 1232，RM69A10 / GT9895 | MIPI-DSI / I2C |
| 摄像头 | OV2710 | MIPI-CSI |
| 音频 | ES8390、NS4150B、麦克风和耳机接口 | I2S / 模拟音频 |
| 运动和磁传感 | BHI260AP、QMC6309 | I2C |
| NFC/RFID | ST25R3916B | I2C |
| 红外 | VSMY14940、TSOP75338 | GPIO |
| 振动反馈 | AW86224AFCR | I2C |
| 存储 | TF 卡插槽 | SDIO 3.0 |
| 电源和 USB | AXP517、CH343P、XL9535 | I2C / USB-UART / GPIO |

以上器件信息基于评估预览版，后续可能调整。

## 引脚说明

板级引脚定义位于设备驱动配置文件中：

- [`config.h`](https://github.com/Xinyuan-LilyGO/lilygo_device_driver/blob/main/src/device/t_display_p4_air/config.h)

## 项目资料

| 文档 | 说明 |
| --- | --- |
| [`T-Display-P4-Air v1.0 评估预览版原理图，2026-10-16`](<./project/T-Display-P4-Air_v1.0(Evaluation Preview)_202610161627.pdf>) | 当前 Air 评估原理图 |
| [`Air 板级驱动配置`](https://github.com/Xinyuan-LilyGO/lilygo_device_driver/blob/main/src/device/t_display_p4_air/config.h) | 板级 GPIO 和外设配置 |
| [`nRF9151 Serial Modem 指南`](./apps/ncs-serial-modem/README_CN.md) | nRF9151 modem 编译和测试信息 |
| [`固件记录`](./firmware) | Air 固件版本和测试镜像 |

## 常见问题

<details>
<summary>Q. 为什么缺少子模块头文件或组件？</summary>

A. 请初始化仓库的全部子模块：

```bash
git submodule update --init --recursive
```

</details>

<details>
<summary>Q. 如何切换不同的示例？</summary>

A. 执行 `idf.py menuconfig`，在 `lilygo_device_driver configuration` 中选择 Air 设备，再在 `Example Configuration` 中选择示例。

</details>

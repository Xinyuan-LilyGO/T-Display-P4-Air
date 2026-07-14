# T-Display-P4-Air nRF9151 串行调制解调器

## **[英文](./README.md)** | **中文**

本目录提供在 T-Display-P4-Air nRF9151 上运行 Nordic Serial Modem 所需的
板级编译文件。

当前配置使用 UART0，波特率为 115200，并启用 RTS/CTS 硬件流控。DTR 和 RI
均已禁用，同时禁用了从 nRF9151 DK 继承但本开发板不使用的外设。

## 文件说明

- `t-display-p4-air_nrf9151_ns.overlay`：开发板硬件配置。
- `t-display-p4-air_nrf9151_ns.conf`：开发板专用 Kconfig 配置。
- `build_t-display-p4-air_nrf9151_ns.py`：Python 编译辅助脚本。

## 1. 准备环境

通过 nRF Connect for VS Code 安装：

- nRF Connect SDK v3.2.4
- 与 v3.2.4 匹配的工具链
- nRF Connect for VS Code 扩展
- 用于烧录的 SEGGER J-Link 软件

下文中的 `<NCS_WORKSPACE>` 表示用户本机的 nRF Connect SDK v3.2.4 工作区
根目录。该目录中应包含 `.west`、`nrf`、`zephyr` 和其他 SDK 模块。

例如，本机的 nRF Connect SDK v3.2.4 安装在：

```text
D:\nrf_connect_sdk\v3.2.4
```

那么 `<NCS_WORKSPACE>` 就表示 `D:\nrf_connect_sdk\v3.2.4`，文档中的：

```powershell
cd <NCS_WORKSPACE>
```

实际执行时应写成：

```powershell
cd D:\nrf_connect_sdk\v3.2.4
```

## 2. 将项目放入 SDK 工作区

将本目录完整复制到本机 SDK 工作区，然后把上游 `ncs-serial-modem` 仓库放入
其中，最终结构如下：

```text
<NCS_WORKSPACE>\
|-- .west\
|-- nrf\
|-- zephyr\
`-- ncs-serial-modem\
    |-- README.md
    |-- README_CN.md
    |-- t-display-p4-air_nrf9151_ns.overlay
    |-- t-display-p4-air_nrf9151_ns.conf
    |-- build_t-display-p4-air_nrf9151_ns.py
    `-- ncs-serial-modem\
        |-- west.yml
        |-- zephyr\module.yml
        `-- app\
```

继续操作前，请确认内层目录包含 `west.yml` 和 `app/CMakeLists.txt`。

## 3. 打开 Nordic 工具链终端

在 VS Code 中通过 nRF Connect 扩展选择 NCS v3.2.4 工具链，然后打开其集成终端。

检查工具链环境：

```powershell
west --version
python --version
cmake --version
ninja --version
```

除非已经加载 Nordic 工具链环境，否则不要使用普通终端编译。

## 4. 选择 Serial Modem manifest

将 `<NCS_WORKSPACE>` 替换为本机实际的 SDK 工作区路径：

```powershell
cd <NCS_WORKSPACE>
west config manifest.path ncs-serial-modem/ncs-serial-modem
west update
west config manifest.path
```

最后一条命令必须输出：

```text
ncs-serial-modem/ncs-serial-modem
```

`west update` 可能访问网络，并将 SDK 模块同步到 Serial Modem 要求的版本。

## 5. 编译

在 Nordic 工具链终端中执行：

```powershell
cd <NCS_WORKSPACE>\ncs-serial-modem
python .\build_t-display-p4-air_nrf9151_ns.py
```

默认执行干净编译。后续需要增量编译时使用：

```powershell
python .\build_t-display-p4-air_nrf9151_ns.py --no-pristine
```

编译输出目录：

```text
<NCS_WORKSPACE>\ncs-serial-modem\build\t-display-p4-air_nrf9151_ns
```

在 `<NCS_WORKSPACE>\ncs-serial-modem` 中执行的等效手动命令：

```powershell
$boardRoot = (Get-Location).Path

west build -p always `
  -b nrf9151dk/nrf9151/ns `
  -d "$boardRoot\build\t-display-p4-air_nrf9151_ns" `
  "$boardRoot\ncs-serial-modem\app" `
  -- `
  "-DEXTRA_DTC_OVERLAY_FILE=$boardRoot\t-display-p4-air_nrf9151_ns.overlay" `
  "-DEXTRA_CONF_FILE=$boardRoot\t-display-p4-air_nrf9151_ns.conf"
```

文件名中的 `_ns` 表示这些文件用于 Nordic 非安全域编译目标
`nrf9151dk/nrf9151/ns`。

## 6. 连接 J-Link

烧录前：

1. 给开发板供电，并确认 nRF9151 已使能。
2. 将 J-Link VTref 连接到开发板 I/O 电压。
3. 连接 GND、SWDIO 和 SWCLK。
4. 启用蜂窝发射前必须连接 LTE 天线。

## 7. 烧录

在 `<NCS_WORKSPACE>\ncs-serial-modem` 中执行：

```powershell
west flash -r jlink -d .\build\t-display-p4-air_nrf9151_ns
```

基础板目标将 J-Link 配置为 `nRF9151_xxCA`、4 MHz。如果 SWD 连接不稳定，
请通过 runner 参数降低 J-Link 速度。

## 8. 验证 UART 通信

UART 参数：

- 115200 波特率
- 8 数据位、无校验、1 停止位
- 启用 RTS/CTS 硬件流控
- AT 命令结束符为回车符（`\r`）
- 禁用 DTR 和 RI

主控 MCU 必须启用 RTS/CTS 硬件流控，否则即使 TX 和 RX 连接正确，UART 通信
仍可能停滞。

复位后，Serial Modem 初始化完成时会发送 `Ready\r\n`。发送 `AT\r`，应收到
`OK` 响应。

## 9. 输出原始 GNSS NMEA

本 fork 新增 `AT#XGNSSNMEA`，用于通过 AT UART 原样输出 GGA、GLL、RMC、GSA
和 GSV 语句。必须在启动 GNSS 前开启：

```text
AT+CFUN=0
AT%XSYSTEMMODE=0,0,1,0
AT+CFUN=31
AT#XGNSSNMEA=1
AT#XGNSS=1,0,1
```

输出会保留 `$`、校验和及 `\r\n`。使用 `AT#XGNSSNMEA=0` 停止输出，使用
`AT#XGNSSNMEA?` 查询当前状态。

## 当前配置的限制

- 不支持 DTR/RI UART 电源管理和 DTR 唤醒。
- 禁用需要 nRF9151 外部 Flash 的完整 Modem FOTA/DFU。
- 应用升级和 Modem Delta 固件升级仍可使用内部 Flash。
- 调试日志使用 Segger RTT，通过 J-Link 读取，不占用 Serial Modem UART。

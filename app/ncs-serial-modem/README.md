# T-Display-P4-Air nRF9151 Serial Modem

## **English** | **[Chinese](./README_CN.md)**

Board-specific build files for running Nordic Serial Modem on the
T-Display-P4-Air nRF9151.

This configuration uses UART0 at 115200 baud with RTS/CTS hardware flow
control. DTR and RI are disabled. Peripherals inherited from the nRF9151 DK but
not used by this board are also disabled.

## Files

- `t-display-p4-air_nrf9151_ns.overlay`: board hardware configuration.
- `t-display-p4-air_nrf9151_ns.conf`: board-specific Kconfig settings.
- `build_t-display-p4-air_nrf9151_ns.py`: Python build helper.

## 1. Prerequisites

Install the following through nRF Connect for VS Code:

- nRF Connect SDK v3.2.4
- The matching v3.2.4 toolchain
- nRF Connect for VS Code extension
- SEGGER J-Link software for flashing

In the commands below, `<NCS_WORKSPACE>` means the root directory of the local
nRF Connect SDK v3.2.4 workspace. It is the directory containing `.west`,
`nrf`, `zephyr`, and the other SDK modules.

For example, if nRF Connect SDK v3.2.4 is installed at:

```text
D:\nrf_connect_sdk\v3.2.4
```

then `<NCS_WORKSPACE>` means `D:\nrf_connect_sdk\v3.2.4`, and this command:

```powershell
cd <NCS_WORKSPACE>
```

must be entered as:

```powershell
cd D:\nrf_connect_sdk\v3.2.4
```

## 2. Place the project in the SDK workspace

Copy this complete directory into the local SDK workspace. Then place the
upstream `ncs-serial-modem` repository inside it as shown below:

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

The inner directory must contain `west.yml` and `app/CMakeLists.txt` before
continuing.

## 3. Open the Nordic toolchain terminal

In VS Code, select the NCS v3.2.4 toolchain through the nRF Connect extension,
then open its integrated terminal.

Verify the environment:

```powershell
west --version
python --version
cmake --version
ninja --version
```

Do not use a normal terminal unless the Nordic toolchain environment has
already been loaded into it.

## 4. Select the Serial Modem manifest

Replace `<NCS_WORKSPACE>` with the actual local SDK workspace path:

```powershell
cd <NCS_WORKSPACE>
west config manifest.path ncs-serial-modem/ncs-serial-modem
west update
west config manifest.path
```

The last command must print:

```text
ncs-serial-modem/ncs-serial-modem
```

`west update` may access the network and synchronizes the SDK modules to the
versions required by Serial Modem.

## 5. Build

From the Nordic toolchain terminal:

```powershell
cd <NCS_WORKSPACE>\ncs-serial-modem
python .\build_t-display-p4-air_nrf9151_ns.py
```

The default is a pristine build. Use the following command for a later
incremental rebuild:

```powershell
python .\build_t-display-p4-air_nrf9151_ns.py --no-pristine
```

Build output is written to:

```text
<NCS_WORKSPACE>\ncs-serial-modem\build\t-display-p4-air_nrf9151_ns
```

Equivalent manual command, executed from `<NCS_WORKSPACE>\ncs-serial-modem`:

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

The `_ns` suffix indicates that these files are used with the non-secure Nordic
board target `nrf9151dk/nrf9151/ns`.

## 6. Connect J-Link

Before flashing:

1. Power the board and verify that the nRF9151 is enabled.
2. Connect J-Link VTref to the board I/O voltage.
3. Connect GND, SWDIO, and SWCLK.
4. Connect the LTE antenna before enabling the cellular transmitter.

## 7. Flash

Run this command from `<NCS_WORKSPACE>\ncs-serial-modem`:

```powershell
west flash -r jlink -d .\build\t-display-p4-air_nrf9151_ns
```

The board target configures J-Link for `nRF9151_xxCA` at 4 MHz. If the SWD
connection is unstable, lower the J-Link speed through the runner arguments.

## 8. Verify UART communication

UART settings:

- 115200 baud
- 8 data bits, no parity, 1 stop bit
- RTS/CTS hardware flow control enabled
- AT command terminator: carriage return (`\r`)
- DTR and RI disabled

The host MCU must enable RTS/CTS hardware flow control. Otherwise UART traffic
can stall even when TX and RX are connected correctly.

After reset, Serial Modem sends `Ready\r\n` when initialization completes.
Send `AT\r` and expect an `OK` response.

## 9. Raw GNSS NMEA output

This fork adds `AT#XGNSSNMEA` for forwarding unmodified GGA, GLL, RMC, GSA,
and GSV sentences through the AT UART. Enable it before starting GNSS:

```text
AT+CFUN=0
AT%XSYSTEMMODE=0,0,1,0
AT+CFUN=31
AT#XGNSSNMEA=1
AT#XGNSS=1,0,1
```

The output retains the leading `$`, checksum, and `\r\n`. Stop the stream with
`AT#XGNSSNMEA=0`, and use `AT#XGNSSNMEA?` to query its state.

## Configuration limitations

- DTR/RI UART power management and DTR wake-up are unavailable.
- Full modem FOTA/DFU requiring external nRF9151 flash is disabled.
- Application updates and delta modem firmware updates can still use internal
  flash.
- Debug logs use Segger RTT through J-Link and do not consume the Serial Modem
  UART.

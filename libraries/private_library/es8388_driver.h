/*
 * @Description: ES8388 driver helpers
 * @Author: LILYGO_L
 * @Date: 2026-05-24 16:30:00
 * @LastEditTime: 2026-06-16 14:23:50
 * @License: GPL 3.0
 */

#pragma once

#include <cstdint>
#include <memory>

#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"

#include "lilygo_device_driver_library.h"

namespace private_library {

using Driver = lilygo_device_driver::TDisplayP4AirDriver;
namespace board = lilygo_device_driver::t_display_p4_air;
namespace gpio = board::gpio;
namespace device = board::device;

namespace es8388 {

inline std::shared_ptr<cpp_bus_driver::HardwareI2c1> g_i2c_bus;
inline std::shared_ptr<cpp_bus_driver::HardwareI2s> g_i2s_bus;
inline audio_codec_ctrl_if_t g_ctrl_if = {};
inline const audio_codec_data_if_t* g_data_if = nullptr;
inline const audio_codec_gpio_if_t* g_gpio_if = nullptr;
inline const audio_codec_if_t* g_codec_if = nullptr;

/**
 * @brief 将整数形式的 MCLK 倍频转换为 I2S 驱动枚举。
 * @param value MCLK 倍频值。
 * @return 匹配的 I2S MCLK 倍频枚举，不支持时返回 256 倍频。
 */
inline i2s_mclk_multiple_t MclkMultiple(int value) {
  switch (value) {
    case 128:
      return i2s_mclk_multiple_t::I2S_MCLK_MULTIPLE_128;
    case 256:
      return i2s_mclk_multiple_t::I2S_MCLK_MULTIPLE_256;
    case 384:
      return i2s_mclk_multiple_t::I2S_MCLK_MULTIPLE_384;
    case 512:
      return i2s_mclk_multiple_t::I2S_MCLK_MULTIPLE_512;
    default:
      return i2s_mclk_multiple_t::I2S_MCLK_MULTIPLE_256;
  }
}

/**
 * @brief 将整数形式的采样位宽转换为 I2S 驱动枚举。
 * @param value 单个采样点的位宽。
 * @return 匹配的 I2S 采样位宽枚举，不支持时返回 16 位。
 */
inline i2s_data_bit_width_t BitsPerSample(int value) {
  switch (value) {
    case 8:
      return i2s_data_bit_width_t::I2S_DATA_BIT_WIDTH_8BIT;
    case 16:
      return i2s_data_bit_width_t::I2S_DATA_BIT_WIDTH_16BIT;
    case 24:
      return i2s_data_bit_width_t::I2S_DATA_BIT_WIDTH_24BIT;
    case 32:
      return i2s_data_bit_width_t::I2S_DATA_BIT_WIDTH_32BIT;
    default:
      return i2s_data_bit_width_t::I2S_DATA_BIT_WIDTH_16BIT;
  }
}

/**
 * @brief 通过 I2C 写入 ES8388 寄存器。
 * @param ctrl 音频编解码器控制接口。
 * @param reg 要写入的寄存器地址。
 * @param reg_len 寄存器地址长度。
 * @param data 要写入的数据。
 * @param data_len 写入数据长度。
 * @return 写入成功时返回 ESP_CODEC_DEV_OK，否则返回错误码。
 */
inline int WriteReg(const audio_codec_ctrl_if_t* ctrl, int reg, int reg_len,
    void* data, int data_len) {
  int len = reg_len + data_len;
  if (len > 4) {
    printf("not support write huge data\n");
    return ESP_CODEC_DEV_WRITE_FAIL;
  }

  uint8_t write_data[4] = {0};
  int index = 0;
  if (reg_len > 1) {
    write_data[index++] = reg >> 8;
    write_data[index++] = reg & 0xff;
  } else {
    write_data[index++] = reg & 0xff;
  }

  uint8_t* write_buffer = static_cast<uint8_t*>(data);
  while (index < len) {
    write_data[index++] = *(write_buffer++);
  }

  if (!g_i2c_bus->Write(write_data, len)) {
    printf("es8388 i2c write failed\n");
    return ESP_CODEC_DEV_WRITE_FAIL;
  }

  return ESP_CODEC_DEV_OK;
}

/**
 * @brief 通过 I2C 读取 ES8388 寄存器。
 * @param ctrl 音频编解码器控制接口。
 * @param reg 要读取的寄存器地址。
 * @param reg_len 寄存器地址长度。
 * @param data 读取数据输出缓冲区。
 * @param data_len 读取数据长度。
 * @return 读取成功时返回 ESP_CODEC_DEV_OK，否则返回错误码。
 */
inline int ReadReg(const audio_codec_ctrl_if_t* ctrl, int reg, int reg_len,
    void* data, int data_len) {
  uint8_t address_data[2] = {0};
  if (reg_len > 1) {
    address_data[0] = reg >> 8;
    address_data[1] = reg & 0xff;
  } else {
    address_data[0] = reg & 0xff;
  }

  if (!g_i2c_bus->WriteRead(
          address_data, reg_len, static_cast<uint8_t*>(data), data_len)) {
    printf("es8388 i2c write read failed\n");
    return ESP_CODEC_DEV_READ_FAIL;
  }

  return ESP_CODEC_DEV_OK;
}

/**
 * @brief 创建 ES8388 使用的音频编解码器控制接口。
 * @param bus ES8388 所在的 I2C 总线。
 * @return 音频编解码器控制接口指针。
 */
inline audio_codec_ctrl_if_t* CreateCtrlIf(
    const std::shared_ptr<cpp_bus_driver::HardwareI2c1>& bus) {
  g_i2c_bus = bus;
  g_ctrl_if = {};
  g_ctrl_if.open = [](const audio_codec_ctrl_if_t* ctrl, void* cfg,
                       int cfg_size) -> int { return 0; };
  g_ctrl_if.is_open = [](const audio_codec_ctrl_if_t* ctrl) -> bool {
    return true;
  };
  g_ctrl_if.close = [](const audio_codec_ctrl_if_t* ctrl) -> int { return 0; };
  g_ctrl_if.write_reg = WriteReg;
  g_ctrl_if.read_reg = ReadReg;
  return &g_ctrl_if;
}

/**
 * @brief 创建音频采样参数。
 * @param sample_rate 音频采样率。
 * @return 可传入 esp_codec_dev_open 的采样参数。
 */
inline esp_codec_dev_sample_info_t CreateSampleInfo(
    int sample_rate = device::es8388::kSampleRate) {
  return {
      .bits_per_sample = BitsPerSample(device::es8388::kBitsPerSample),
      .channel = device::es8388::kChannel,
      .channel_mask = 0,
      .sample_rate = static_cast<uint32_t>(sample_rate),
      .mclk_multiple = MclkMultiple(device::es8388::kMclkMultiple),
  };
}

/**
 * @brief 创建 ES8388 使用的 I2C 总线对象。
 * @param driver 已初始化的板级驱动对象。
 * @return ES8388 I2C 总线对象。
 */
inline std::shared_ptr<cpp_bus_driver::HardwareI2c1> CreateI2cBus(
    Driver& driver) {
  return std::make_shared<cpp_bus_driver::HardwareI2c1>(
      driver.bus().xl9535_i2c_bus);
}

/**
 * @brief 创建 ES8388 使用的 I2S 总线对象。
 * @return ES8388 I2S 总线对象。
 */
inline std::shared_ptr<cpp_bus_driver::HardwareI2s> CreateI2sBus() {
  return std::make_shared<cpp_bus_driver::HardwareI2s>(
      gpio::es8388::kAdcData, gpio::es8388::kDacData,
      gpio::es8388::kWsLrck, gpio::es8388::kBclk, gpio::es8388::kMclk,
      i2s_port_t::I2S_NUM_0,
      cpp_bus_driver::HardwareI2s::DataMode::kInputOutput,
      cpp_bus_driver::HardwareI2s::I2sMode::kStd,
      i2s_clock_src_t::I2S_CLK_SRC_DEFAULT);
}

/**
 * @brief 初始化 ES8388 所需的 I2C、I2S 和功放控制总线。
 * @param driver 已初始化的板级驱动对象。
 * @param sample_rate I2S 采样率。
 * @return 总线初始化成功时返回 true，否则返回 false。
 */
inline bool InitBus(Driver& driver,
    int sample_rate = device::es8388::kSampleRate) {
  bool result = true;

  if (g_i2c_bus == nullptr) {
    g_i2c_bus = CreateI2cBus(driver);
  }
  if (g_i2s_bus == nullptr) {
    g_i2s_bus = CreateI2sBus();
  }

  result &= driver.chip().xl9535->GpioWrite(gpio::xl9535::kNs4150En,
      1);
  result &= g_i2c_bus->Init(cpp_bus_driver::kDefaultValue,
      device::es8388::kI2cAddress);
  result &= g_i2s_bus->Init(MclkMultiple(device::es8388::kMclkMultiple),
      static_cast<uint32_t>(sample_rate),
      BitsPerSample(device::es8388::kBitsPerSample));

  if (!result) {
    printf("es8388 bus init failed\n");
  }
  return result;
}

/**
 * @brief 创建 ES8388 音频编解码器设备。
 * @param driver 已初始化的板级驱动对象。
 * @param sample_rate 音频采样率。
 * @return 创建成功时返回编解码器设备句柄，否则返回 nullptr。
 */
inline esp_codec_dev_handle_t CreateCodecDevice(Driver& driver,
    int sample_rate = device::es8388::kSampleRate) {
  if (!InitBus(driver, sample_rate)) {
    return nullptr;
  }

  auto* i2s_bus = g_i2s_bus.get();
  audio_codec_i2s_cfg_t i2s_cfg = {
      .port = i2s_bus->port(),
      .rx_handle = i2s_bus->rx_handle(),
      .tx_handle = i2s_bus->tx_handle(),
      .clk_src = i2s_clock_src_t::I2S_CLK_SRC_DEFAULT,
  };
  g_data_if = audio_codec_new_i2s_data(&i2s_cfg);
  g_gpio_if = audio_codec_new_gpio();

  es8388_codec_cfg_t es8388_cfg = {
      .ctrl_if = CreateCtrlIf(g_i2c_bus),
      .gpio_if = g_gpio_if,
      .codec_mode = esp_codec_dec_work_mode_t::ESP_CODEC_DEV_WORK_MODE_BOTH,
      .master_mode = 0,
      .pa_pin = -1,
      .pa_reverted = false,
      .hw_gain =
          {
              .pa_voltage = 3.3,
              .codec_dac_voltage = 3.3,
              .pa_gain = 0.0,
          },
  };
  g_codec_if = es8388_codec_new(&es8388_cfg);

  esp_codec_dev_cfg_t dev_cfg = {
      .dev_type = esp_codec_dev_type_t::ESP_CODEC_DEV_TYPE_IN_OUT,
      .codec_if = g_codec_if,
      .data_if = g_data_if,
  };
  return esp_codec_dev_new(&dev_cfg);
}

/**
 * @brief 打开音频编解码器并设置输出音量和输入增益。
 * @param codec_dev 音频编解码器设备句柄。
 * @param sample_rate 音频采样率。
 * @param out_volume 输出音量。
 * @param in_gain 输入增益。
 * @return 打开成功时返回 true，否则返回 false。
 */
inline bool OpenCodec(esp_codec_dev_handle_t codec_dev, int sample_rate,
    int out_volume = 60, int in_gain = 20) {
  if (codec_dev == nullptr) {
    printf("codec device is null\n");
    return false;
  }

  auto sample_info = CreateSampleInfo(sample_rate);
  esp_err_t result = esp_codec_dev_open(codec_dev, &sample_info);
  if (result != ESP_OK) {
    printf("esp_codec_dev_open failed (error code: %#X)\n", result);
    return false;
  }

  result = esp_codec_dev_set_out_vol(codec_dev, out_volume);
  if (result != ESP_OK) {
    printf("esp_codec_dev_set_out_vol failed (error code: %#X)\n", result);
  }

  result = esp_codec_dev_set_in_gain(codec_dev, in_gain);
  if (result != ESP_OK) {
    printf("esp_codec_dev_set_in_gain failed (error code: %#X)\n", result);
  }

  return true;
}

/**
 * @brief 初始化并打开 ES8388 音频编解码器。
 * @param driver 已初始化的板级驱动对象。
 * @param sample_rate 音频采样率。
 * @param out_volume 输出音量。
 * @param in_gain 输入增益。
 * @return 初始化成功时返回编解码器设备句柄，否则返回 nullptr。
 */
inline esp_codec_dev_handle_t InitCodec(Driver& driver, int sample_rate,
    int out_volume = 60, int in_gain = 20) {
  esp_codec_dev_handle_t codec_dev = CreateCodecDevice(driver, sample_rate);
  if (OpenCodec(codec_dev, sample_rate, out_volume, in_gain)) {
    printf("codec device opened successfully\n");
  }
  return codec_dev;
}

}  // namespace es8388
}  // namespace private_library

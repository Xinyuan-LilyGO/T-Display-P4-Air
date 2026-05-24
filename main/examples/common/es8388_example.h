/*
 * @Description: ES8388 example helpers
 * @Author: LILYGO_L
 * @Date: 2026-05-24 16:30:00
 * @LastEditTime: 2026-05-24 16:30:00
 * @License: GPL 3.0
 */

#pragma once

#include <cstdint>
#include <memory>

#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"

#include "t_display_p4_air_example.h"

namespace t_display_p4_air_example {
namespace es8388 {

inline std::shared_ptr<cpp_bus_driver::HardwareI2c1> g_i2c_bus;
inline std::shared_ptr<cpp_bus_driver::HardwareI2s> g_i2s_bus;
inline audio_codec_ctrl_if_t g_ctrl_if = {};
inline const audio_codec_data_if_t* g_data_if = nullptr;
inline const audio_codec_gpio_if_t* g_gpio_if = nullptr;
inline const audio_codec_if_t* g_codec_if = nullptr;

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

inline bool InitBootKey() {
  static cpp_bus_driver::Tool tool;
  return tool.SetGpioMode(gpio::kChipBoot,
      cpp_bus_driver::Tool::GpioMode::kInput,
      cpp_bus_driver::Tool::GpioStatus::kPullup);
}

inline bool BootKeyPressed() {
  static cpp_bus_driver::Tool tool;
  return !tool.GpioRead(gpio::kChipBoot);
}

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

inline esp_codec_dev_sample_info_t CreateSampleInfo(
    int sample_rate = device::es8388::kSampleRate) {
  return {
      .bits_per_sample = BitsPerSample(device::es8388::kBitsPerSample),
      .channel = device::es8388::kChannel,
      .channel_mask = 0,
      .sample_rate = sample_rate,
      .mclk_multiple = MclkMultiple(device::es8388::kMclkMultiple),
  };
}

inline std::shared_ptr<cpp_bus_driver::HardwareI2c1> CreateI2cBus(
    Driver& driver) {
  return std::make_shared<cpp_bus_driver::HardwareI2c1>(
      driver.bus().xl9535_i2c_bus);
}

inline std::shared_ptr<cpp_bus_driver::HardwareI2s> CreateI2sBus() {
  return std::make_shared<cpp_bus_driver::HardwareI2s>(
      gpio::es8388::kAdcData, gpio::es8388::kDacData,
      gpio::es8388::kWsLrck, gpio::es8388::kBclk, gpio::es8388::kMclk,
      i2s_port_t::I2S_NUM_0,
      cpp_bus_driver::HardwareI2s::DataMode::kInputOutput,
      cpp_bus_driver::HardwareI2s::I2sMode::kStd,
      i2s_clock_src_t::I2S_CLK_SRC_APLL);
}

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
      cpp_bus_driver::Xl95x5::Value::kHigh);
  result &= g_i2c_bus->Init(cpp_bus_driver::kDefaultValue,
      device::es8388::kI2cAddress);
  result &= g_i2s_bus->Init(MclkMultiple(device::es8388::kMclkMultiple),
      sample_rate,
      BitsPerSample(device::es8388::kBitsPerSample));

  if (!result) {
    printf("es8388 bus init failed\n");
  }
  return result;
}

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
      .clk_src = i2s_clock_src_t::I2S_CLK_SRC_APLL,
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

inline esp_codec_dev_handle_t InitCodec(Driver& driver, int sample_rate,
    int out_volume = 60, int in_gain = 20) {
  esp_codec_dev_handle_t codec_dev = CreateCodecDevice(driver, sample_rate);
  if (OpenCodec(codec_dev, sample_rate, out_volume, in_gain)) {
    printf("codec device opened successfully\n");
  }
  return codec_dev;
}

}  // namespace es8388
}  // namespace t_display_p4_air_example

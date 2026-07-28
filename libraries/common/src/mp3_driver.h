/*
 * @Description: MP3 decode driver helpers
 * @Author: LILYGO_L
 * @Date: 2026-05-24 16:30:00
 * @LastEditTime: 2026-05-24 16:30:00
 * @License: GPL 3.0
 */

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>

#include "esp_audio_dec.h"
#include "esp_audio_dec_default.h"
#include "esp_codec_dev.h"

namespace common {
namespace mp3 {

inline bool OpenCodec(esp_codec_dev_handle_t codec_dev, uint32_t sample_rate) {
  esp_codec_dev_sample_info_t sample_info = {
      .bits_per_sample = 16,
      .channel = 2,
      .channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0) |
                      ESP_CODEC_DEV_MAKE_CHANNEL_MASK(1),
      .sample_rate = sample_rate,
      .mclk_multiple = 256,
  };

  return esp_codec_dev_open(codec_dev, &sample_info) == ESP_CODEC_DEV_OK;
}

/**
 * @brief 将 ID3v2 synchsafe 编码的 4 字节数据转换为整数。
 * @param buffer 指向 4 字节 synchsafe 数据的缓冲区。
 * @return 转换后的无符号整数。
 */
inline uint32_t SynchsafeToUint32(const uint8_t* buffer) {
  return ((buffer[0] & 0x7F) << 21) | ((buffer[1] & 0x7F) << 14) |
         ((buffer[2] & 0x7F) << 7) | (buffer[3] & 0x7F);
}

/**
 * @brief 将大端序 4 字节数据转换为整数。
 * @param buffer 指向 4 字节大端序数据的缓冲区。
 * @return 转换后的无符号整数。
 */
inline uint32_t BigEndianToUint32(const uint8_t* buffer) {
  return (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) |
         buffer[3];
}

/**
 * @brief 解析 MP3 文件开头的 ID3v2 信息并同步音频采样率。
 * @param file 已打开的 MP3 文件。
 * @param path MP3 文件路径，用于日志输出。
 * @param codec_dev 音频编解码器设备句柄。
 * @param audio_sample_rate 当前音频采样率，解析到不同采样率时会更新。
 * @return MP3 音频数据起始偏移。
 */
inline size_t ParseId3v2(FILE* file, const char* path,
    esp_codec_dev_handle_t codec_dev, uint32_t* audio_sample_rate) {
  long current_pos = ftell(file);
  fseek(file, 0, SEEK_END);
  long file_total_size = ftell(file);
  fseek(file, current_pos, SEEK_SET);

  uint8_t header[10] = {0};
  bool has_id3v2 = false;
  uint32_t id3v2_size = 0;
  uint8_t major_version = 0;

  if (fread(header, 1, sizeof(header), file) == sizeof(header) &&
      memcmp(header, "ID3", 3) == 0) {
    has_id3v2 = true;
    major_version = header[3];
    id3v2_size = SynchsafeToUint32(header + 6);

    printf("Input from '%s':\n", path);
    printf("  Metadata:\n");

    size_t pos = sizeof(header);
    if (header[5] & 0x40) {
      uint8_t ext_buffer[4] = {0};
      if (fread(ext_buffer, 1, sizeof(ext_buffer), file) == sizeof(ext_buffer)) {
        uint32_t ext_size = major_version == 4
                                ? SynchsafeToUint32(ext_buffer)
                                : BigEndianToUint32(ext_buffer);
        if (ext_size > sizeof(ext_buffer)) {
          fseek(file, ext_size - sizeof(ext_buffer), SEEK_CUR);
        }
        pos += ext_size;
      }
    }

    while (pos < sizeof(header) + id3v2_size) {
      char frame_id[5] = {0};
      if (fread(frame_id, 1, 4, file) != 4 || frame_id[0] == 0) {
        break;
      }
      pos += 4;

      uint8_t size_buffer[4] = {0};
      if (fread(size_buffer, 1, sizeof(size_buffer), file) !=
          sizeof(size_buffer)) {
        break;
      }
      uint32_t frame_size = major_version == 4
                                ? SynchsafeToUint32(size_buffer)
                                : BigEndianToUint32(size_buffer);
      pos += sizeof(size_buffer);

      fseek(file, 2, SEEK_CUR);
      pos += 2;

      const char* tag_key = nullptr;
      if (strcmp(frame_id, "TIT2") == 0) {
        tag_key = "title";
      } else if (strcmp(frame_id, "TALB") == 0) {
        tag_key = "album";
      } else if (strcmp(frame_id, "TPE1") == 0) {
        tag_key = "artist";
      } else if (strcmp(frame_id, "TPE2") == 0) {
        tag_key = "album_artist";
      } else if (strcmp(frame_id, "TCON") == 0) {
        tag_key = "genre";
      } else if (strcmp(frame_id, "TSSE") == 0) {
        tag_key = "encoder";
      }

      if (tag_key != nullptr && frame_size > 1) {
        auto frame_data = std::make_unique<uint8_t[]>(frame_size + 1);
        fread(frame_data.get(), 1, frame_size, file);
        frame_data[frame_size] = 0;

        uint8_t encoding = frame_data[0];
        if (encoding == 0 || encoding == 3) {
          printf("    %-16s: %s\n", tag_key,
              reinterpret_cast<char*>(frame_data.get() + 1));
        } else {
          printf("    %-16s: (UTF-16 text)\n", tag_key);
        }
      } else {
        fseek(file, frame_size, SEEK_CUR);
      }
      pos += frame_size;
    }
  } else {
    printf("Error metadata input from '%s'\n", path);
    rewind(file);
  }

  long mp3_data_start = has_id3v2 ? (sizeof(header) + id3v2_size) : 0;
  fseek(file, mp3_data_start, SEEK_SET);

  uint8_t frame_header[4] = {0};
  bool header_found = false;
  uint32_t sample_rate = 0;
  uint32_t bitrate_kbps = 0;
  const char* channel_str = "stereo";

  for (int i = 0; i < 8192; ++i) {
    if (fread(frame_header, 1, sizeof(frame_header), file) !=
        sizeof(frame_header)) {
      break;
    }

    if (frame_header[0] == 0xFF && (frame_header[1] & 0xE0) == 0xE0) {
      uint8_t m_idx = (frame_header[1] >> 3) & 0x03;
      uint8_t l_idx = (frame_header[1] >> 1) & 0x03;
      uint8_t br_idx = (frame_header[2] >> 4) & 0x0F;
      uint8_t sr_idx = (frame_header[2] >> 2) & 0x03;
      uint8_t padding = (frame_header[2] >> 1) & 0x01;
      uint8_t ch_mode = (frame_header[3] >> 6) & 0x03;

      if (l_idx != 1 || br_idx == 0 || br_idx == 15 || sr_idx == 3) {
        fseek(file, -3, SEEK_CUR);
        continue;
      }

      static const uint32_t kSampleRateTable[4][4] = {
          {11025, 12000, 8000, 0},
          {0, 0, 0, 0},
          {22050, 24000, 16000, 0},
          {44100, 48000, 32000, 0},
      };
      sample_rate = kSampleRateTable[m_idx][sr_idx];

      static const uint16_t kBitrateTable[2][16] = {
          {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144,
              160, 0},
          {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224,
              256, 320, 0},
      };
      bitrate_kbps = kBitrateTable[(m_idx == 3) ? 1 : 0][br_idx];

      if (sample_rate > 0 && bitrate_kbps > 0) {
        uint32_t frame_len = m_idx == 3
                                 ? (144000 * bitrate_kbps / sample_rate +
                                       padding)
                                 : (72000 * bitrate_kbps / sample_rate +
                                       padding);
        long sync_pos = ftell(file) - sizeof(frame_header);
        uint8_t peek_buffer[40] = {0};
        fread(peek_buffer, 1, sizeof(peek_buffer), file);

        bool is_xing_frame = false;
        for (int p = 0; p < 36; ++p) {
          if (memcmp(peek_buffer + p, "Xing", 4) == 0 ||
              memcmp(peek_buffer + p, "Info", 4) == 0) {
            is_xing_frame = true;
            break;
          }
        }

        if (is_xing_frame) {
          fseek(file, sync_pos + frame_len, SEEK_SET);
          continue;
        }

        fseek(file, sync_pos + sizeof(frame_header), SEEK_SET);
        channel_str = (ch_mode == 3) ? "mono" : "stereo";
        header_found = true;
        break;
      }
    }
    fseek(file, -3, SEEK_CUR);
  }

  if (header_found) {
    double duration =
        ((file_total_size - mp3_data_start) * 8.0) / (bitrate_kbps * 1000.0);
    int hour = static_cast<int>(duration) / 3600;
    int minute = (static_cast<int>(duration) % 3600) / 60;
    double second = duration - (hour * 3600) - (minute * 60);

    printf("  Duration: %02d:%02d:%05.2f, bitrate: %lu kb/s\n", hour,
        minute, second, static_cast<unsigned long>(bitrate_kbps));
    printf("  Stream #0:0: Audio: mp3, %lu Hz, %s\n",
        static_cast<unsigned long>(sample_rate), channel_str);

    if (sample_rate != *audio_sample_rate) {
      printf("match sample rate %luhz -> %luhz\n",
          static_cast<unsigned long>(*audio_sample_rate),
          static_cast<unsigned long>(sample_rate));
      esp_codec_dev_close(codec_dev);
      if (OpenCodec(codec_dev, sample_rate)) {
        *audio_sample_rate = sample_rate;
      }
    }
  } else {
    printf("The ID3v2 header does not exist\n");
  }

  fseek(file, mp3_data_start, SEEK_SET);
  return static_cast<size_t>(mp3_data_start);
}

/**
 * @brief 解析并打印 MP3 文件尾部的 ID3v1 信息。
 * @param file 已打开的 MP3 文件。
 */
inline void ParseId3v1(FILE* file) {
  long original_pos = ftell(file);

  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  if (file_size < 128) {
    fseek(file, original_pos, SEEK_SET);
    return;
  }

  fseek(file, -128, SEEK_END);
  uint8_t tag[128] = {0};
  if (fread(tag, 1, sizeof(tag), file) != sizeof(tag)) {
    fseek(file, original_pos, SEEK_SET);
    return;
  }

  if (memcmp(tag, "TAG", 3) != 0) {
    fseek(file, original_pos, SEEK_SET);
    return;
  }

  printf("ID3v1 tag detected:\n");
  printf("Title: %.30s\n", reinterpret_cast<char*>(tag + 3));
  printf("Artist: %.30s\n", reinterpret_cast<char*>(tag + 33));
  printf("Album: %.30s\n", reinterpret_cast<char*>(tag + 63));
  printf("Year: %.4s\n", reinterpret_cast<char*>(tag + 93));
  printf("Genre: %u\n", tag[127]);

  fseek(file, original_pos, SEEK_SET);
}

/**
 * @brief 解码并播放指定 MP3 文件。
 * @param path MP3 文件路径。
 * @param codec_dev 音频编解码器设备句柄。
 * @param audio_sample_rate 当前音频采样率，播放过程中可能被同步更新。
 */
inline void PlayFile(const char* path, esp_codec_dev_handle_t codec_dev,
    uint32_t* audio_sample_rate) {
  esp_audio_dec_register_default();
  esp_audio_dec_cfg_t dec_cfg = {
      .type = ESP_AUDIO_TYPE_MP3,
  };

  esp_audio_dec_handle_t decoder = nullptr;
  if (esp_audio_dec_open(&dec_cfg, &decoder) != ESP_AUDIO_ERR_OK) {
    printf("esp_audio_dec_open failed\n");
    esp_audio_dec_unregister_default();
    return;
  }

  FILE* file = fopen(path, "rb");
  if (file == nullptr) {
    printf("fopen mp3 failed: %s\n", path);
    esp_audio_dec_close(decoder);
    esp_audio_dec_unregister_default();
    return;
  }

  size_t mp3_offset =
      ParseId3v2(file, path, codec_dev, audio_sample_rate);
  ParseId3v1(file);
  fseek(file, mp3_offset, SEEK_SET);

  constexpr size_t kReadBufferSize = 4 * 1024;
  constexpr size_t kPcmBufferSize = 8 * 1024;
  auto read_buffer = std::make_unique<uint8_t[]>(kReadBufferSize);
  auto pcm_buffer = std::make_unique<uint8_t[]>(kPcmBufferSize);
  size_t remain_bytes = 0;

  while (1) {
    size_t read_bytes = fread(read_buffer.get() + remain_bytes, 1,
        kReadBufferSize - remain_bytes, file);
    size_t total_bytes = remain_bytes + read_bytes;
    if (total_bytes == 0) {
      break;
    }

    esp_audio_dec_in_raw_t in_raw = {
        .buffer = read_buffer.get(),
        .len = total_bytes,
    };
    esp_audio_dec_out_frame_t out_frame = {
        .buffer = pcm_buffer.get(),
        .len = kPcmBufferSize,
    };

    while (in_raw.len > 0) {
      esp_audio_err_t result =
          esp_audio_dec_process(decoder, &in_raw, &out_frame);
      if (result != ESP_AUDIO_ERR_OK) {
        if (result == ESP_AUDIO_ERR_FAIL) {
          printf("esp_audio_dec_process failed\n");
        }
        break;
      }

      if (out_frame.decoded_size > 0) {
        esp_codec_dev_write(
            codec_dev, out_frame.buffer, out_frame.decoded_size);
      }

      if (in_raw.consumed == 0) {
        break;
      }

      in_raw.buffer += in_raw.consumed;
      in_raw.len -= in_raw.consumed;
    }

    remain_bytes = in_raw.len;
    if (remain_bytes > 0) {
      memmove(read_buffer.get(), in_raw.buffer, remain_bytes);
    }

    if (read_bytes == 0 && remain_bytes == total_bytes) {
      break;
    }
  }

  fclose(file);
  esp_audio_dec_close(decoder);
  esp_audio_dec_unregister_default();
}

}  // namespace mp3
}  // namespace common

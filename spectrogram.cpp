#include "spectrogram.hpp"
#include "ma_wrapper.hpp"
#include "miniaudio.h"
#include "sliding_dft.hpp"
#include <GL/gl.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <imgui.h>
#include <iostream>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <vector>

constexpr size_t time_window_size = 32;
constexpr size_t pixels_time_window_size = 1;

// Simple helper function to load an image into a OpenGL texture with common
// settings
bool LoadTextureFromMemory(unsigned char *image_data, size_t image_width,
                           size_t image_height, GLuint *out_texture) {

  // Create a OpenGL texture identifier
  GLuint image_texture;
  glGenTextures(1, &image_texture);
  glBindTexture(GL_TEXTURE_2D, image_texture);

  // Setup filtering parameters for display
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // Upload pixels into texture
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, image_data);
  if (GLenum err = glGetError(); err != GL_NO_ERROR)
    throw std::runtime_error("Bad texture");

  *out_texture = image_texture;

  return true;
}

struct Color {
  uint8_t r, g, b, a;
};

Color colormap(float t) {
  t = std::clamp(t, 0.0f, 1.0f);

  // black -> purple -> magenta -> orange -> white

  float r, g, b;

  if (t < 0.25f) {
    // black -> purple
    float k = t / 0.25f;
    r = 0.2f * k;
    g = 0.0f;
    b = 0.4f * k;
  } else if (t < 0.5f) {
    // purple -> magenta
    float k = (t - 0.25f) / 0.25f;
    r = 0.2f + 0.6f * k;
    g = 0.0f;
    b = 0.4f + 0.4f * k;
  } else if (t < 0.75f) {
    // magenta -> orange
    float k = (t - 0.5f) / 0.25f;
    r = 0.8f + 0.2f * k;
    g = 0.0f + 0.6f * k;
    b = 0.8f - 0.8f * k;
  } else {
    // orange -> white
    float k = (t - 0.75f) / 0.25f;
    r = 1.0f;
    g = 0.6f + 0.4f * k;
    b = k;
  }

  return {(uint8_t)(r * 255), (uint8_t)(g * 255), (uint8_t)(b * 255), 255};
}

template <typename coef_type, std::size_t dft_width>
std::vector<std::array<coef_type, dft_width>> calc_spec_data() {
  SlidingDFT<coef_type, dft_width> dft;

  MA::Decoder decoder(input_file);

  ma_result result;
  ma_uint64 file_len_pcm;
  result =
      ma_data_source_get_length_in_pcm_frames(&decoder.get(), &file_len_pcm);

  const size_t num_fft_windows = file_len_pcm / time_window_size;
  std::vector<std::array<coef_type, dft_width>> dft_res(num_fft_windows);

  constexpr ma_uint64 bufLen = 512;

  using Frame = float;
  Frame buf[bufLen];

  ma_uint64 num_processed_frames = 0;

  while (true) {
    ma_uint64 framesRead = 0;
    result =
        ma_decoder_read_pcm_frames(&decoder.get(), buf, bufLen, &framesRead);
    if (result == MA_AT_END)
      break;

    if (result != MA_SUCCESS) {
      throw std::runtime_error("Failed to read or convert PCM frames");
    }

#if FILE_PARTS_ARE_SUPPORTED
    bool exit = false;

    if (cur_frame + framesRead >= file_len_pcm) {
      framesRead = file_len_pcm - cur_frame;
      exit = true;
    }
#endif

    for (ma_uint64 i = 0; i < framesRead; ++i) {
      dft.update(buf[i]);

      if (num_processed_frames % time_window_size == time_window_size - 1) {
        for (size_t j = 0; j < dft_width; ++j) {
          dft_res[num_processed_frames / time_window_size][j] =
              std::abs(dft.dft[j]);
        }
      }

      num_processed_frames++;
    }
#if FILE_PARTS_ARE_SUPPORTED
    if (exit)
      break;
#endif
  }
  return dft_res;
}

template <typename coef_type, std::size_t len>
std::vector<unsigned char>
raw_to_image(const std::vector<std::array<coef_type, len>> &raw_data,
             unsigned int &image_width, unsigned int &image_height) {
  //*
  float max_datum = -std::numeric_limits<float>::infinity(),
        min_datum = std::numeric_limits<float>::infinity();
  for (auto &col : raw_data)
    for (auto d : col) {
      max_datum = std::max(max_datum, d);
      min_datum = std::min(min_datum, d);
    }
  const float min_db = 20.0f * std::log10(min_datum + 1e-12f);
  const float max_db = 20.0f * std::log10(max_datum + 1e-12f);
  std::clog << min_db << ' ' << max_db << std::endl;
  //*/
  // const float min_db = -100, max_db = -20;

  image_width = raw_data.size() * pixels_time_window_size;
  image_height = 512;

  auto hz_to_mel = [](float hz) { return 2595 * std::log10(1 + hz / 700); };
  const float min_mel = hz_to_mel(0), max_mel = hz_to_mel(24'000);
  assert(min_mel == 0);

  std::vector<unsigned char> result(image_width * image_height * 4);

  for (size_t i = 0; i < raw_data.size(); ++i) {
    for (size_t coef = 0; coef < len / 2; ++coef) {

      float db = 20.0f * std::log10(raw_data[i][coef] + 1e-12f);
      db = std::clamp(db, min_db, max_db);

      float norm = (db - min_db) / (max_db - min_db);
      Color c = colormap(norm);

      size_t y_begin =
          std::lerp(image_height - 1, 0,
                    (hz_to_mel(coef * 1.f / len * 48000) - min_mel) /
                        (max_mel - min_mel));
      size_t y_end =
          std::lerp(image_height - 1, 0,
                    (hz_to_mel((coef + 1) * 1.f / len * 48000) - min_mel) /
                        (max_mel - min_mel));

      for (size_t y = y_begin; y-- > y_end;)
        for (size_t pos_in_window = 0; pos_in_window < pixels_time_window_size;
             ++pos_in_window) {
          size_t x = i * pixels_time_window_size + pos_in_window;

          size_t pos = (y * image_width + x) * 4;

          result[pos + 0] = c.r;
          result[pos + 1] = c.g;
          result[pos + 2] = c.b;
          result[pos + 3] = 255;
        }
      // *reinterpret_cast<uint32_t *>(result + pos * 4) =
      // static_cast<uint32_t>(rand());
      // static_cast<uint>(234);
    }
  }

  return result;
}

GLuint make_spectrogram_texture(unsigned int &image_width,
                                unsigned int &image_height) {
  constexpr unsigned int dft_width = 2048;
  auto fft_data = calc_spec_data<float, dft_width>();
  // fft_data.resize(64);

  std::vector<unsigned char> image_data(
      raw_to_image(fft_data, image_width, image_height));

  GLuint texture;
  LoadTextureFromMemory(image_data.data(), image_width, image_height, &texture);
  return texture;
}

#include "miniaudio.h"
#include <stdexcept>

namespace MA {
struct Decoder {
private:
  ma_decoder _decoder;

public:
  Decoder(const std::string &file_name,
          const ma_decoder_config &config = makeDefaultConfig()) {
    if (ma_decoder_init_file(file_name.c_str(), &config, &_decoder) !=
        MA_SUCCESS) {
      throw std::runtime_error("Could not load file: " + file_name);
    }
  }

  ~Decoder() { ma_decoder_uninit(&_decoder); }

  // Decoder(const Decoder &) = delete; // TODO
  // Decoder &operator=(const Decoder &) = delete;

  const ma_decoder &get() const { return _decoder; } // TODO
  ma_decoder &get() { return _decoder; }

  size_t readFrames(void *output, size_t framesToRead) {
    ma_uint64 framesRead = 0;
    ma_result result = ma_decoder_read_pcm_frames(&_decoder, output,
                                                  framesToRead, &framesRead);

    if (result != MA_SUCCESS && result != MA_AT_END) {
      throw std::runtime_error("Read error");
    }

    return static_cast<size_t>(framesRead);
  }

  void seekToFrame(size_t frame) {
    if (ma_decoder_seek_to_pcm_frame(&_decoder, frame) !=
        MA_SUCCESS) { // TODO: return bool (==MA_SUCCESS)?
      throw std::runtime_error("Seek to start failed");
    }
  }

private:
  static ma_decoder_config makeDefaultConfig() {
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 1, 48000);
    // config.encodingFormat = ma_encoding_format_wav;
    return config;
  }
};

class Device {
  ma_device d;

public:
  explicit Device(ma_device_config cfg) {
    if (ma_device_init(NULL, &cfg, &d) != MA_SUCCESS) {
      throw std::runtime_error("Failed to open playback device.\n");
    }
  }
  const ma_device &get() const { return d; }
  ma_device &get() { return d; }
  ~Device() { ma_device_uninit(&d); }
};
}; // namespace MA

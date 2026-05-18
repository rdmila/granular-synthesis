#include "ma_wrapper.hpp"
#include "miniaudio.h"
#include "sliding_dft.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <vector>

struct GrainInfo {
  size_t frame_pos;
  float RMP_percent;
};

struct MainFreq {
  struct Point {
    ma_uint64 x;
    float y;
  };
  std::vector<Point> points;

  float get_freq(ma_uint64 x) {
    for (int i = 0; i + 1 < points.size(); ++i) {
      if (points[i].x <= x && x <= points[i + 1].x) {
        return points[i].y + static_cast<float>(x - points[i].x) /
                                 (points[i + 1].x - points[i].x) *
                                 (points[i + 1].y - points[i].y);
      }
    }
    throw std::runtime_error("Fragment out of bounds");
  }
};

struct SourceMetaInfo {
  MainFreq main_freq;
  int cylinders_cnt;
};

std::vector<GrainInfo>
split_grains(MA::Decoder &decoder,
             SourceMetaInfo &info) { // TODO: decoder to "Reader" abstraction
  std::vector<GrainInfo> result;
  size_t begin, end;

  // if (info.main_freq.points.empty()) {
  //   begin = 0;
  //   ma_uint64 len;
  //   ma_data_source_get_length_in_pcm_frames(&decoder.get(), &len);
  //   end = static_cast<size_t>(len - 1);
  // } else {
  begin = info.main_freq.points.begin()->x;
  end = info.main_freq.points.rbegin()->x;
  // }

  float RPM_max = info.main_freq.get_freq(end);

  ma_data_source_set_range_in_pcm_frames(&decoder.get(), begin, end);

  size_t source_rate = 48000; // TODO: get from decoder
                              //
  auto find_start = [](MA::Decoder &decoder) { return 0; }; // TODO
  size_t pcm_pos = find_start(decoder); // relative to "begin"
                                        //
  constexpr size_t buf_size = 1024;
  float *buffer = new float[buf_size];
  //
  while (true) {

    size_t frames_read = decoder.readFrames(buffer, buf_size);
    if (frames_read < buf_size) {
      break;
    }

    //
    float base_freq_per_sec = info.main_freq.get_freq(begin + pcm_pos);

    constexpr size_t DFTsize = 1024;
    static SlidingDFT<float, DFTsize> dft;
    float base_freq_samples = base_freq_per_sec * DFTsize / source_rate;
    for (size_t i = 0; i < DFTsize; ++i) {
      assert(abs(buffer[i]) <= 1.1);
      dft.update(buffer[i]);
    }

    size_t loudest_freq_idx = -1;
    float max_coef_norm = -1;

    float eps = 0.25;
    // std::cout << pcm_pos << std::endl;

    size_t left = base_freq_samples * (1 - eps);
    size_t right = base_freq_samples * (1 + eps);

    // left = 1;
    // right = DFTsize - 1;
    for (size_t i = left; i <= right; ++i) {
      // std::cout << i << " " << base_freq_samples << std::endl;
      if (i == 0) {
        continue;
      }

      float norm = std::norm(dft.dft[i]);

      if (norm >= max_coef_norm) {
        loudest_freq_idx = i;
        max_coef_norm = norm;
      }
    }

    std::cout << loudest_freq_idx << " " << pcm_pos << std::endl;

    float true_base_freq =
        static_cast<float>(loudest_freq_idx) * source_rate / DFTsize;

    size_t grain_length_frames =
        info.cylinders_cnt * DFTsize / loudest_freq_idx;

    result.emplace_back(pcm_pos, true_base_freq / RPM_max);

    pcm_pos += grain_length_frames;
    ma_data_source_seek_to_pcm_frame(&decoder.get(), begin + pcm_pos);
  }
  return result;
}

int main() {
  // read wav
  const char *file = "sample2.wav";
  MA::Decoder decoder(file);

  SourceMetaInfo info;
  info.cylinders_cnt = 6;
  ma_uint64 len;
  ma_data_source_get_length_in_pcm_frames(&decoder.get(), &len);
  // info.main_freq.points = {{45000, 100}, {90000, 18000}};

  info.main_freq.points = {{0, 9000}, {len - 1, 18000}};

  std::vector<GrainInfo> partition = split_grains(decoder, info);
}

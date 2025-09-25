#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>
#include <whisper.h>

static bool load_wav(const char* path, std::vector<float>& pcm, int& sample_rate) {
  drwav wav;
  if (!drwav_init_file(&wav, path, nullptr)) return false;
  sample_rate = static_cast<int>(wav.sampleRate);
  if (wav.channels != 1 || wav.totalPCMFrameCount == 0) {
    drwav_uninit(&wav);
    return false;
  }
  pcm.resize(static_cast<size_t>(wav.totalPCMFrameCount));
  if (pcm.empty()) {
    drwav_uninit(&wav);
    return false;
  }
  drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, pcm.data());
  drwav_uninit(&wav);
  return true;
}

static int clamp_threads() {
  unsigned int threads = std::thread::hardware_concurrency();
  if (threads == 0) threads = 1;
  if (threads > 4) threads = 4;
  return static_cast<int>(threads);
}

int main() {
  const char* sample_path = std::getenv("WHISPER_SAMPLE_PATH");
  if (!sample_path || std::strlen(sample_path) == 0) sample_path = "whisper-jfk.wav";
  const char* model_path = std::getenv("WHISPER_MODEL_PATH");
  if (!model_path || std::strlen(model_path) == 0) model_path = "whisper-tiny-en.bin";

  std::vector<float> pcm;
  int sample_rate = 0;
  if (!load_wav(sample_path, pcm, sample_rate)) {
    std::fprintf(stderr, "failed to load %s\n", sample_path);
    return 1;
  }
  if (sample_rate != WHISPER_SAMPLE_RATE) {
    std::fprintf(stderr, "unexpected sample rate %d\n", sample_rate);
    return 1;
  }

  std::printf("whisper system: %s\n", whisper_print_system_info());

  whisper_context_params ctx_params = whisper_context_default_params();
  whisper_context* ctx = whisper_init_from_file_with_params(model_path, ctx_params);
  if (!ctx) {
    std::fprintf(stderr, "failed to load %s\n", model_path);
    return 1;
  }

  whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
  params.n_threads = clamp_threads();
  params.language = "en";
  params.translate = false;
  params.print_progress = false;
  params.print_realtime = false;
  params.print_timestamps = false;
  params.print_special = false;
  params.no_context = true;
  params.single_segment = true;

  if (whisper_full(ctx, params, pcm.data(), static_cast<int>(pcm.size())) != 0) {
    std::fprintf(stderr, "transcription failed\n");
    whisper_free(ctx);
    return 1;
  }

  const int segments = whisper_full_n_segments(ctx);
  for (int i = 0; i < segments; ++i) {
    const char* text = whisper_full_get_segment_text(ctx, i);
    if (text) {
      std::printf("segment %d: %s\n", i, text);
    }
  }
  if (segments == 0) {
    std::printf("no segments produced\n");
  }

  whisper_free(ctx);
  return 0;
}

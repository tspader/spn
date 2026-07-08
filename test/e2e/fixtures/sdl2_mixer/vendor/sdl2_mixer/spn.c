#include "spn.h"

SPN_EXPORT
s32 run_cmake(spn_t* spn, spn_node_ctx_t* ctx) {
  const spn_t* sdl2 = spn_get_dep(spn, "sdl2");
  if (!sdl2) {
    return 1;
  }

  spn_cmake_t* cmake = spn_cmake_new(spn);
  spn_cmake_add_define(cmake, "CMAKE_PREFIX_PATH", spn_get_dir(sdl2, SPN_DIR_STORE));
  spn_cmake_add_define(cmake, "BUILD_SHARED_LIBS", "OFF");
  spn_cmake_add_define(cmake, "SDL2MIXER_DEBUG_POSTFIX", "");
  spn_cmake_add_define(cmake, "SDL2MIXER_VENDORED", "OFF");
  spn_cmake_add_define(cmake, "SDL2MIXER_SAMPLES", "OFF");
  spn_cmake_add_define(cmake, "SDL2MIXER_CMD", "OFF");
  spn_cmake_add_define(cmake, "SDL2MIXER_FLAC", "OFF");
  spn_cmake_add_define(cmake, "SDL2MIXER_GME", "OFF");
  spn_cmake_add_define(cmake, "SDL2MIXER_MOD", "OFF");
  spn_cmake_add_define(cmake, "SDL2MIXER_MP3", "OFF");
  spn_cmake_add_define(cmake, "SDL2MIXER_MIDI", "OFF");
  spn_cmake_add_define(cmake, "SDL2MIXER_OPUS", "OFF");
  spn_cmake_add_define(cmake, "SDL2MIXER_VORBIS", "OFF");
  spn_cmake_add_define(cmake, "SDL2MIXER_WAVPACK", "OFF");
  spn_cmake_add_define(cmake, "SDL2MIXER_WAVE", "ON");
  if (spn_cmake_run(cmake)) {
    return 1;
  }
  return spn_cmake_install(cmake);
}

SPN_EXPORT
spn_err_t configure(spn_t* spn, spn_config_t* config) {
  spn_node_t* cmake = spn_add_node(config, "cmake");
  spn_node_set_fn(cmake, "run_cmake");
  return SPN_OK;
}

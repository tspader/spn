local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'ggml-org/whisper.cpp',
  lib = 'whisper',
  kinds = { 'shared', 'static' },
  build = function(builder)
    builder:cmake({
      install = true,
      defines = {
        { 'BUILD_SHARED_LIBS', builder.kind == spn.build_kind.shared },
        { 'WHISPER_BUILD_TESTS', false },
        { 'WHISPER_BUILD_EXAMPLES', false },
        { 'WHISPER_BUILD_SERVER', false },
        { 'WHISPER_CURL', false },
        { 'WHISPER_SDL2', false },
        { 'GGML_CCACHE', false },
      }
    })
    builder:copy({
      { builder:source('samples/jfk.wav'), builder:vendor('whisper-jfk.wav') },
      { builder:source('models/for-tests-ggml-tiny.en.bin'), builder:vendor('whisper-tiny-en.bin') },
    })
  end,
})

return recipe

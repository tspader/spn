local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'ggml-org/whisper.cpp',
  libs = { 'whisper' },
  kinds = { 'shared', 'static' },
  build = function(builder)
    builder:cmake({
      install = true,
      defines = {
        { 'WHISPER_BUILD_TESTS', false },
        { 'WHISPER_BUILD_EXAMPLES', false },
        { 'WHISPER_BUILD_SERVER', false },
        { 'WHISPER_CURL', false },
        { 'WHISPER_SDL2', false },
        { 'WHISPER_USE_SYSTEM_GGML', true },
      }
    })
    builder:copy({
      { builder:source('samples/jfk.wav'), builder:vendor('whisper-jfk.wav') },
      { builder:source('models/for-tests-ggml-tiny.en.bin'), builder:vendor('whisper-tiny-en.bin') },

    })
  end,
})

return recipe

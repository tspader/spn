local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'ggml-org/ggml',
  lib = 'ggml',
  kinds = { 'shared', 'static' },
  build = function(builder)
    builder:cmake({
      install = true,
      defines = {
        { 'BUILD_SHARED_LIBS', builder.kind == spn.build_kind.shared },
        { 'GGML_BUILD_TESTS', false },
        { 'GGML_BUILD_EXAMPLES', false },
      }
    })
    builder:copy({
      { builder:source('examples'), builder:vendor() },
    })
  end,
})

return recipe


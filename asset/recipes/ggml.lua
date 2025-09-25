local spn = require('spn')
local inspect = require('inspect')

local recipe = spn.recipes.basic({
  git = 'ggml-org/ggml',
  kinds = { 'shared', 'static' },
  libs = {
    'ggml',
    'ggml-base',
  },
  options = {
    backend = 'cpu', -- 'cuda', 'metal'
  },
  configure = function(self, options)
    table.insert(self.libs, string.format('ggml-%s', options.backend))
  end,
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


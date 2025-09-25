local spn = require('spn')
local dbg = require('debugger')

local recipe = spn.recipes.basic({
  git = 'ggml-org/ggml',
  kinds = { 'shared', 'static' },
  libs = {
    'ggml-base',
    'ggml',
  },
  options = {
    backends = {
      cpu = true,
      cuda = false,
      metal = false
    }
  },
  configure = function(self)
    for backends, enabled in pairs(self.spec.options.backends) do
      if enabled then
        table.insert(self.libs, 1, string.format('ggml-%s', backends))
      end
    end
  end,
  build = function(builder)
    builder:cmake({
      install = true,
      defines = {
        { 'BUILD_SHARED_LIBS', builder.kind == spn.build_kind.shared },
        { 'GGML_BUILD_TESTS', false },
        { 'GGML_BUILD_EXAMPLES', false },
        { 'GGML_CPU', builder.options.backends.cpu },
        { 'GGML_CUDA', builder.options.backends.cuda },
        { 'GGML_METAL', builder.options.backends.metal },
      }
    })

    builder:copy({
      { builder:source('examples'), builder:vendor() },
    })
  end,
})

return recipe


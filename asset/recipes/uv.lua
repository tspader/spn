local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'libuv/libuv',
  libs = { 'uv' },
  kinds = { 'shared', 'static' },
  build = function(builder)
    local shared = builder.kind == 'shared'
    builder:cmake({
      defines = {
        { 'LIBUV_BUILD_TESTS', false },
        { 'LIBUV_BUILD_BENCHMARKS', false },
        { 'LIBUV_BUILD_SHARED', shared },
        { 'LIBUV_BUILD_STATIC', not shared },
      },
      install = true,
    })

  end,
})

return recipe

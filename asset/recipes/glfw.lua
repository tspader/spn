local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'glfw/glfw',
  lib = 'glfw',
  kinds = { 'shared', 'static' },
  build = function(builder)
    local shared = builder.kind == 'shared'
    builder:cmake({
      defines = {
        { 'BUILD_SHARED_LIBS', shared },
        { 'GLFW_BUILD_EXAMPLES', false },
        { 'GLFW_BUILD_TESTS', false },
        { 'GLFW_BUILD_DOCS', false },
        { 'GLFW_BUILD_WAYLAND', false },
      },
      install = true,
    })

    builder:copy({
      { builder:lib('libglfw3.a'), builder:lib('libglfw.a') }
    })

  end,
})

return recipe

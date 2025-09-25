local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'freetype/freetype',
  libs = { 'freetype' },
  kinds = { 'shared', 'static' },
  build = function(builder)
    local shared = builder.kind == 'shared'
    builder:cmake({
      defines = {
        { 'BUILD_SHARED_LIBS', shared },
      },
      install = true,
    })

    builder:copy({
      { builder:store('include/freetype2/ft2build.h'), builder:include() },
      { builder:store('include/freetype2/freetype'), builder:include() },
    })
  end,
})

return recipe

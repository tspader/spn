local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'freetype/freetype',
  libs = { 'freetype' },
  kinds = { 'shared', 'static' },
  build = function(builder)
    builder:cmake({
      defines = {
        { 'DISABLE_FORCE_DEBUG_POSTFIX', true, }
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

local spn = require('spn')

local config = spn.recipes.basic({
  git = 'mity/md4c',
  kinds = { 'shared', 'static' },
  lib = 'md4c',
  build = function(builder)
    builder:cmake({
      defines = {
        { 'BUILD_SHARED_LIBS', true },
      }
    })

    builder:cmake({
      defines = {
        { 'BUILD_SHARED_LIBS', false },
      }
    })

    builder:copy({
      { builder:source('src/md4c.*'), builder:include() },
      { builder:work('src/libmd4c*'), builder:lib() },
    })
  end
})
return config

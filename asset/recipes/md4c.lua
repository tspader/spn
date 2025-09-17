local spn = require('spn')

local config = spn.recipes.basic({
  git = 'mity/md4c',
  lib = 'md4c',
  copy = {
    [spn.dir.include] = {
      [spn.dir.source] = {
        'src/md4c.h',
        'src/md4c.c',
      },
    },
    [spn.dir.lib] = {
      [spn.dir.work] = {
        'src/libmd4c.so',
        'src/libmd4c.a',
        'src/libmd4c.so.0'
      }
    }
  },
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
  end
})
return config

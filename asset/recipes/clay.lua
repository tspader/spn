local spn = require('spn')

local config = spn.recipes.basic({
  git = 'nicbarker/clay',
  copy = {
    [spn.dir.include] = {
      [spn.dir.source] = {
        'clay.h',
        'examples',
        'renderers',
      },
    }
  }
})
return config

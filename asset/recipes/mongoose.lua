local spn = require('spn')

local config = spn.recipes.basic({
  git = 'cesanta/mongoose',
  copy = {
    [spn.dir.include] = {
      [spn.dir.source] = {
        'mongoose.h',
        'mongoose.c',
      },
    },
  },
})
return config

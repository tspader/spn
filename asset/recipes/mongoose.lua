local spn = require('spn')

local config = spn.recipes.basic({
  git = 'cesanta/mongoose',
  build = function (builder)
    builder:copy({
      { builder:source('mongoose.h'), builder:include() },
      { builder:source('mongoose.c'), builder:include() },
    })
  end
})
return config

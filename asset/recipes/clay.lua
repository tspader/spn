local spn = require('spn')

local config = spn.recipes.basic({
  git = 'nicbarker/clay',
  kinds = {
    'source'
  },
  build = function(builder)
    builder:copy({
      { builder:source('clay.h'), builder:include() },
      { builder:source('examples'), builder:include() },
      { builder:source('renderers'), builder:include() },
    })
  end
})
return config

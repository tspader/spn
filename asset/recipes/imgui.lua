local spn = require('spn')

local config = spn.recipes.basic({
  git = 'ocornut/imgui',
  kinds = {
    'source',
    'shared',
  },
  include = {
    vendor = true
  },
  build = function(builder)
    builder:copy({
      { builder:source('*.h'), builder:include() },
      { builder:source('*.cpp'), builder:vendor() },
      { builder:source('backends/*.h'), builder:include() },
      { builder:source('backends/*.cpp'), builder:vendor() },
      { builder:source('misc'), builder:include() },
      { builder:source('examples'), builder:vendor() },
    })
  end
})
return config

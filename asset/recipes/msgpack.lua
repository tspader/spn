local spn = require('spn')
local dbg = require('debugger')

local config = spn.recipes.basic({
  git = 'msgpack/msgpack-c',
  branch = 'c_master',
  lib = 'msgpack-c',
  build = function(builder)
    builder:cmake({
      install = true
    })

    builder:copy({
      { builder:source('example'), builder:include() },
    });
  end

})
return config

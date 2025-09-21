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
      { builder:source('example/*.c'), builder:include('example') }
    });

  --   builder:copy({
  --     source = {
  --       ['include/msgpack.h'] = builder.path.include(),
  --       ['include/msgpack/*'] = builder.path.include('msgpack'),
  --       ['example'] = builder.path.include(),
  --     },
  --     work = {
  --       ['include/msgpack/*'] = builder.path.include('msgpack'),
  --       ['libmsgpack-c.so'] = builder.path.lib(),
  --       ['libmsgpack-c.a'] = builder.path.lib()
  --     }
    -- })
  end

})
return config

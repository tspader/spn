local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'jedisct1/libsodium',
  lib = 'sodium',
  kinds = { 'shared', 'static' },
  build = function(builder)
    local shared = builder.kind == 'shared'
    local autogen = builder:source('autogen.sh')
    builder:sh({
      command = autogen.absolute,
      args = { '-s' },
      directory = builder.paths.source,
    })

    local configure = builder:source('configure')
    builder:sh({
      command = configure.absolute,
      args = {
        string.format('--prefix=%s', builder.paths.store),
        shared and '--enable-shared' or '--disable-shared',
        shared and '--disable-static' or '--enable-static',
      },
      directory = builder.paths.work,
    })

    builder:make({ jobs = 8 })
    builder:make({ target = 'install' })
  end,
})

return recipe

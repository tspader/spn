local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'richgel999/miniz',
  libs = { 'miniz' },
  kinds = { 'source' },
  include = {
    vendor = true,
  },
  build = function(builder)
    builder:copy({
      { builder:source('miniz.h'), builder:include() },
      { builder:source('miniz_common.h'), builder:include() },
      { builder:source('miniz_tdef.h'), builder:include() },
      { builder:source('miniz_tinfl.h'), builder:include() },
      { builder:source('miniz_zip.h'), builder:include() },
      { builder:source('miniz.c'), builder:vendor() },
      { builder:source('miniz_tdef.c'), builder:vendor() },
      { builder:source('miniz_tinfl.c'), builder:vendor() },
    })

    local export_header = builder:include('miniz_export.h')
    local file = assert(io.open(export_header.absolute, 'w'))
    file:write('#ifndef MINIZ_EXPORT_H\n')
    file:write('#define MINIZ_EXPORT_H\n')
    file:write('#define MINIZ_EXPORT\n')
    file:write('#define MINIZ_NO_EXPORT\n')
    file:write('#endif\n')
    file:close()
  end,
})

return recipe

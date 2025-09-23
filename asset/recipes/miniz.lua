local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'richgel999/miniz',
  lib = 'miniz',
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
    builder:sh({
      command = 'sh',
      args = {
        '-c',
        string.format([[cat <<'EOF' > %s
#ifndef MINIZ_EXPORT_H
#define MINIZ_EXPORT_H
#define MINIZ_EXPORT
#define MINIZ_NO_EXPORT
#endif
EOF
]], export_header.absolute)
      }
    })
  end,
})

return recipe

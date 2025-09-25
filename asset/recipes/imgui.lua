local spn = require('spn')

local config = spn.recipes.basic({
  git = 'ocornut/imgui',
  libs = { 'imgui' },
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
      { builder:source('backends/*.h'), builder:include() },
      { builder:source('misc'), builder:include() },
      { builder:source('examples'), builder:vendor() },
    })

    if builder.kind == 'source' then
      builder:copy({
        { builder:source('*.cpp'), builder:vendor() },
        { builder:source('backends/*.cpp'), builder:vendor() },
      })
    elseif builder.kind == 'shared' then
      builder:cc({
        compiler = 'g++',
        shared = true,
        source = {
          'imgui.cpp',
          'imgui_demo.cpp',
          'imgui_draw.cpp',
          'imgui_tables.cpp',
          'imgui_widgets.cpp',
        },
        output = 'libimgui.so',
      })

      builder:copy({
        { builder:work('libimgui.so'), builder:lib() },
      })
    end
  end
})
return config

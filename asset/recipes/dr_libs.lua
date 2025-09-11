local spn = require('spn')

local config = spn.recipes.basic({
  git = 'mackron/dr_libs',
  copy = {
    [spn.dir.include] = {
      [spn.dir.source] = {
        'dr_flac.h',
        'dr_mp3.h',
        'dr_wav.h',
      },
    }
  }
})
return config

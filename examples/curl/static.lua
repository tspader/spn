local project = {
  name = 'curl',
  deps = {
    sp = {},
    curl = {
      kind = 'static'
    },
  },
  system_deps = {
    'z',
    'ssl',
    'crypto',
  }
}

return project

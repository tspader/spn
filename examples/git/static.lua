local project = {
  name = 'git-static',
  deps = {
    sp = {},
    git = {
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

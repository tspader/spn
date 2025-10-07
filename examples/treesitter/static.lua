local project = {
  name = 'treesitter-static',
  system_deps = {
    'm'
  },
  deps = {
    treesitter = {
      kind = 'static'
    },
  },
}

return project

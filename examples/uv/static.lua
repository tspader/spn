local project = {
  name = 'uv-static',
  system_deps = {
    'm'
  },
  deps = {
    uv = {
      kind = 'static'
    },
  },
}

return project

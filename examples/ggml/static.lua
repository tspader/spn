local project = {
  name = 'ggml-static',
  deps = {
    ggml = {
      kind = 'static',
      include = {
        vendor = true
      },
      options = {
        backends = {
          cpu = true,
        }
      }
    },
  },
  system_deps = {
    'gomp'
  }
}

return project

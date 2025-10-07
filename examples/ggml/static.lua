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
          cuda = true,
          cpu = true,
          metal = true,
        }
      }
    },
  },
}

return project

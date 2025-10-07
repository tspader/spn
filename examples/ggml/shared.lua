local project = {
  name = 'ggml',
  deps = {
    ggml = {
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

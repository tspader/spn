local project = {
  name = 'ggml',
  deps = {
    ggml = {
      include = {
        vendor = true
      },
      options = {
        backends = {
          cpu = true,
          -- GPU backends, of course, also work
          cuda = false,
          metal = false,
        }
      }
    },
  },
}

return project

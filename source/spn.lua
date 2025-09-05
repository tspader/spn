local ffi = require('ffi')
local c = require('c')
local sp = c.sp
local sdl = c.sdl

local sp_lua_stack_t = require('stack')

---@class spn_lua_dep_t
---@field name string
---@field options table

---@class spn_lua_dep_paths_t
---@field recipe string
---@field source string
---@field build string
---@field store string
---@field bin string
---@field include string
---@field vendor string

---@class spn_lua_dep_build_t
---@field dep spn_lua_dep_t
---@field paths spn_lua_dep_paths_t

local spn = {
  recipes = {},
  lock = {},
  project = {
    name = '',
    deps = {},
  },

  iterator = require('iterator'),
}

----------
-- INIT --
----------
function spn.init(app)
  c.load()

  app = ffi.cast('spn_lua_context_t*', app)
  spn.app = app

  local config = dofile(app.paths.user_config:cstr())
  if not config then
    return
  end

  if config.spn then
    app.config.paths.spn = sp.str.from_cstr(config.spn)
  end

  if config.pull_recipes then
    app.config.pull_recipes = config.pull_recipes
  end

  if config.pull_deps then
    app.config.pull_deps = config.pull_deps
  end

  local entries = sp.os.scan_directory(app.paths.recipes)
  for index = 0, entries.count do
    local entry = entries.data[index]
    local extension = sp.os.extract_extension(entry.file_name)
    if sp.str.equal_cstr(extension, "lua") then
      local recipe = dofile(entry.file_path:cstr())

      local dep = ffi.new('spn_dep_info_t')
      dep.name = sp.os.extract_stem(entry.file_name)
      dep.git = sp.str.from_cstr(recipe.git)
      dep.lib = sp.str.from_cstr(recipe.lib)
      dep.paths.source = sp.os.join_path(app.paths.source, dep.name)
      dep.paths.recipe = sp.str.copy(entry.file_path)

      app.deps[0] = sp.dyn_array.push(app.deps[0], dep)

      local name = entry.file_path:cstr()
      spn.recipes[name] = dofile(name)
    end
  end

  spn.project = dofile(app.paths.project.config:cstr())
  --spn.lock = dofile(app.paths.project.lock:cstr())
  --name, url commit, build ID on spn_lock_entry_t goes to lock file
  --grab current commit from lockfile OR origin/head spn_git_get_commit(source, SPN_GIT_ORIGIN_HEAD)
  app.project.name = sp.str.from_cstr(spn.project.name)

  for name, spec in pairs(spn.project.deps) do
    ---@cast spec spn_lua_dep_t

    local values = {}
    table.insert(values, name)

    local stack = sp_lua_stack_t:new()
    stack:push(spec.options)

    while not stack:is_empty() do
      local t = stack:pop()

      for key, value in pairs(t) do
        if type(value) == 'table' then
          stack:push(value)
        else
          table.insert(values, key)
          table.insert(values, value)
        end
      end
    end

    local hashes = ffi.new('sp_hash_t* [1]')
    local hash = ffi.new('sp_hash_t [1]')
    for value in spn.iterator.values(values) do
      hash[0] = ffi.C.sp_hash_str(sp.str.from_cstr(tostring(value)))
      hashes[0] = sp.dyn_array.push(hashes[0], hash)
    end

    local dep = ffi.new('spn_dep_spec_t')
    dep.info = ffi.C.spn_dep_find(sp.str.from_cstr(name))
    dep.hash = ffi.C.sp_hash_combine(hashes[0], #values)
    if sp.os.does_path_exist(dep.info.paths.source) then
      dep.commit = ffi.C.spn_git_get_commit(dep.info.paths.source, sp.str.from_cstr("origin/HEAD"))
    end

    app.project.deps = sp.dyn_array.push(app.project.deps, dep)
  end


end

function spn.build(dep)
  c.load()
  dep = ffi.cast('spn_dep_build_context_t*', dep)

  local build = {
    dep = dep.info.name:cstr(),
    paths = {
      recipe = dep.info.paths.recipe:cstr(),
      source = dep.info.paths.source:cstr(),
      build = dep.paths.build:cstr(),
      store = dep.paths.store:cstr(),
      bin = dep.paths.bin:cstr(),
      include = dep.paths.include:cstr(),
      vendor = dep.paths.vendor:cstr(),
    }
  }

  local recipe = dofile(build.paths.recipe)
  recipe.build(build)

  for file in spn.iterator.values(recipe.files.include) do
    local source = sp.os.join_path(dep.info.paths.source, sp.str.from_cstr(file))
    local store = sp.os.join_path(dep.paths.include, sp.str.from_cstr(file))
    print(string.format(
      '%s -> %s',
      source:cstr(),
      store:cstr()
    ))
    sdl.CopyFile(source:cstr(), store:cstr())
  end
end

local module = {}

module.recipes = {
  default = function()
    return {
      files = {
        include = {},
        bin = {},
        vendor = {}
      },
      lib = '',
      build = function(context)
        print('building')
      end
    }
  end,
  single_header = function(config)
    local recipe = module.recipes.default()
    recipe.git = config.git
    recipe.lib = config.lib or ''
    table.insert(recipe.files.include, config.header)
    return recipe
  end
}

module.internal = spn

return module

local inspect = require('inspect')
local dbg = require('debugger')
local ffi = require('ffi')
local c = require('c')
local sp = c.sp
local serpent = require('serpent')
local sp_lua_stack_t = require('stack')
local spn_lua_dep_builder_t = require('build')

-------------
-- PROJECT --
-------------
---@class spn_lua_dep_include_t
---@field include boolean
---@field vendor boolean
---@field store boolean

---@class spn_lua_dep_t
---@field kind? string
---@field include? spn_lua_dep_include_t
---@field options table


-----------
-- BUILD --
-----------
---@class spn_lua_dep_paths_t
---@field recipe string
---@field source string
---@field work string
---@field store string
---@field lib string
---@field include string
---@field vendor string

---@class spn_lua_dep_build_t
---@field dep spn_lua_dep_t
---@field paths spn_lua_dep_paths_t

---@class spn_lua_cc_config_t
---@field compiler string
---@field output string
---@field shared string|nil
---@field source string[]|nil
---@field include string[]|nil

---@class spn_lua_make_config_t
---@field makefile string|nil
---@field target string|nil
---@field targets string[]|nil
---@field directory string|nil
---@field jobs number|string|nil
---@field variables table<string, string>|nil

---@class spn_lua_cmake_config_t
---@field defines string[]|nil
---@field install boolean|nil
---@field parallel boolean|nil

---@class spn_lua_sh_config_t
---@field command string
---@field args string[]|nil
---@field directory string|nil

------------
-- RECIPE --
------------
---@class spn_lua_recipe_t
---@field git string
---@field libs string[]
---@field kinds string[]
---@field include spn_lua_dep_include_t
---@field branch string
---@field build fun(spn_lua_dep_build_t): nil

---@class spn_lua_recipe_config_t
---@field git string
---@field kinds? string[]
---@field include? spn_lua_dep_include_t
---@field libs? string[]
---@field branch? string
---@field build fun(spn_lua_dep_build_t): nil | nil

---@class spn_lua_single_header_config_t : spn_lua_recipe_config_t
---@field header string


------------
-- MODULE --
------------
local spn = {
  recipes = {},
  lock_file = {},
  project = {
    name = '',
    deps = {},
  },

  iterator = require('iterator'),
  dir = {
    source = 'source',
    work = 'work',
    include = 'include',
    lib = 'lib',
    vendor = 'vendor',
  },
  join_path = function(la, lb)
    return sp.os.join_path(sp.str.from_cstr(la), sp.str.from_cstr(lb)):cstr()
  end
}


----------
-- INIT --
----------
function spn.init(app)
  c.load()

  -- Load the project agnostic user config
  app = ffi.cast('spn_lua_context_t*', app)
  spn.app = app
  local config = {}
  local loader = loadfile(app.paths.user_config:cstr())
  if loader then
    local ok, result = pcall(loader)
    if not ok then
      error(string.format('failed to evaluate user config %s: %s', app.paths.user_config:cstr(), result))
    end
    if type(result) == 'table' then
      config = result
    end
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

  -- Read all recipes
  local entries = sp.os.scan_directory(app.paths.recipes)
  for index = 0, entries.count - 1 do
    local entry = entries.data[index]
    local extension = sp.os.extract_extension(entry.file_name)
    if sp.str.equal_cstr(extension, "lua") then
      local recipe = dofile(entry.file_path:cstr())

      local dep = ffi.new('spn_dep_info_t')
      dep.name = sp.os.extract_stem(entry.file_name)
      dep.git = sp.str.from_cstr(recipe.git)
      dep.branch = sp.str.from_cstr(recipe.branch)
      dep.paths.source = sp.os.join_path(app.paths.source, dep.name)
      dep.paths.recipe = sp.str.copy(entry.file_path)

      for lib in spn.iterator.values(recipe.libs) do
        dep.libs = sp.dyn_array.push(dep.libs, sp.str.from_cstr(lib))
      end

      app.deps[0] = sp.dyn_array.push(app.deps[0], dep)

      local name = dep.name:cstr()
      spn.recipes[name] = dofile(entry.file_path:cstr())
    end
  end

  -- Read this project
  spn.project = dofile(app.paths.project.config:cstr())

  spn.lock_file = {
    deps = {}
  }

  local chunk, err = loadfile(app.paths.project.lock:cstr())
  if chunk then
    local ok, lock_file = pcall(chunk)
    spn.lock_file = lock_file
  end

  app.project.name = sp.str.from_cstr(spn.project.name)
  if spn.project.system_deps then
    for dep_name in spn.iterator.values(spn.project.system_deps) do
      app.project.system_deps = sp.dyn_array.push(app.project.system_deps, sp.str.from_cstr(dep_name))
    end
  end

  -- Read each dep used by the project
  for name, spec in pairs(spn.project.deps) do
    ---@cast spec spn_lua_dep_t

    -- Copy what we need from the table into a struct
    local recipe = spn.recipes[name]

    local dep = ffi.new('spn_dep_spec_t')
    dep.info = ffi.C.spn_dep_find(sp.str.from_cstr(name))

    local lock = spn.lock_file.deps[name]
    if lock then
      dep.lock = sp.str.from_cstr(lock.commit)
    end

    local kind = spec.kind or recipe.kinds[1]
    dep.kind = c.spn.dep.build_kind_from_str(sp.str.from_cstr(kind))

    local include = {}
    for key, value in spn.iterator.pairs(recipe.include) do
      include[key] = value
    end
    if spec.include then
      for key, value in spn.iterator.pairs(spec.include) do
        include[key] = value
      end
    end

    dep.include.include = include.include
    dep.include.vendor = include.vendor
    dep.include.store = include.store

    -- Hash name and everything in the options table
    local values = {}
    table.insert(values, name)
    table.insert(values, kind)

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
      local hashable = tostring(value)
      hash[0] = ffi.C.sp_hash_str(sp.str.from_cstr(hashable))
      hashes[0] = sp.dyn_array.push(hashes[0], hash)
    end

    dep.hash = ffi.C.sp_hash_combine(hashes[0], #values)

    -- Push it to an array in C
    app.project.deps = sp.dyn_array.push(app.project.deps, dep)
  end
end

function spn.build(dep)
  -- Load the FFI again; each dep gets its own Lua context
  c.load()

  local builder = spn_lua_dep_builder_t.new(ffi.cast('spn_dep_build_context_t*', dep))
  builder:build()
end

function spn.lock()
  local context = spn.app
  local lock_array = context.lock[0]

  local num_deps = 0
  for _ in pairs(spn.project.deps) do
    num_deps = num_deps + 1
  end

  local lockfile = {
    deps = {}
  }

  for i = 0, num_deps - 1 do
    local entry = lock_array[i]

    local name = ffi.string(entry.name.data, entry.name.len)
    local commit = ffi.string(entry.commit.data, entry.commit.len)
    local build_id = ffi.string(entry.build_id.data, entry.build_id.len)

    lockfile.deps[name] = {
      name = name,
      commit = commit,
      build_id = build_id
    }
  end

  local serialized = serpent.block(lockfile, {comment = false, sortkeys = true})

  local path = context.paths.project.lock:cstr()
  local file = io.open(path, 'w')
  if not file then
    return false
  end

  file:write("return " .. serialized)
  file:close()

  return true
end

function spn.copy(to)
  to = sp.str.from_cstr(to)

  for dep in spn.iterator.values(spn.project.deps) do
    local entries = sp.os.scan_directory(sp.str.from_cstr(dep.paths.lib))

    for j = 0, entries.count - 1 do
      local entry = entries.data[j]
      sp.os.copy_file(
        entry.file_path,
        sp.os.join_path(sp.str.from_cstr(to), sp.os.extract_file_name(entry.file_path))
      )
    end
  end
end

function spn.ternary(value, default_value)
  if value == nil then return default_value end
  return value
end

---@param config spn_lua_recipe_config_t
---@return spn_lua_recipe_t
local basic = function(config)
  local recipe = {
    git = '',
    libs = {},
    kinds = { 'shared', 'static', 'source' },
    include = {
      include = true,
      vendor = false,
      store = false
    },
    branch = 'HEAD',
    build = function() end
  }

  recipe.git = config.git
  recipe.libs = config.libs or recipe.libs
  recipe.kinds = config.kinds or recipe.kinds
  recipe.branch = config.branch or recipe.branch
  recipe.build = config.build or recipe.build
  if config.include then
    recipe.include = {
      include = spn.ternary(config.include.include, recipe.include.include),
      vendor = spn.ternary(config.include.vendor, recipe.include.vendor),
      store = spn.ternary(config.include.store, recipe.include.store),
    }
  end
  return recipe
end


---@param config spn_lua_single_header_config_t
---@return spn_lua_recipe_t
local single_header = function(config)
  local recipe = basic(config)
  recipe.kinds = { 'source' }
  recipe.build = function(builder)
    builder:copy({
      { builder:source(config.header), builder:include() }
    })
  end
  return recipe
end

local module = {
  recipes = {
    single_header = single_header,
    basic = basic,
  },
  dir = spn.dir,
  join_path = spn.join_path,
  iterator = spn.iterator,
  build_kind = {
    shared = 'shared',
    static = 'static',
    source = 'source'
  },
  internal = spn
}

return module

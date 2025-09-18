local inspect = require('inspect')
local dbg = require('debugger')
local ffi = require('ffi')
local c = require('c')
local sp = c.sp
local serpent = require('serpent')
local sp_lua_stack_t = require('stack')
local spn_lua_dep_builder_t = require('build')

---@class spn_lua_dep_t
---@field name string
---@field options table

---@class spn_lua_dep_paths_t
---@field recipe string
---@field source string
---@field build string
---@field store string
---@field lib string
---@field include string
---@field vendor string

---@class spn_lua_dep_build_t
---@field dep spn_lua_dep_t
---@field paths spn_lua_dep_paths_t

---@class spn_lua_make_config_t
---@field makefile string|nil
---@field target string|nil

---@class spn_lua_cmake_config_t
---@field defines string[]|nil
---@field parallel boolean|nil

---@class spn_lua_sh_config_t
---@field command string
---@field args string[]|nil

local spn = {
  recipes = {},
  lock = {},
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
  -- This function is called from the dependency's build thread. Lua is not thread-safe, which means that
  -- each thread must have its own interpreter context, and that we must load the FFI again. This only
  -- has to be done in the thread's entry point.
  c.load()

  local builder = spn_lua_dep_builder_t:new(ffi.cast('spn_dep_build_context_t*', dep))
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

    lockfile.deps[i + 1] = {
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

  print(inspect(spn.project))
  for dep in spn.iterator.values(spn.project.deps) do
    local entries = sp.os.scan_directory(sp.str.from_cstr(dep.paths.lib))
    for j = 0, entries.count - 1 do
      local entry = entries.data[j]
      print(entry.file_path:cstr())
      sp.os.copy_file(
        entry.file_path,
        sp.os.join_path(sp.str.from_cstr(to), sp.os.extract_file_name(entry.file_path))
      )
    end
  end
end



---@class spn_lua_copy_entries_t
---@field include string[]
---@field lib string[]
---@field vendor string[]

---@class spn_lua_recipe_t
---@field git string
---@field lib string
---@field copy spn_lua_copy_entries_t
---@field build fun(spn_lua_dep_build_t): nil


---@class spn_lua_copy_entries_config_t
---@field include? string[]
---@field lib? string[]
---@field vendor? string[]

---@class spn_lua_recipe_config_t
---@field git string
---@field lib? string
---@field copy spn_lua_copy_entries_t
---@field build fun(spn_lua_dep_build_t): nil | nil



---@class spn_lua_single_header_config_t : spn_lua_recipe_config_t
---@field header string

---@return spn_lua_recipe_t
local recipe_template = function()
  local recipe = {
    git = '',
    lib = '',
    copy = {
      [spn.dir.include] = {
        [spn.dir.work] = {},
        [spn.dir.source] = {}
      },
      [spn.dir.lib] = {
        [spn.dir.work] = {},
        [spn.dir.source] = {}
      },
    },
    build = function(context)  end
  }

  return recipe
end

---@param config spn_lua_recipe_config_t
---@return spn_lua_recipe_t
local basic = function(config)
  local recipe = recipe_template()
  recipe.git = config.git
  recipe.lib = config.lib or recipe.lib
  recipe.copy = config.copy or recipe.copy
  recipe.build = config.build or recipe.build
  return recipe
end


---@param config spn_lua_single_header_config_t
---@return spn_lua_recipe_t
local single_header = function(config)
  local recipe = basic(config)
  table.insert(recipe.copy[spn.dir.include][spn.dir.source], config.header)
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

  internal = spn
}

return module

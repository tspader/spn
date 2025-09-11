local ffi = require('ffi')
local iterator = require('iterator')
local dbg = require('debugger')
local c = require('c')
local sp = c.sp

local module = {}
module.__index = module

function module:new(dep)
  local self = setmetatable({}, module)
  self.dep = dep
  self.name = dep.info.name:cstr()
  self.paths = {
    recipe = dep.info.paths.recipe:cstr(),
    source = dep.paths.source:cstr(),
    work = dep.paths.work:cstr(),
    store = dep.paths.store:cstr(),
    lib = dep.paths.lib:cstr(),
    include = dep.paths.include:cstr(),
    vendor = dep.paths.vendor:cstr(),
  }
  return self
end

---@param config spn_lua_sh_config_t
function module:sh(config)
  local context = ffi.new('spn_sh_process_context_t')
  context.command = sp.str.from_cstr(config.command)
  context.work = self.dep.paths.work
  context.shell = self.dep.sh

  if config.args then
    local args_array = ffi.new('sp_str_t* [1]')
    for arg in iterator.values(config.args) do
      local str_arg = sp.str.from_cstr(arg)
      args_array[0] = sp.dyn_array.push(args_array[0], str_arg)
    end

    context.args = args_array[0]
  end

  ffi.C.spn_sh_run(context)

  if context.result.return_code == 0 then
    return
  end

  print('something fucked up!')
  dbg()
end

---@param config spn_lua_make_config_t
function module:make(config)
  config = config or {}

  local sh = {
    command = 'make',
    args = {
      '--quiet',
      '--directory', self.paths.work
    }
  }

  if config.makefile then
    table.insert(sh.args, '--makefile')
    table.insert(sh.args, config.makefile)
  end

  if config.target then
    table.insert(sh.args, config.target)
  end

  self:sh(sh)
end

---@param config spn_lua_cmake_config_t
function module:cmake(config)
  config = config or {
    defines = {}
  }

  local sh = {
    command = 'cmake',
    args = {
      '-S', self.paths.source,
      '-B', self.paths.work,
    }
  }

  for define in iterator.values(config.defines) do
    local name = define[1]
    local value = define[2] and 'ON' or 'OFF'
    table.insert(sh.args, string.format('-D%s=%s', name, value))
  end

  self:sh({
    command = 'sh',
    args = {
      '-c',
      'cmake ' .. table.concat(sh.args, ' ')
    }
  })

  self:sh({
    command = 'sh',
    args = {
      '-c',
      'cmake --build ' .. self.paths.work .. ' --parallel'
    }
  })
end


function module:build()
  self.recipe = dofile(self.paths.recipe)

  if self.recipe.build then
    self.recipe.build(self)
  end

  if self.recipe.copy then
    for to_id, entries in iterator.pairs(self.recipe.copy) do
      local store = sp.str.from_cstr(self.paths[to_id])

      for from_id, files in iterator.pairs(entries) do
        local source = sp.str.from_cstr(self.paths[from_id])

        for file_path in iterator.values(files) do
          local from = sp.os.join_path(source, sp.str.from_cstr(file_path))

          local target_name = sp.os.extract_file_name(sp.str.from_cstr(file_path))
          local to = sp.os.join_path(store, target_name)

          if sp.os.is_directory(from) then
            sp.os.copy_directory(from, to)
          elseif sp.os.is_regular_file(from) then
            sp.os.copy_file(from, to)
          end
        end
      end
    end
  end

end

return module

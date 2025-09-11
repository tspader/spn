---@class sp_lua_stack_t
local sp_lua_stack_t = {}
sp_lua_stack_t.__index = sp_lua_stack_t

---@param v any
---@return string
local hash_anything = function(v)
	if type(v) == 'table' then
	  if not v then return '0x00000000' end

    local str = tostring(v)
    local parts = {}
    for match in string.gmatch(str, ' ') do
      table.insert(parts, match)
    end

    local address = parts[2]

	  return address
  else
    return tostring(v)
	end
end

function sp_lua_stack_t:new()
  local instance = setmetatable({}, sp_lua_stack_t)
  instance:init()
  return instance
end

function sp_lua_stack_t:init()
  self.stack = {}
  self.visited = {}
end

function sp_lua_stack_t:peek(n)
  if n then
    if n < #self.stack then
      return self.stack[#self.stack - n]
    end
  else
    return self.stack[#self.stack]
  end
end

function sp_lua_stack_t:pop()
  local out = self:peek()
  self.stack[#self.stack] = nil
  return out
end

function sp_lua_stack_t:push(item)
  table.insert(self.stack, item)

  local hash = hash_anything(item)
  self.visited[hash] = true
end

function sp_lua_stack_t:size()
  return #self.stack
end

function sp_lua_stack_t:clear()
  self.stack = {}
end

function sp_lua_stack_t:push_unique(item)
  if item == nil then return end
  if self:is_visited(item) then return end

  self:push(item)
end

---@param item any
---@return boolean
function sp_lua_stack_t:is_visited(item)
  local hash = hash_anything(item)
  return self.visited[hash]
end

function sp_lua_stack_t:is_empty()
  return #self.stack == 0
end


return sp_lua_stack_t

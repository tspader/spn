local module = {}
module.__index = module

function module.ternary(value, default_value)
  if value == nil then return default_value end
  return value
end

function module.clone(t)
  if not t then return nil end

  for k, v in pairs(t) do
    if type(v) == 'table' then
      t[k] = module.clone(v)
    else
      t[k] = v
    end
  end

  return t
end

function module.merge(base, apply)
  local result = module.clone(base)

  assert(result)
  if type(apply) ~= 'table' then return base end

  for k, v in pairs(apply) do
    if type(v) == 'table' then
      result[k] = result[k] or {}
      assert(type(result[k]) == 'table')

      result[k] = module.merge(result[k], v)
    else
      result[k] = v
    end
  end

  return result
end

return module

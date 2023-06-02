local utils = {}
usecolors = true

function utils.split (inputstr, sep)
  if sep == nil then
    sep = "%s"
  end

  local t={}
  for str in string.gmatch(inputstr, "([^"..sep.."]+)") do
    table.insert(t, str)
  end

  return t
end

local colors = {
  black   = "0;30",
  red     = "0;31",
  green   = "0;32",
  yellow  = "0;33",
  blue    = "0;34",
  magenta = "0;35",
  cyan    = "0;36",
  white   = "0;37",
  B        = "1;",
  Bblack   = "1;30",
  Bred     = "1;31",
  Bgreen   = "1;32",
  Byellow  = "1;33",
  Bblue    = "1;34",
  Bmagenta = "1;35",
  Bcyan    = "1;36",
  Bwhite   = "1;37"
}

function utils.color(color_name)
  if usecolors then
    return "\27[" .. (colors[color_name] or "0") .. "m"
  else
    return ""
  end
end

function utils.colorize(color_name, string, reset_name)
  return utils.color(color_name) .. tostring(string) .. utils.color(reset_name)
end

local backslash, null, newline, carriage, tab, quote, quote2, obracket, cbracket

function utils.loadColors(n)
  if n ~= nil then usecolors = n end
  backslash = utils.colorize("Bgreen", "\\\\", "green")
  null      = utils.colorize("Bgreen", "\\0", "green")
  newline   = utils.colorize("Bgreen", "\\n", "green")
  carriage  = utils.colorize("Bgreen", "\\r", "green")
  tab       = utils.colorize("Bgreen", "\\t", "green")
  quote     = utils.colorize("Bgreen", '"', "green")
  quote2    = utils.colorize("Bgreen", '"')
  obracket  = utils.colorize("B", '[')
  cbracket  = utils.colorize("B", ']')
end

utils.loadColors()

function utils.dump(o, depth)
  local t = type(o)
  if t == 'string' then
    return quote .. o:gsub("\\", backslash):gsub("%z", null):gsub("\n", newline):gsub("\r", carriage):gsub("\t", tab) .. quote2
  end
  if t == 'nil' then
    return utils.colorize("Bblack", "nil")
  end
  if t == 'boolean' then
    return utils.colorize("yellow", tostring(o))
  end
  if t == 'number' then
    return utils.colorize("blue", tostring(o))
  end
  if t == 'userdata' then
    return utils.colorize("magenta", tostring(o))
  end
  if t == 'thread' then
    return utils.colorize("Bred", tostring(o))
  end
  if t == 'function' then
    return utils.colorize("cyan", tostring(o))
  end
  if t == 'cdata' then
    return utils.colorize("Bmagenta", tostring(o))
  end
  if t == 'table' then
    if type(depth) == 'nil' then
      depth = 0
    end
    if depth > 1 then
      return utils.colorize("yellow", tostring(o))
    end
    local indent = ("  "):rep(depth)

    -- Check to see if this is an array
    local is_array = true
    local i = 1
    for k,v in pairs(o) do
      if not (k == i) then
        is_array = false
      end
      i = i + 1
    end

    local first = true
    local lines = {}
    i = 1
    local estimated = 0
    for k,v in (is_array and ipairs or pairs)(o) do
      local s
      if is_array then
        s = ""
      else
        if type(k) == "string" and k:find("^[%a_][%a%d_]*$") then
          s = k .. ' = '
        else
          s = '[' .. utils.dump(k, 100) .. '] = '
        end
      end
      s = s .. utils.dump(v, depth + 1)
      lines[i] = s
      estimated = estimated + #s
      i = i + 1
    end
    if estimated > 200 then
      return "{\n  " .. indent .. table.concat(lines, ",\n  " .. indent) .. "\n" .. indent .. "}"
    else
      return "{ " .. table.concat(lines, ", ") .. " }"
    end
  end
  -- This doesn't happen right?
  return tostring(o)
end

-- A nice global data dumper
function utils.prettyPrint(...)
  local n = select('#', ...)
  local arguments = { ... }

  for i = 1, n do
    arguments[i] = utils.dump(arguments[i])
  end

  print(table.concat(arguments, "\t") .. '\n')
end

function utils.profiler(func, count)
    if type(func) ~= "function" then
        return
    end

    if count == nil then
        count = 100
    end

    local total_time = 0
    for i = 1, count do
        local startTime = os.time()
        func()
        local endTime = os.time()
        total_time = total_time + os.difftime(endTime, startTime)
    end

    local average_time = total_time / count

    print("function run times=" .. count .. ", total time=" .. total_time .. ", average cost time=".. average_time)
end

return utils

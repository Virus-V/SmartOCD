-- Copyright (c) 2023, Virus.V <virusv@live.com>
-- All rights reserved.
--
-- Redistribution and use in source and binary forms, with or without
-- modification, are permitted provided that the following conditions are met:
--
-- * Redistributions of source code must retain the above copyright notice, this
--   list of conditions and the following disclaimer.
--
-- * Redistributions in binary form must reproduce the above copyright notice,
--   this list of conditions and the following disclaimer in the documentation
--   and/or other materials provided with the distribution.
--
-- * Neither the name of SmartOCD nor the names of its
--   contributors may be used to endorse or promote products derived from
--   this software without specific prior written permission.
--
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
-- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
-- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
-- DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
-- FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
-- DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
-- SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
-- CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
-- OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
-- OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
-- Copyright 2023 Virus.V <virusv@live.com>
--

local loop = require('Loop')
local tcp = require('Loop.tcp')
local dqueue = require('scripts.libs.dqueue')

function mysplit (inputstr, sep)
  if sep == nil then
    sep = "%s"
  end

  local t={}
  for str in string.gmatch(inputstr, "([^"..sep.."]+)") do
    table.insert(t, str)
  end

  return t
end

local utils = {}
local usecolors = true

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

local noAckMode = false

-- General query command
local function handleGeneralQuery(command)
  local cmdStart,cmdEnd = string.find(command, 'qSupported', 1, true)
  if cmdStart == 1 then
    local cmdBody = string.sub(command, cmdEnd + 2)
    print(cmdBody)
    local featureTable = mysplit(cmdBody, ';')
    utils.prettyPrint(featureTable)
    return string.format("PacketSize=%x;QStartNoAckMode+;qXfer:features:read+;type=SmartOCD", 512)
  end

  cmdStart,cmdEnd = string.find(command, 'QStartNoAckMode', 1, true)
  if cmdStart == 1 then
    noAckMode = true
    return "OK"
  end

  return "E01"
end

local function handleCommandv(command)
  if string.find(command, 'vMustReplyEmpty', 1, true) == 1 then
    return ""
  end
  return "E01"
end

local function handleGdbCommand(command)
  local prefix = string.sub(command, 1, 1)

  if prefix == 'q' or prefix == 'Q' then
    return handleGeneralQuery(command)
  end

  if prefix == 'v' then
    return handleCommandv(command)
  end
  return 'E01'
end

-- 创建命令重传队列
local respRetryList = dqueue.new()

local commandBuffer = ""
-- 处理GDB命令
local function receiveGdbCommand(client)
  local cmdStart = 0
  local cmdEnd = 0

  -- 处理命令是否正确接收
  while not noAckMode do
    local respStatus = string.sub(commandBuffer, 1, 1)
    if respStatus == '+' then
      -- 命令被正确接收，无需重试
      local retryCmd = respRetryList:pop_left()
    elseif respStatus == '-' then
      -- 命令接收失败，需要重传
      print(string.format("retry:%s ", retryCmd))
      client:Write(retryCmd)
      respRetryList:push_right(retryCmd)
    else
      break
    end

    -- 第一个字符处理完了，干掉
    -- 再来一遍
    commandBuffer = string.sub(commandBuffer, 2)
  end

  cmdStart = string.find(commandBuffer, '$', 1, true)
  if cmdStart == nil then
    return
  end

  cmdEnd = string.find(commandBuffer, '#', 1, true)
  if cmdEnd == nil then
    return
  end

  if string.len(commandBuffer) < cmdEnd + 2 then -- 命令后面跟两个字节checksum
    return
  end

  local currentCommand = string.sub(commandBuffer, cmdStart, cmdEnd + 2)
  commandBuffer = string.sub(commandBuffer, cmdEnd + 3)

  -- verfiy command checksum
  local cmdBody =  string.sub(currentCommand, 2, -4)
  local cmdCheckSum = string.sub(currentCommand, -2)
  print(string.format("cmdBody:%s, cmdCheckSum:%s", cmdBody, cmdCheckSum))

  -- 校验数据
  local checksum = 0
  for i=1,string.len(cmdBody),1 do
    checksum = checksum + string.byte(cmdBody, i)
  end
  checksum = checksum & 0xff

  if tonumber(cmdCheckSum, 16) ~= checksum then
    print(string.format("Command %s checksum failed. calculated checksum: %x", currentCommand, checksum))
    client:Write("-")
    return
  else
    client:Write('+')
  end

  -- 处理命令
  local respBody = handleGdbCommand(cmdBody)
  -- 计算checksum
  checksum = 0
  for i=1,string.len(respBody),1 do
    checksum = checksum + string.byte(respBody, i)
  end
  checksum = checksum & 0xff

  local respCommand = string.format("$%s#%02X", respBody, checksum)

  -- 将命令响应加到重传列表中
  if not noAckMode then
    respRetryList:push_right(respCommand)
  end

  client:Write(respCommand)
end

local function onConnect(server, err)
  assert(not err, err)
  local c = tcp.Create()
  server:Accept(c)

  -- 开始接收
  c:ReadStart(function(err, data)
    assert(not err, err)

    if data then
      -- 追加数据
      commandBuffer = commandBuffer .. data
      local ret, err = pcall(receiveGdbCommand, c)
      if ret ~= true then
        print("Handle GDB command failed:", err)
        c:Close()
        commandBuffer = ''
      end
    else
      print("client close")
      c:Close()
      commandBuffer = ''
    end

  end)

end

function GdbCreate(addr, port, handler)
  local s = tcp.Create()

  s:Bind(addr, port)
  s:Listen(128, onConnect)
  return {
    server = server
  }
end

GdbCreate("0.0.0.0", 1337)

loop.Run('default')
loop.Close()

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
local timer = require('Loop.timer')
local dqueue = require('libs.dqueue')
local utils = require('libs.utils')

-- 转义特殊字符
local function gdbRspEscape(input)
  local output = string.gsub(input, '}', '}]') -- escape '}'(0x7d) to '}]'(0x7d 0x5d)
  output = string.gsub(output, '#', '}\x03') -- escape '#' (0x23) to '}\x03' (0x7d 0x03)
  output = string.gsub(output, '$', '}\x04') -- escape '$' (0x23) to '}\x04' (0x7d 0x04)
  -- output = string.gsub(output, '*', '}\x0a') -- escape '$' (0x2a) to '}\x0a' (0x7d 0x0a)
  return output
end

local noAckMode = false
-- Target处于运行模式，GDB需要响应用户输入的Ctrl-C
local waitingUserSig = false

-- 创建命令重传队列
local respRetryList = dqueue.new()

local function reg2hexle(reg)
  return string.format("%02X%02X%02X%02X", reg & 0xff, (reg >> 8) & 0xff, (reg >> 16) & 0xff, (reg >> 24) & 0xff)
end

local function hexle2reg(reg)
  return string.format("%02X%02X%02X%02X", reg & 0xff, (reg >> 8) & 0xff, (reg >> 16) & 0xff, (reg >> 24) & 0xff)
end

-- General query command
local function handleGeneralQuery(client, command)
  local cmdStart,cmdEnd = string.find(command, 'qSupported', 1, true)
  if cmdStart == 1 then
    local cmdBody = string.sub(command, cmdEnd + 2)
    -- print(cmdBody)
    local featureTable = utils.split(cmdBody, ';')
    utils.prettyPrint(featureTable)
    -- TODO: 增加memory map的定义
    --ReplyGdbCommand(client, string.format("PacketSize=%x;QStartNoAckMode+;qXfer:features:read+;qXfer:memory-map:read+;type=SmartOCD", 512))
    ReplyGdbCommand(client, string.format("PacketSize=%x;QStartNoAckMode+;vContSupported+;qXfer:features:read+;type=SmartOCD", 512))
    return
  end

  cmdStart,cmdEnd = string.find(command, 'QStartNoAckMode', 1, true)
  if cmdStart == 1 then
    ReplyGdbCommand(client, 'OK')
    noAckMode = true
    print("gdb server enter noAckMode")
    return
  end

  cmdStart,cmdEnd = string.find(command, 'qXfer:features:read', 1, true)
  if cmdStart == 1 then
    local cmdBody = string.sub(command, cmdEnd + 2)
    -- print(cmdBody)
    local targetInfo = utils.split(cmdBody, ',')
    local offset = utils.split(targetInfo[1], ':')[2]
    -- utils.prettyPrint(targetInfo, offset)

    local targetXml = io.open('scripts/arch/riscv/target.xml', 'rb')
    assert(targetXml ~= nil)
    targetXml:seek("set", tonumber(offset, 16))
    local targetXmlData = targetXml:read(tonumber(targetInfo[2],16))
    if (string.len(targetXmlData) == tonumber(targetInfo[2],16)) then
      ReplyGdbCommand(client, 'm'..targetXmlData)
    else
      ReplyGdbCommand(client, 'l'..targetXmlData)
    end
    targetXml:close()
    return
  end

  -- trace state
  cmdStart,cmdEnd = string.find(command, 'qTStatus', 1, true)
  if cmdStart == 1 then
    ReplyGdbCommand(client, 'T0;tnotrun:0')
    return
  end

  cmdStart,cmdEnd = string.find(command, 'qAttached', 1, true)
  if cmdStart == 1 then
    ReplyGdbCommand(client, '1')
    return
  end

  cmdStart,cmdEnd = string.find(command, 'qC', 1, true)
  if cmdStart == 1 then
    ReplyGdbCommand(client, 'QC0')
    return
  end

  assert(cmdStart == nil and cmdEnd == nil)
  ReplyGdbCommand(client, '')
end

local function handleCommandv(client, command)
  local cmdStart,cmdEnd = string.find(command, 'vMustReplyEmpty', 1, true)
  if cmdStart == 1 then
    ReplyGdbCommand(client, '')
    return
  end

  cmdStart,cmdEnd = string.find(command, 'vCont?', 1, true)
  if cmdStart == 1 then
    ReplyGdbCommand(client, 'vCont;c;s')
    return
  end

  -- Cont;
  cmdStart,cmdEnd = string.find(command, 'vCont:', 1, true)
  if cmdStart == 1 then

    ReplyGdbCommand(client, 'T05')
    return
  end

  assert(cmdStart == nil and cmdEnd == nil)
  ReplyGdbCommand(client, '')
end

-- 处理读内存操作
local function handleCommandMemoryRead(client, command)
  local cmdBody = string.sub(command, 2)
  local addressInfo = utils.split(cmdBody, ',')
  utils.prettyPrint("read", addressInfo)
  addressInfo[1] = tonumber(addressInfo[1], 16)
  addressInfo[2] = tonumber(addressInfo[2], 16) + addressInfo[1]
  local resp = ''

  -- 备份s0、s1寄存器
  local s0 = dmObj:AccessGPR(8)
  local s1 = dmObj:AccessGPR(9)

  repeat
    local align = addressInfo[1] & 0x3
    local left = addressInfo[2] - addressInfo[1]

    local read_len
    if align == 0 then
      read_len = 4
    elseif align == 1 or align == 3 then
      read_len = 1
    else
      read_len = 2
    end

    -- 确定合适的访问宽度
    while read_len > left do
       read_len = read_len >> 1
    end
    assert(read_len, "Invalid read_len")

    -- print(string.format("address:%x, length: %d, next_addr:%x", addressInfo[1], read_len, addressInfo[1] + read_len))

    if read_len == 4 then
      local data = dmObj:AccessMemory(addressInfo[1], 4)
      resp = resp .. string.format("%02X%02X%02X%02X", data & 0xFF, (data >> 8) & 0xFF, (data >> 16) & 0xFF, (data >> 24) & 0xFF)
      addressInfo[1] = addressInfo[1] + 4
    elseif read_len == 1 then
      local data = dmObj:AccessMemory(addressInfo[1], 1)
      resp = resp .. string.format("%02X", data & 0xFF)
      addressInfo[1] = addressInfo[1] + 1
    elseif read_len == 2 then
      local data = dmObj:AccessMemory(addressInfo[1], 2)
      resp = resp .. string.format("%02X%02X", data & 0xFF, (data >> 8) & 0xFF)
      addressInfo[1] = addressInfo[1] + 2
    end
  until addressInfo[1] >= addressInfo[2]

  -- 恢复s0、s1寄存器
  dmObj:AccessGPR(8, s0)
  dmObj:AccessGPR(9, s1)

  ReplyGdbCommand(client, resp)
end

local function handleCommandMemoryWrite(client, command)
  local cmdBody = string.sub(command, 2)
  local writeInfo = utils.split(cmdBody, ':')
  local addressInfo = utils.split(writeInfo[1], ',')
  utils.prettyPrint("write", writeInfo, addressInfo)
  addressInfo[1] = tonumber(addressInfo[1], 16)
  addressInfo[2] = tonumber(addressInfo[2], 16) + addressInfo[1]

  -- 备份s0、s1寄存器
  local s0 = dmObj:AccessGPR(8)
  local s1 = dmObj:AccessGPR(9)

  repeat
    local align = addressInfo[1] & 0x3
    local left = addressInfo[2] - addressInfo[1]
    -- utils.prettyPrint(align, left)

    local read_len
    if align == 0 then
      read_len = 4
    elseif align == 1 or align == 3 then
      read_len = 1
    else
      read_len = 2
    end

    -- 确定合适的访问宽度
    while read_len > left do
       read_len = read_len >> 1
    end
    assert(read_len, "Invalid read_len")

    -- print(string.format("address:%x, length: %d, next_addr:%x", addressInfo[1], read_len, addressInfo[1] + read_len))

    if read_len == 4 then
      local data = string.sub(writeInfo[2], 1, 8)
      local b = tonumber(data, 16)
      assert(data ~= nil)
      b = (b >> 24) | ((b >> 8) & 0xff00) | ((b & 0xff00) << 8) | (b << 24)
      b = b & 0xffffffff;

      dmObj:AccessMemory(addressInfo[1], 4, b)

      addressInfo[1] = addressInfo[1] + 4
      writeInfo[2] = string.sub(writeInfo[2], 9)
    elseif read_len == 1 then
      local data = string.sub(writeInfo[2], 1, 2)
      local b = tonumber(data, 16)
      assert(b ~= nil)
      b = b & 0xff

      dmObj:AccessMemory(addressInfo[1], 1, b)

      addressInfo[1] = addressInfo[1] + 1
      writeInfo[2] = string.sub(writeInfo[2], 3)
    elseif read_len == 2 then
      local data = string.sub(writeInfo[2], 1, 4)
      local b = tonumber(data, 16)
      assert(b ~= nil)
      b = ((b >> 8) & 0xff) | ((b & 0xff) << 8)
      b = b & 0xffff

      dmObj:AccessMemory(addressInfo[1], 2, b)

      addressInfo[1] = addressInfo[1] + 2
      writeInfo[2] = string.sub(writeInfo[2], 5)
    end
  until addressInfo[1] >= addressInfo[2]

  -- 恢复s0、s1寄存器
  dmObj:AccessGPR(8, s0)
  dmObj:AccessGPR(9, s1)

  ReplyGdbCommand(client, 'OK')
end

local function handleCommandH(client, command)
  -- Hg0 接下来的g命令是针对哪个thread操作，这里不做处理
  ReplyGdbCommand(client, 'OK')
end

local poll_timer = timer.Create()

-- 处理GDB命令
local function handleGdbCommand(client, command)
  local prefix = string.sub(command, 1, 1)
  --print(string.format("handle command:%s", command))

  if prefix == 'q' or prefix == 'Q' then
    handleGeneralQuery(client, command)
  elseif prefix == 'v' then
    handleCommandv(client, command)
  elseif prefix == 'H' then
    handleCommandH(client, command)
  elseif prefix == '?' then
    dmObj:Halt()

    poll_timer:Start(0, 10, function ()
      local is_halted = dmObj:IsHalt()
      if is_halted then
        poll_timer:Stop()
        ReplyGdbCommand(client, 'S02')
      end
    end)
  elseif prefix == 'g' then
    local allReg = ''
    for i=0,31,1 do
      allReg = allReg .. reg2hexle(dmObj:AccessGPR(i))
    end

    local s0 = dmObj:AccessGPR(8)
    -- read dpc to get current pc
    allReg = allReg .. reg2hexle(dmObj:AccessCSR(0x7b1))
    dmObj:AccessGPR(8, s0)

    ReplyGdbCommand(client, allReg)
  elseif prefix == 'm' then
    handleCommandMemoryRead(client, command)
  elseif prefix == 'M' then
    handleCommandMemoryWrite(client, command)
  elseif prefix == 'p' then
    local cmdBody = tonumber(string.sub(command, 2), 16)
    assert(cmdBody >= 0, 'Invalid register index')

    local s0 = dmObj:AccessGPR(8)
    local s1 = dmObj:AccessGPR(9)
    if cmdBody <= 31 then -- Read GPR
      ReplyGdbCommand(client, reg2hexle(dmObj:AccessGPR(cmdBody)))
    elseif cmdBody == 32 then -- Read PC
      ReplyGdbCommand(client, reg2hexle(dmObj:AccessCSR(0x7b1)))
    elseif cmdBody <= 64 then -- Read fp
      ReplyGdbCommand(client, '')
    else
      ReplyGdbCommand(client, reg2hexle(dmObj:AccessCSR(cmdBody - 65)))
    end
    dmObj:AccessGPR(8, s0)
    dmObj:AccessGPR(9, s1)
  elseif prefix == 'c' then
    dmObj:Run()
    ReplyGdbCommand(client, 'OK')

    waitingUserSig = true
    poll_timer:Start(0, 10, function ()
      local is_halted = dmObj:IsHalt()
      if is_halted then
        poll_timer:Stop()
        ReplyGdbCommand(client, 'S02')
      end
    end)
  else
    print(string.format("unknow command:%s", command))
    ReplyGdbCommand(client, '')
  end
end

local commandBuffer = ""

-- 处理GDB Remote Protocol
local function receiveGdbCommand(client)
  local cmdBody = ''

  while commandBuffer ~= "" do -- 一直处理，直到commandbuffer为空

    print(string.format("cmdBuffer: %s", commandBuffer))

    -- 处理命令是否正确接收
    while not noAckMode do
      local respStatus = string.sub(commandBuffer, 1, 1)
      if respStatus == '+' then
        -- 命令被正确接收，无需重试，移除该命令
        respRetryList:pop_left()
      elseif respStatus == '-' then
        local retryCmd = respRetryList:pop_left()
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

    -- 需要等待信号
    if waitingUserSig and string.byte(commandBuffer, 1) == 0x03 then
      waitingUserSig = false
      cmdBody = '?'
      commandBuffer = string.sub(commandBuffer, 2)
    else -- 解析正常GDB命令
      local cmdStart = 0
      local cmdEnd = 0

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

      if cmdStart ~= 1 then
        print("Warn: Garbage characters appear before the command:" .. commandBuffer)
      end

      local currentCommand = string.sub(commandBuffer, cmdStart, cmdEnd + 2)
      commandBuffer = string.sub(commandBuffer, cmdEnd + 3)

      -- verfiy command checksum
      cmdBody =  string.sub(currentCommand, 2, -4)
      local cmdCheckSum = string.sub(currentCommand, -2)
      -- print(string.format("cmdBody:%s, cmdCheckSum:%s", cmdBody, cmdCheckSum))

      -- 校验数据
      local checksum = 0
      if not noAckMode then
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
      end
    end

    -- 处理命令
    handleGdbCommand(client, cmdBody)
  end
end

function ReplyGdbCommand(client, respBody)
  print(string.format("resp: %s", respBody))
  -- 计算checksum
  local checksum = 0
  if not noAckMode then
    for i=1,string.len(respBody),1 do
      checksum = checksum + string.byte(respBody, i)
    end
    checksum = checksum & 0xff
  end

  local respCommand = string.format("$%s#%02X", respBody, checksum)

  -- 将命令响应加到重传列表中
  if not noAckMode then
    respRetryList:push_right(respCommand)
  end

  client:Write(respCommand)
end

local onConnect = function(server, err)
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
end

-- ==================================================================================================================================================================

local dmi = require('arch.riscv.dmi_jtag')
local dm = require('arch.riscv.dm')

local adapterObj = require("adapters.ft2232")

-- 注册FT2232 Adapter对象
local dmiObj = dmi.Create(adapterObj)
-- 创建dm对象
dmObj = dm.Create(dmiObj)
--[[
print(string.format("0x%X", dmObj:AccessMemory(0x42030000, 4)))
print(string.format("0x%X", dmObj:AccessMemory(0x42030004, 4)))
dmObj:AccessMemory(0x42030000, 4, 0x12345678)
dmObj:AccessMemory(0x42030004, 4, 0xABCDEF01)
print(string.format("0x%X", dmObj:AccessMemory(0x42030000, 4)))
print(string.format("0x%X", dmObj:AccessMemory(0x42030004, 4)))

utils.profiler(function ()
  dmiObj:WriteReg(0x10, (0xfffff << 6) | 0x1)
end, 50000)
]]

local createGdbServer = function(adapter, addr, port)
  local r = {adapter = adapter}

  return setmetatable(r, {__index = methods})
end

GdbCreate("0.0.0.0", 1337)

loop.Run('default')
loop.Close()

return {
  Create = createGdbServer
}

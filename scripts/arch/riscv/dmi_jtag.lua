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

local A = require("Adapter")

local _M = { _VERSION = '0.0.1' }
local mt = { __index = _M }

function _M.Create(adapter)
  assert(adapter, "Invaild adapter object")
  local obj = {
    idle = 0,
    abit = 0,
  }

  local jtag = adapter:GetSkill(A.SKILL_JTAG)
  -- JTAG 原生方式读取IDCODE
  jtag:ToState(A.TAP_RESET)	-- TAP到RESET状态，默认连接IDCODE扫描链
  jtag:ToState(A.TAP_DR_SHIFT)	-- TAP到DR-Shift状态，读取idcode
  local raw_idcodes = jtag:ExchangeData(string.pack("I4I4I4I4", 0, 0, 0, 0), 128)
  -- string.unpack 额外返回一个参数是next position，下面这个返回三个参数
  local idcodes = {string.unpack("I4I4I4I4", raw_idcodes)}
  for key=1, #idcodes-1 do
    print(string.format("TAP #%d : 0x%08X", key-1, idcodes[key]))
  end

  -- 0x10 -> IR
  jtag:ToState(A.TAP_IR_SHIFT)
  jtag:ExchangeData(string.pack("I4", 0x10), 0x5)

  -- raw_dtmcs <- DR
  jtag:ToState(A.TAP_DR_SHIFT)
  local raw_dtmcs = jtag:ExchangeData(string.pack("I4", 0x0), 32)
  local dtmcs = string.unpack("I4", raw_dtmcs)
  print(string.format("DTM Control and Status: 0x%08X", dtmcs))
  print(string.format(" dtmcs.version: %d", dtmcs & 0xf))
  print(string.format(" dtmcs.abit: %d", (dtmcs >> 4) & 0x3f))
  print(string.format(" dtmcs.dmistat: %d", (dtmcs >> 10) & 0x3))
  print(string.format(" dtmcs.idle: %d", (dtmcs >> 12) & 0x7))

  -- 如果dtmstat不是No Error状态，复位 DTM
  if (((dtmcs >> 10) & 0x3) ~= 0) then
    jtag:ToState(A.TAP_DR_SHIFT)
    jtag:ExchangeData(string.pack("I4", 0x1 << 16), 32)
  end

  -- 设置IR为DMI寄存器
  -- 0x11 -> IR (Debug Module Interface Access)
  jtag:ToState(A.TAP_IR_SHIFT)
  jtag:ExchangeData(string.pack("I4", 0x11), 0x5)

  obj.abit = (dtmcs >> 4) & 0x3f
  obj.idle = (dtmcs >> 12) & 0x7
  obj.jtag = jtag

  return setmetatable(obj, mt)
end

-- TODO:优化速度
function _M.ReadReg(self, reg)
  reg = reg & ((1 << self.abit) - 1)

  self.jtag:ToState(A.TAP_DR_SHIFT)
  self.jtag:ExchangeData(string.pack("I8", (reg << 34) | 0x1), self.abit + 34)
  self.jtag:ToState(A.TAP_IDLE)
  self.jtag:Idle(self.idle) -- wait a while
  self.jtag:ToState(A.TAP_DR_SHIFT)
  local raw_register = self.jtag:ExchangeData(string.pack("I8", 0), self.abit + 34)
  local register = string.unpack("I8", raw_register)
  return (register >> 2) & 0xFFFFFFFF
end

function _M.WriteReg(self, reg, data)
  reg = reg & ((1 << self.abit) - 1)
  data = data & 0xFFFFFFFF

  self.jtag:ToState(A.TAP_DR_SHIFT)
  self.jtag:ExchangeData(string.pack("I8", (reg << 34) | (data << 0x2) | 0x2), self.abit + 34)
  self.jtag:ToState(A.TAP_IDLE)
  self.jtag:Idle(self.idle) -- wait a while
end

return _M

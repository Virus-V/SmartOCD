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

local utils = require('libs.utils')

local _M = { _VERSION = '0.0.1' }
local mt = { __index = _M }

function _M.Create(dmi)
  assert(dmi, "Invaild DMI object")
  local tmp = 0

  local obj = {
    -- 操作DM：Probe、ReadReg、WriteReg等
    dmi = dmi,
    dm_reg = {},
    gpr_reg = {},
    feature = {},
    last_operate = {}
  }

  :: reinit ::

  -- 复位 DM
  dmi:WriteReg(0x10, 0)
  -- 选择第一个hart，后续操作都针对这个hart
  dmi:WriteReg(0x10, (0xfffff << 6) | 0x1)

  -- TODO:探测DM支持哪些feature
  -- 检测是否支持Abstract Command Autoexec
  dmi:WriteReg(0x18, 0xffffffff)
  if dmi:ReadReg(0x18) ~= 0 then
    obj.feature.autoexec = 1
    print("support autoexec")
  end
  dmi:WriteReg(0x18, 0)

  -- 读abstractcs
  tmp = dmi:ReadReg(0x16)
  obj.feature.datacount = tmp & 0xf
  obj.feature.progbufsize = (tmp >> 24) & 0x1f

  -- 读dmstatus
  tmp = dmi:ReadReg(0x11)
  obj.feature.impebreak = (tmp >> 22) & 1
  obj.feature.dm_version = tmp & 0xf

  -- 读sbcs
  tmp = dmi:ReadReg(0x38)
  print(string.format("sbcs: 0x%08X", tmp))
  if tmp ~= 0 then
    print("support system bus access")
    obj.feature.sbaccess8 = tmp & 0x1 ~= 0 and 1 or nil
    obj.feature.sbaccess16 = tmp & 0x2 ~= 0 and 1 or nil
    obj.feature.sbaccess32 = tmp & 0x4 ~= 0 and 1 or nil
    obj.feature.sbaccess64 = tmp & 0x8 ~= 0 and 1 or nil
    obj.feature.sbaccess128 = tmp & 0x10 ~= 0 and 1 or nil
  end

  if obj.feature.aarpostincrement == nil then
    -- 读S0，然后regno自增，是否生效
    dmi:WriteReg(0x17, (0x2 << 20) | (0x1 << 17) | (0x1 << 19) | 0x1008)
    dmi:ReadReg(0x04)

    -- 读abstract command status状态
    local abstractcs = dmi:ReadReg(0x16)
    if ((abstractcs >> 8) & 0x7) ~= 0 then
      obj.feature.aarpostincrement = 0
      goto reinit
    end

    tmp = dmi:ReadReg(0x17)
    if (tmp & 0xff) == 0x1009 then
      obj.feature.aarpostincrement = 1
    else
      obj.feature.aarpostincrement = 0
    end
  end

  -- TODO Halt CPU
  utils.prettyPrint(obj.feature)

  local abstractcs = dmi:ReadReg(0x16)
  print(string.format("abstractcs: 0x%08X, cmderr: %d", abstractcs, (abstractcs >> 8) & 0x7))
  assert(((abstractcs >> 8) & 0x7) == 0, "cmderr")

  --dmi:WriteReg()

  return setmetatable(obj, mt)
end

function _M.CheckStatus(self)
  local abstractcs = self.dmi:ReadReg(0x16)
  local cmderr = (abstractcs >> 8) & 0x7
  local cmderr_str = {"busy", "not support", "exception", "halt/resume", "bus"}
  if cmderr ~= 0 then
    print(string.format("abstractcs: 0x%08X\n cmderr: %d - %s", abstractcs, cmderr, cmderr_str[cmderr]))
    print(string.format(" command: 0x%08X", self.dmi:ReadReg(0x17)))

    for i=0,self.feature.progbufsize - 1,1 do
      print(string.format(" progbuf%d: 0x%08X", i, self.dmi:ReadReg(0x20 + i)))
    end

    for i=0,self.feature.datacount - 1,1 do
      print(string.format(" data%d: 0x%08X", i, self.dmi:ReadReg(0x04 + i)))
    end
    -- clear error
    self.dmi:ReadReg(0x16, 0x7 << 8)
  end
end

-- 读写通用寄存器
-- TODO:需要探测是否支持hart在runing状态下访问
-- 注意，寄存器id 不需要+0x1000
function _M.AccessGPR(self, regno, data)
  local reg = (regno & 0xfff) + 0x1000
  if data == nil then -- read
    -- [cmd=0 aarsize=2 transfer=1]
    self.dmi:WriteReg(0x17, (0x2 << 20) | (0x1 << 17) | reg)
    self:CheckStatus()
    -- read data from data0, which copy regno register data into data0.
    data = self.dmi:ReadReg(0x04)
    return data
  else -- write
    -- write [data] into data0
    self.dmi:WriteReg(0x04, data)
    -- [cmd=0 aarsize=2 transfer=1 write=1]
    self.dmi:WriteReg(0x17, (0x2 << 20) | (0x1 << 17) | (0x1 << 16) | reg)
    self:CheckStatus()
  end
end

-- 读写CSR，csr addr 和csr指令中的一致
-- reference: “Zicsr”, Control and Status Register (CSR) Instructions, Version 2.0
-- reference: RISC-V Privileged Instruction Set Listings
-- reference: Control and Status Registers (CSRs)
-- 注意：该操作会修改s0寄存器，调用者应保证数据一致性
function _M.AccessCSR(self, regno, data)
  local reg = regno & 0xfff

  if data == nil then -- read csr
    -- CSRRS s0, csr, x0 into progbuf0.
    self.dmi:WriteReg(0x20, 0x2473 | (reg << 20))
    -- ebreak into progbuf1
    self.dmi:WriteReg(0x21, 0x100073)
    -- [postexec]
    self.dmi:WriteReg(0x17, 1 << 18)
    -- [aarsize=2 transfer, regno=0x1008 s0]
    self.dmi:WriteReg(0x17, (0x2 << 20) | (0x1 << 17) | 0x1008)

    data = self.dmi:ReadReg(0x04)
    self:CheckStatus()
    -- read data0
    return data
  else -- write csr: CSRW csr, rs1
    -- CSRRW x0, csr, rs1 into progbuf0
    self.dmi:WriteReg(0x20, 0x41073 | (reg << 20))
    -- ebreak into progbuf1
    self.dmi:WriteReg(0x21, 0x100073)
    -- write new value to data0
    self.dmi:WriteReg(0x04, data)
    -- [aarsize=2 postexec=1 transfer=1 write=1 regno=0x1008]
    self.dmi:WriteReg(0x17, (0x2 << 20) | (0x1 << 18) | (0x1 << 17) | (0x1 << 16) | 0x1008)
    self:CheckStatus()
  end
end

-- 通过ProgBuf访问内存，支持byte、halfword、word宽度访问
-- 注意：该操作会修改s0和s1寄存器，调用者应保证数据一致性
function _M.AccessMemory(self, address, length, data)
  assert(length, 'Missing access length')
  local length_encode = 0;

  -- reference: 2.6 Load and Store Instructions
  if length == 1 then -- load/store byte
    length_encode = 0
  elseif length == 2 then -- load/store halfword
    length_encode = 1
    assert (address & 1 == 0, "address not halfword aligned!")
  elseif length == 4 then -- load/store word
    length_encode = 2
    assert (address & 3 == 0, "address not word aligned!")
  else
    error("Invalid memory access length.")
  end

  if data == nil then
    -- load s0, 0(s0) into progbuf0
    self.dmi:WriteReg(0x20, 0x40403 | (length_encode << 12))
    -- ebreak into progbuf1
    self.dmi:WriteReg(0x21, 0x100073)
    -- address into data0
    self.dmi:WriteReg(0x04, address)

    -- Write s0, then execute program buffer
    -- [aarsize=2 postexec=1 write=1 regno=0x1008 : command=0x00271008
    self.dmi:WriteReg(0x17, (0x2 << 20) | (0x1 << 18) | (0x1 << 17) | (0x1 << 16) | 0x1008)
    self:CheckStatus()
    -- [aarsize=2 transfer=1 regno = 0x1008]
    self.dmi:WriteReg(0x17, (0x2 << 20) | (0x1 << 17) | 0x1008)
    self:CheckStatus()
    -- read data0, which store read data from addr
    data = self.dmi:ReadReg(0x04)
    if length == 1 then -- load/store byte
      data = data & 0xff;
    elseif length == 2 then -- load/store halfword
      data = data & 0xffff;
    end
    return data
  else
    -- sw s1, 0(s0) into progbuf0
    self.dmi:WriteReg(0x20, 0x940023 | (length_encode << 12))
    -- ebreak into progbuf1
    self.dmi:WriteReg(0x21, 0x00100073)

    -- operate write-addr into data0 register
    self.dmi:WriteReg(0x04, address)
    -- write s0
    -- [aarsize=2 write=1 regno=0x1008]
    self.dmi:WriteReg(0x17, (0x2 << 20) | (0x1 << 17) | (0x1 << 16) | 0x1008)
    self:CheckStatus()

    if length == 1 then -- load/store byte
      data = data & 0xff;
    elseif length == 2 then -- load/store halfword
      data = data & 0xffff;
    end
    -- operate value into data0 register
    self.dmi:WriteReg(0x04, data)
    -- write s1, then execute program buffer
    -- [aarsize=2  postexec=1 write=1 regno=0x1009
    self.dmi:WriteReg(0x17, (0x2 << 20) | (0x1 << 18) | (0x1 << 17) | (0x1 << 16) | 0x1009)
    self:CheckStatus()
  end
end

-- halt cpu
function _M.Halt(self)
  -- halt the hart, dmcontrol register haltreq bit and dmactive bit set 1
  self.dmi:WriteReg(0x10, 0x80000001)

  -- Determine whether the halt was successful
  local dmstatus = self.dmi:ReadReg(0x11)

  if(((dmstatus >> 8) & 0xf) == 0x3) then
    print(string.format("halt successful, dmstatus: 0x%08X.", dmstatus))
  end

  -- deassert haltreq
  self.dmi:WriteReg(0x10, 0x1)
end

-- run cpu
function _M.Run(self)
  self.dmi:WriteReg(0x10, 0x40000001)

  local dmstatus = self.dmi:ReadReg(0x11)
  if(((dmstatus >> 8) & 0xf) == 0xC) then
    print(string.format("run successful, dmstatus: 0x%08X", dmstatus))
  end

  self.dmi:WriteReg(0x10, 0x1)
end

function _M.Reset(self)
  -- reset the hart, dmcontrol register haltreset bit and dmactive bit set 1
  self.dmi:WriteReg(0x10, 0x40000001)
  -- Determine whether the halt was successful
  local dmstatus = self.dmi:ReadReg(0x11)

  if(((dmstatus >> 16) & 0xf) == 0xf) then
    print(string.format("reset successful, dmstatus: 0x%08X", dmstatus))
  end

  self.dmi:WriteReg(0x10, 0x1)
end

function _M.IsHalt(self)
  -- Determine whether the halt was successful
  local dmstatus = self.dmi:ReadReg(0x11)

  print(string.format("is halt? dmstatus: 0x%08X", dmstatus))
  if(((dmstatus >> 8) & 0xf) == 0x3) then
    return true
  end

  return false
end

return _M

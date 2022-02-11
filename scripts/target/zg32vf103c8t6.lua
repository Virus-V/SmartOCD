--[[--
scripts/target/bl602.lua
Copyright (c) 2020 Virus.V <virusv@live.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
--]]--

Adapter = require("Adapter")

package.path = "scripts/?.lua;"..package.path

adapterObj = require("adapters/ft2232")

jtag = adapterObj:GetSkill(Adapter.SKILL_JTAG)
-- JTAG 原生方式读取IDCODE
jtag:ToState(adapter.TAP_RESET)	-- TAP到RESET状态，默认连接IDCODE扫描链
jtag:ToState(adapter.TAP_DR_SHIFT)	-- TAP到DR-Shift状态，读取idcode
local raw_idcodes = jtag:ExchangeData(string.pack("I4I4I4I4", 0, 0, 0, 0), 128)
-- string.unpack 额外返回一个参数是next position，下面这个返回三个参数
local idcodes = {string.unpack("I4I4I4I4", raw_idcodes)}
for key=1, #idcodes-1 do
  print(string.format("TAP #%d : 0x%08X", key-1, idcodes[key]))
end

-- 0x10 -> IR
jtag:ToState(adapter.TAP_IR_SHIFT)
jtag:ExchangeData(string.pack("I4", 0x10), 0x5)

-- raw_dtmcs <- DR
jtag:ToState(adapter.TAP_DR_SHIFT)
local raw_dtmcs = jtag:ExchangeData(string.pack("I4", 0x0), 32)
local dtmcs = string.unpack("I4", raw_dtmcs)
print(string.format("DTM Control and Status: 0x%08X", dtmcs))
print(string.format(" dtmcs.version: %d", dtmcs & 0xf))
print(string.format(" dtmcs.abit: %d", (dtmcs >> 4) & 0x3f))
print(string.format(" dtmcs.dmistat: %d", (dtmcs >> 10) & 0x3))
print(string.format(" dtmcs.idle: %d", (dtmcs >> 12) & 0x7))

-- 如果dtmstat不是No Error状态，复位 DTM
if (((dtmcs >> 10) & 0x3) ~= 0) then
  jtag:ToState(adapter.TAP_DR_SHIFT)
  jtag:ExchangeData(string.pack("I4", 0x1 << 16), 32)
end

local abit = (dtmcs >> 4) & 0x3f
--local idle = (dtmcs >> 12) & 0x7
local idle = 0x10

-- read dm register
function dm_read_register(reg)
  reg = reg & ((1 << abit) - 1)

  -- 0x11 -> IR (Debug Module Interface Access)
  jtag:ToState(adapter.TAP_IR_SHIFT)
  jtag:ExchangeData(string.pack("I4", 0x11), 0x5)

  jtag:ToState(adapter.TAP_DR_SHIFT)
  jtag:ExchangeData(string.pack("I8", (reg << 34) | 0x1), abit + 34)
  jtag:ToState(adapter.TAP_IDLE)
  jtag:Idle(idle) -- wait a while
  jtag:ToState(adapter.TAP_DR_SHIFT)
  local raw_register = jtag:ExchangeData(string.pack("I8", 0), abit + 34)
  local register = string.unpack("I8", raw_register)
  return (register >> 2) & 0xFFFFFFFF
end

-- write dm reigster
function dm_write_register(reg, data)
  reg = reg & ((1 << abit) - 1)
  data = data & 0xFFFFFFFF

  -- 0x11 -> IR (Debug Module Interface Access)
  jtag:ToState(adapter.TAP_IR_SHIFT)
  jtag:ExchangeData(string.pack("I4", 0x11), 0x5)

  jtag:ToState(adapter.TAP_DR_SHIFT)
  jtag:ExchangeData(string.pack("I8", (reg << 34) | (data << 0x2) | 0x2), abit + 34)
  jtag:ToState(adapter.TAP_IDLE)
  jtag:Idle(idle) -- wait a while
end

-- ************ halt/run *************
function halt_the_hart(mesg)
  -- halt the hart, dmcontrol register haltreq bit and dmactive bit set 1
  dm_write_register(0x10, 0x80000001)
  --jtag:Idle(idle) -- wait a while
  -- Determine whether the halt was successful
  local dmstatus = dm_read_register(0x11)
  if(((dmstatus >> 8) & 0xf) == 0x3) then
    print(string.format("in [%s], halt successful.", mesg))
    --print(string.format("dmstatus hart status running or halt: 0x%1X", (dmstatus >> 8) & 0xf))
  end
  -- set dmcontrol register dmactive bit set 1
  dm_write_register(0x10, 0x1)
  local dmcontrol = dm_read_register(0x10)
  print(string.format("dmcontrol: 0x%08X", dmcontrol))
  -- clear cmderr
  dm_write_register(0x16, 0x7 << 8)
  local abstractcs = dm_read_register(0x16)
  print(string.format("abstractcs: 0x%08X", abstractcs))
end

function run_the_hart(mesg)
  -- halt the hart, dmcontrol register resumereq bit and dmactive bit set 1
  dm_write_register(0x10, 0x40000001)
  --jtag:Idle(idle) -- wait a while
  -- Determine whether the halt was successful
  local dmstatus = dm_read_register(0x11)
  if(((dmstatus >> 8) & 0xf) == 0xC) then
    print(string.format("in [%s], running successful.", mesg))
    --print(string.format("dmstatus hart status running or halt: 0x%1X", (dmstatus >> 8) & 0xf))
  end
  -- set dmcontrol register dmactive bit set 1
  dm_write_register(0x10, 0x1)
  local dmcontrol = dm_read_register(0x10)
  print(string.format("dmcontrol: 0x%08X", dmcontrol))
  -- clear cmderr
  dm_write_register(0x16, 0x7 << 8)
  local abstractcs = dm_read_register(0x16)
  print(string.format("abstractcs: 0x%08X", abstractcs))
end

-- ************ status *************
-- Determine whether the abstract command is successfully executed
function abstractcs_status(mesg)
  local abstractcs = dm_read_register(0x16)
  -- judge an abstract command is currently being executed
  while(((abstractcs >> 12) & 0x1) == 1) do
    print(string.format("in [%s], busy equal 1, abstract command is executing.", mesg))
    jtag:Idle(idle) -- wait a while
  end
  --print(string.format("dmcontrol: 0x%08X", abstractcs))
  if(((abstractcs >> 8) & 0x7) ~= 0) then
    print(string.format("in [%s], cmderr is not equal 0.", mesg))
    print(string.format("abstractcs: 0x%08X", abstractcs))
    print(string.format(" abstractcs.cmderr:%d", (abstractcs >> 8) & 0x7 ))
  end
end

function abstractcs_command_status(mesg)
  -- ensure that haltreq, resumereq, and ackhavereset are all 0.
  local dmcontrol = dm_read_register(0x10)
  if(((dmcontrol >> 28) & 0xd) ~= 0) then
    print(string.format("in [%s], haltreq, resumereq, and ackhavereset are not all 0.", mesg))
    print(string.format("dmcontrol: 0x%08X", dmcontrol))
    print(string.format(" dmcontrol.ackhavereset: %d", (dmcontrol >> 28) & 0x1 ))
    print(string.format(" dmcontrol.resumereq: %d", (dmcontrol >> 30) & 0x1))
    print(string.format(" dmcontrol.haltreq: %d", (dmcontrol >> 31)))
  end
end

-- ************ abstractcs command *************
-- read/write riscv register
-- regno : Number of the register to access
-- op : w - write operation; r - read operation
-- data: optional arg, op equal 'w', write valid data
function abstract_command_access_register(regno, op, data)
  -- checkout status
  abstractcs_command_status("abstract_command_access_register")
  -- aarsize = 2, transfer 0x220000
  local command = (0x2 << 20) | (0x1 << 17) | regno
  --print(string.format("command: 0x%08X", command))
  if(op == 'r') then
    -- read regno register.
    dm_write_register(0x17, command)
    -- read data from data0, which copy regno register data into data0.
    local register_data = dm_read_register(0x04)
    -- clear data0
    dm_write_register(0x04, 0)
    return register_data
  elseif(op == 'w') then
    -- write new data into data0 register.
    dm_write_register(0x04, data)
    -- write regno register
    command = command | (0x1 << 16)
    dm_write_register(0x17, command)
    --print(string.format("w command: 0x%08X", command))
    -- clear data0
    dm_write_register(0x04, 0)
  else
    print("Illegal operation, please enter w or r")
  end
end

-- ************ program buffer *************
function progbuf_read(addr)
  --print(string.format("------ [progbuf_read] number of register to access : 0x%08X ------", addr))
  -- checkout status
  abstractcs_command_status()
  -- lw s0, 0(s0) into progbuf0
  dm_write_register(0x20, 0x42403)
  -- ebreak into progbuf1
  dm_write_register(0x21, 0x100073)
  -- address 0x40009000
  dm_write_register(0x04, addr)

  -- Write s0, then execute program buffer
  -- [aarsize=2 transfer=1] postexec=1 write=1 regno=0x1008 : command=0x00271008
  local command = (0x2 << 20) | (0x1 << 18) | (0x1 << 17) | (0x1 << 16) | 0x1008
  dm_write_register(0x17, command)
  -- Determine whether the operation was successful
  abstractcs_status("progbuf_read write s0")

  -- read s0
  -- [aarsize=2 transfer=1] regno = 0x1008 : command=0x00221008
  command = (0x2 << 20) | (0x1 << 17) | 0x1008
  dm_write_register(0x17, command)
  -- Determine whether the operation was successful
  abstractcs_status("progbuf_read read s0")

  -- read data0, which store read data from addr
  local memory_data = dm_read_register(0x04)
  return memory_data
end

function progbuf_write(addr, data)
  -- checkout status
  abstractcs_command_status("progbuf_write")
  -- sw s1, 0(s0) into progbuf0
  dm_write_register(0x20, 0x00942023)
  -- ebreak into progbuf1
  dm_write_register(0x21, 0x00100073)

  -- operate write-addr into data0 register
  dm_write_register(0x04, addr)
  -- write s0
  -- [aarsize=2 transfer=1] write=1 regno=0x1008
  local command = (0x2 << 20) | (0x1 << 17) | (0x1 << 16) | 0x1008
  dm_write_register(0x17, command)
  -- Determine whether the operation was successful
  abstractcs_status("progbuf_write write s0")

  -- operate value into data0 register
  dm_write_register(0x04, data)
  -- write s1, then execute program buffer
  -- [aarsize=2 transfer=1] postexec=1 write=1 regno=0x1009
  command = (0x2 << 20) | (0x1 << 18) | (0x1 << 17) | (0x1 << 16) | 0x1009
  dm_write_register(0x17, command)
  -- Determine whether the operation was successful
  abstractcs_status("progbuf_write write s1")
end

print("***** DM register *****")
print(string.format("dmstatus: 0x%08X", dm_read_register(0x11)))
print(string.format("dmcontrol: 0x%08X", dm_read_register(0x10)))

local hartinfo = dm_read_register(0x12)
print(string.format("hartinfo: 0x%08X", hartinfo))
print(string.format(" hartinfo.dataaddr: 0x%X", (hartinfo >> 0) & ((0x1 << 12) - 1)))
print(string.format(" hartinfo.datasize: 0x%X", (hartinfo >> 12) & ((0x1 << 4) - 1)))
print(string.format(" hartinfo.dataaccess: %d", (hartinfo >> 16) & ((0x1 << 1) - 1)))
print(string.format(" hartinfo.nscratch: %d", (hartinfo >> 20) & ((0x1 << 4) - 1)))

-- write 1 to hartsel
dm_write_register(0x10, (0xfffff << 6) | 0x1)
print(string.format("HARTSELLEN: 0x%08X", dm_read_register(0x10)))
print(string.format("nextdm: 0x%08X", dm_read_register(0x1d)))

-- judge abstract commonds are supported, read abstractcs register
local abstractcs = dm_read_register(0x16)
print(string.format("abstractcs: 0x%08X", abstractcs))
--print(string.format(" abstractcs.progbufsize:%d", (abstractcs >> 24) & 0x7 ))
print(string.format(" abstractcs.cmderr:%d", (abstractcs >> 8) & 0x7 ))
print(string.format(" abstractcs.datacount: 0x%01X", (abstractcs & 0xf)))

-- halt hart
halt_the_hart("test")

print("****** read/write riscv register ******")
-- dcsr Debug Control and Status  0x7b0
local re_data = abstract_command_access_register(0x7b0, 'r')
print(string.format("dcsr: 0x%08X", re_data))
-- mstatus 0x300
re_data = abstract_command_access_register(0x300, 'r')
print(string.format("mstatus: 0x%08X", re_data))

print("****** read/write memory ******")
local data = progbuf_read(0x20000000)
print(string.format("data: 0x%08X", data))

progbuf_write(0x20000000, 0xabcd1234)
jtag:ToState(adapter.TAP_IDLE)
jtag:Idle(idle) -- wait a while

data = progbuf_read(0x20000000)
print(string.format("change data: 0x%08X", data))


run_the_hart("test")
jtag:ToState(adapter.TAP_IDLE)
jtag:Idle(idle) -- wait a while


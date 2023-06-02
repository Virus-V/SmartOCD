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

-- [reset]
function reset_the_hart(mesg)
  -- reset the hart, dmcontrol register haltreset bit and dmactive bit set 1
  dm_write_register(0x10, 0x40000001)
  -- Determine whether the halt was successful
  local dmstatus = dm_read_register(0x11)
  print(string.format("dmstatus: 0x%08X", dmstatus))
  if(((dmstatus >> 16) & 0xf) == 0xf) then
    print(string.format("in [%s], reset successful.", mesg))
    --print(string.format("dmstatus hart status running or halt: 0x%1X", (dmstatus >> 8) & 0xf))
  end
  -- set dmcontrol register dmactive bit set 1
  local dmstatus = dm_read_register(0x11)
  print(string.format("dmstatus: 0x%08X", dmstatus))
  --dm_write_register(0x10, 0x1)
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
  for i=0,5,1 do
    if(((abstractcs >> 12) & 0x1) == 1) then
      print(string.format("in [%s], busy equal 1, abstract command is executing.", mesg))
      jtag:ToState(adapter.TAP_IDLE)
      jtag:Idle(idle) -- wait a while
      abstractcs = dm_read_register(0x16)
    else
      break
    end
  end
  --print(string.format("dmcontrol: 0x%08X", abstractcs))
  for i=0,5,1 do
    if(((abstractcs >> 8) & 0x7) ~= 0) then
      print(string.format("in [%s], cmderr is not equal 0.", mesg))
      print(string.format("abstractcs: 0x%08X", abstractcs))
      print(string.format(" abstractcs.cmderr:%d", (abstractcs >> 8) & 0x7 ))
      jtag:ToState(adapter.TAP_IDLE)
      jtag:Idle(idle) -- wait a while
      -- clear cmderr
      dm_write_register(0x16, 0x7 << 8)
      abstractcs = dm_read_register(0x16)
      print(string.format("abstractcs again: 0x%08X", abstractcs))
    else
      break
    end
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
function access_register_read(regno)
  local reg = regno & 0xffff
  -- checkout status
  abstractcs_command_status("access_register_read")
  -- [aarsize=2 transfer=1]
  local command = (0x2 << 20) | (0x1 << 17) | reg
  dm_write_register(0x17, command)
  -- read data from data0, which copy regno register data into data0.
  local reg_data = dm_write_register(0x04)
  abstractcs_status("access_register_read")
  return reg_data
end

function access_register_write(regno, data)
  local reg = regno & 0xffff
  -- checkout status
  abstractcs_command_status("access_register_write")
  -- write [data] into data0
  dm_write_register(0x04, data)
  -- [aarsize=2 transfer=1 write=1 regno=reg] postexec=postexec
  local command = (0x2 << 20) | (0x1 << 17) | (0x1 << 16) | reg
  dm_write_register(0x17, command)
  abstractcs_status("access_register_write")
end

function access_memory_read(addr)
  print(string.format("------ [access_memory_read]: addr [0x%08X] ------", addr))
  local addr = addr & 0xffffffff
  -- checkout status
  abstractcs_command_status("abstract_command_access_register")
  -- read memory address, which writes into data1 register
  dm_write_register(0x05, addr)
  -- [cmdtype=2 aamsize=2] write=0
  local command = (0x2 << 24) | (0x2 << 20)
  dm_write_register(0x17, command)
  abstractcs_status("access_memory_read")
  -- read data from data0, which copy regno register data into data0.
  local memory_data = dm_read_register(0x04)
  return memory_data
end

function access_memory_write(addr, data)
  print(string.format("------ [access_memory_write]: addr [0x%08X] data [0x%08X] ------", addr, data))
  local addr = addr & 0xffffffff
  -- checkout status
  abstractcs_command_status("abstract_command_access_register")
  -- read memory address [addr], which writes into data1 register
  dm_write_register(0x05, addr)
  -- write [data] into data0
  dm_write_register(0x04, data)
  -- [cmdtype=2 aamsize=2 write=1]
  local command = (0x2 << 24) | (0x2 << 20) | (0x1 << 16)
  dm_write_register(0x17, command)
  abstractcs_status("access_memory_write")
end

-- ************ program buffer *************
function progbuf_read(addr)
  print(string.format("------ [progbuf_read]: addr [0x%08X] ------", addr))
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
  print(string.format("------ [progbuf_write]: addr [0x%08X] data [0x%08X] ------", addr, data))
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
print(string.format(" abstractcs.progbufsize:%d", (abstractcs >> 24) & 0x1f ))
print(string.format(" abstractcs.cmderr:%d", (abstractcs >> 8) & 0x7 ))
print(string.format(" abstractcs.datacount: 0x%01X", (abstractcs & 0xf)))

-- halt hart
halt_the_hart("test")

--[[
print("****** read/write riscv register ******")
-- dcsr Debug Control and Status  0x7b0
local re_data = access_register_read(0x7b0)
print(string.format("dcsr: 0x%08X", re_data))
-- mstatus 0x300
re_data = access_register_read(0x300)
print(string.format("mstatus: 0x%08X", re_data))
-- write/read s0 register
access_register_write(0x1008, 0x1234abcd)
re_data = access_register_read(0x1008)
print(string.format("s0: 0x%08X", re_data))
]]--

--[[
print("****** read/write memory ******")
local data = access_memory_read(0x20000000)
print(string.format("memory data: 0x%08X", data))
access_memory_write(0x20000000, 0x1234abcd)
data = access_memory_read(0x20000000)
print(string.format("change memory data: 0x%08X", data))

local data = progbuf_read(0x20000000)
print(string.format("data: 0x%08X", data))
progbuf_write(0x20000000, 0xabcd1234)
data = progbuf_read(0x20000000)
print(string.format("change data: 0x%08X", data))
]]--

-- **** flash operation ****
function is_flash_busy(mesg)
  local sr_value = progbuf_read(0x4002200c)
  print(string.format("inquire sts_busy bit, FLASH_SR register: 0x%08X", sr_value))
  while((sr_value & 0x1) == 1) do
    print(string.format("the [%s] operation", mesg))
    sr_value = progbuf_read(0x4002200c)
  end
end

function unlock_efc()
  -- [unlock]
  -- write key value into FLASH_KEYR register
  print("flash unlock")
  progbuf_write(0x40022004, 0x45670123)
  progbuf_write(0x40022004, 0x89ABCDEF)
  -- [judge]
  -- read FLASH_CR and judge whether sts_efc_lock(bit 7) is 0
  flash_reg = progbuf_read(0x40022010)
  print(string.format("unlock efc, FLASH_CR register: 0x%08X", flash_reg))
  -- try 5 times
  for i=0,5,1 do
    if((flash_reg & 0x80) ~= 0) then
      -- [unlock]
      -- write key value into FLASH_KEYR register
      print("flash unlock again")
      progbuf_write(0x40022004, 0x45670123)
      progbuf_write(0x40022004, 0x89ABCDEF)
      -- read FLASH_CR and judge whether sts_efc_lock is 0
      flash_reg = progbuf_read(0x40022010)
      print(string.format("unlock efc, FLASH_CR register read again: 0x%08X", flash_reg))
    else
      print("flash unlock successful")
      break
    end
  end
end

function unlock_option_byte()
  -- [unlock]
  -- write key value into FLASH_KEYR register
  print("option byte unlock")
  progbuf_write(0x40022008, 0x45670123)
  progbuf_write(0x40022008, 0x89ABCDEF)
  -- [judge]
  -- read FLASH_CR and judge whether sts_opt_lock(bit 9) is 0
  flash_reg = progbuf_read(0x40022010)
  print(string.format("unlock option_byte FLASH_CR register: 0x%08X", flash_reg))
  for i=0,5,1 do
    if((flash_reg & 0x200) ~= 0) then
      -- [unlock]
      -- write key value into FLASH_KEYR register
      print("option byte unlock again")
      progbuf_write(0x40022008, 0x45670123)
      progbuf_write(0x40022008, 0x89ABCDEF)
      -- read FLASH_CR and judge whether sts_opt_lock is 0
      flash_reg = progbuf_read(0x40022010)
      print(string.format("unlock option_byte FLASH_CR register read again: 0x%08X", flash_reg))
    else
      print("option_byte unlock successful")
      break
    end
  end
end



print("****** read/write flash ******")
-- flash register : 0x40022000

function erase_efc()
  progbuf_write(0x4002200C, 0)
  -- [unlock efc]
  unlock_efc()
  -- [unlock option_byte]
  unlock_option_byte()

  -- [erase option byte]
  -- set cfg_opt_erase bit(bit 5) into FLASH_CR register
  local flash_reg = progbuf_read(0x40022010)
  local flash_cr_value = flash_reg | 0x20
  progbuf_write(0x40022010, flash_cr_value)
  -- write page addr into FLASH_AR
  progbuf_write(0x40022014, 0x1fff8600)
  -- set cfg_start(bit 6) bit into FLASH_CR register
  flash_reg = progbuf_read(0x40022010)
  flash_cr_value = flash_reg | 0x40
  progbuf_write(0x40022010, flash_cr_value)

  jtag:ToState(adapter.TAP_IDLE)
  jtag:Idle(idle) -- wait a while
  is_flash_busy("erase option byte")

  -- [set option byte]
  -- set cfg_opt_program bit into FLASH_CR
  progbuf_write(0x40022010, 0x10)
  progbuf_write(0x1fff860c, 0x00ff00ff)
  --print(string.format("[ option byte 4: 0x%08X ]", progbuf_read(0x1fff860c)))
  progbuf_write(0x1fff8608, 0x00ff00ff)
  --print(string.format("[ option byte 3: 0x%08X ]", progbuf_read(0x1fff8608)))
  progbuf_write(0x1fff8604, 0xff00ff00)
  --print(string.format("[ option byte 2: 0x%08X ]", progbuf_read(0x1fff8604)))
  progbuf_write(0x1fff8600, 0x00ff5aa5)
  --print(string.format("[ option byte 1: 0x%08X ]", progbuf_read(0x1fff8600)))
  -- clear cfg_opt_program bit into FLASH_CR
  progbuf_write(0x40022010, 0)
end

function erase_flash()
  unlock_efc()
  -- [erase flash]
  -- set cfg_page_erase bit (bit 1) into FLASH_CR register
  local flash_reg = progbuf_read(0x40022010)
  local flash_cr_value = flash_reg | 0x02
  progbuf_write(0x40022010, flash_cr_value)
  -- write page addr into FLASH_AR
  progbuf_write(0x40022014, 0x08000000)
  -- set cfg_start bit(bit 6) into FLASH_CR register
  flash_reg = progbuf_read(0x40022010)
  flash_cr_value = flash_reg | 0x40
  progbuf_write(0x40022010, flash_cr_value)
  is_flash_busy("erase flash")

  jtag:ToState(adapter.TAP_IDLE)
  jtag:Idle(idle) -- wait a while
  -- reset cfg_page_erase
  flash_reg = progbuf_read(0x40022010)
  flash_cr_value = flash_reg & 0xFFFFFFFD
  progbuf_write(0x40022010, flash_cr_value)
  local memory_data = progbuf_read(0x08000000)
  print(string.format("[ memory data: 0x%08X ]", memory_data))
end

function write_flash(addr)
  is_flash_busy()
  -- set cfg_program bit(bit 1) into FLASH_CR register
  local flash_reg = progbuf_read(0x40022010)
  local flash_cr_value = flash_reg | 0x1
  progbuf_write(0x40022010, flash_cr_value)
  -- write data into addr
  progbuf_write(0x08000000, 0x1234abcd)
  -- verify
  print(string.format("[ read memory data: 0x%08X ]", progbuf_read(0x08000000)))
end


erase_efc()
--erase_flash()
--write_flash()

--[[
--local data = progbuf_read(0x08000000)
--print(string.format("flash memory data: 0x%08X", data))
--progbuf_write(0x08000000, 0x1234abcd)
--data = progbuf_read(0x08000000)
--print(string.format("change data: 0x%08X", data))
local data = access_memory_read(0x08000000)
print(string.format("flash data: 0x%08X", data))
access_memory_write(0x08000000, 0x1234abcd)
data = access_memory_read(0x08000000)
print(string.format("change data: 0x%08X", data))
]]--

run_the_hart("test")
jtag:ToState(adapter.TAP_IDLE)
jtag:Idle(idle) -- wait a while

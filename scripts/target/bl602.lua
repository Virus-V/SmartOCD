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
print(string.format("DTMCS:0x%08X", dtmcs))

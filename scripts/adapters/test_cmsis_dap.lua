--[[
    CMSIS-DAP测试程序
]]

dofile("scripts/adapters/cmsis_dap.lua")    -- 获得CMSIS-DAP对象
-- 系统复位
cmObj:Reset(adapter.RESET_SYSTEM)

cmObj:TransferMode(adapter.MODE_JTAG)
print("Change transfer mode to JTAG")

jtagSkill = cmObj:GetSkill(adapter.SKILL_JTAG);

-- 读取JTAG Pins
local pins_data = jtagSkill:JtagPins(0, 0, 0)
print("JTAG Pins: ")
if pins_data & adapter.PIN_SWCLK_TCK > 0 then
	print(" TCK = 1")
else
	print(" TCK = 0")
end
if pins_data & adapter.PIN_SWDIO_TMS > 0 then
	print(" TMS = 1")
else
	print(" TMS = 0")
end
if pins_data & adapter.PIN_TDI > 0 then
	print(" TDI = 1")
else
	print(" TDI = 0")
end
if pins_data & adapter.PIN_TDO > 0 then
	print(" TDO = 1")
else
	print(" TDO = 0")
end
if pins_data & adapter.PIN_nTRST > 0 then
	print(" nTRST = 1")
else
	print(" nTRST = 0")
end
if pins_data & adapter.PIN_nRESET > 0 then
	print(" nRESET = 1")
else
	print(" nRESET = 0")
end

-- JTAG 原生方式读取IDCODE
jtagSkill:JtagToState(adapter.TAP_RESET)	-- TAP到RESET状态，默认连接IDCODE扫描链
jtagSkill:JtagToState(adapter.TAP_DR_SHIFT)	-- TAP到DR-Shift状态，读取idcode
local idcodes = jtagSkill:JtagExchangeData(string.pack("I4I4", 0, 0), 64)
local idcodesa, idcodesb = string.unpack("I4I4", idcodes)
print(string.format("%x,%x", idcodesa, idcodesb))

-- cmObj:Reset(adapter.RESET_DEBUG)

-- -- SmartOCD扩展方式读取IDCODE
-- cmObj:JtagConfig{4,5}
-- cmObj:jtagWriteTAPIR(0, adapter.JTAG_IDCODE)
-- local arm_idcode = cmObj:jtagExchangeTAPDR(0, "\000\000\000\000", 32)
-- print(string.format("%x", string.unpack("I4", arm_idcode)))

-- 切换到SWD模式
cmObj:TransferMode(adapter.MODE_SWD)
print("Change transfer mode to SWD")

dapSkill = cmObj:GetSkill(adapter.SKILL_DAP);

-- 读取DP IDR
local dpidr = dapSkill:DapSingleRead(adapter.REG_DP, 0)
print(string.format("DPIDR: 0x%X", dpidr)) -- 0x2BA01477
-- 写ABORT寄存器
dapSkill:DapSingleWrite(adapter.REG_DP, 0, 0x1e)
-- 写SELECT寄存器
dapSkill:DapSingleWrite(adapter.REG_DP, 0x08, 0x0)
-- 写CTRL_STAT,系统域和调试域上电
dapSkill:DapSingleWrite(adapter.REG_DP, 0x04, 0x50000000)

repeat
    -- 等待上电完成
    local ctrl_stat = dapSkill:DapSingleRead(adapter.REG_DP, 0x04)
until (ctrl_stat & 0xA0000000) == 0xA0000000

print("System & Debug power up.")
-- 读 AHB AP 的CSW和IDR寄存器
dapSkill:DapSingleWrite(adapter.REG_DP, 0x08, 0x0F000000)  -- 写SELECT
local apidr = dapSkill:DapSingleRead(adapter.REG_AP, 0x0C) -- 读APIDR
dapSkill:DapSingleWrite(adapter.REG_DP, 0x08, 0x00000000)  -- 写SELECT
local apcsw = dapSkill:DapSingleRead(adapter.REG_AP, 0x00) -- 读APCSW
print(string.format("AP[0].IDR: 0x%X, AP[0].CSW: 0x%X.", apidr, apcsw))
-- 读ROM的第一个uint32
dapSkill:DapSingleWrite(adapter.REG_AP, 0x04, 0x08000000)  -- 写TAR_LSB, 0x0800000为ROM的起始地址
local data = dapSkill:DapSingleRead(adapter.REG_AP, 0x0C) -- 读DRW
print(string.format("0x08000000 => 0x%X.", data))

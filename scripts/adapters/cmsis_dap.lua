--[[
 * SmartOCD CMSIS-DAP操作
 * Date:2018年04月20日22:13:45
 * Author: Virus.V <virusv@live.com>
]]
adapter = require("Adapter")
cmsis_dap = require("CMSIS-DAP"); -- 加载CMSIS-DAP库
print("Init CMSIS-DAP.")

cmObj = cmsis_dap.Create(); -- 创建新的CMSIS-DAP Adapter对象
-- CMSIS-DAP的VID和PID
local vid_pids = {
	-- Keil Software
	{0xc251, 0xf001},	-- LPC-Link-II CMSIS_DAP
	{0xc251, 0xf002}, 	-- OPEN-SDA CMSIS_DAP (Freedom Board)
	{0xc251, 0x2722}, 	-- Keil ULINK2 CMSIS-DAP
	-- MBED Software
	{0x0d28, 0x0204}	-- MBED CMSIS-DAP
}
-- 连接CMSIS-DAP仿真器
cmObj:Connect(vid_pids, nil)
-- 配置CMSIS-DAP Transfer参数
cmObj:TransferConfig(5, 5, 5)
-- SWD参数
cmObj:SwdConfig(0)
-- 设置传输频率
cmObj:Frequent(5000000)	-- 500KHz
print("CMSIS-DAP: Current frequent is 5MHz")
-- 选择SWD传输模式
local transMode = cmObj:TransferMode()
if transMode == adapter.MODE_SWD then
	print("CMSIS-DAP: Current transfer mode is SWD")
elseif transMode == adapter.MODE_JTAG then
	print("CMSIS-DAP: Current transfer mode is JTAG")
else
	print("CMSIS-DAP: Can't auto select a transfer mode! Default SWD!")
	cmObj:TransferMode(adapter.MODE_SWD)
end

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

-- 系统复位
cmObj:Reset(adapter.RESET_SYSTEM)

-- -- JTAG 原生方式读取IDCODE
-- AdapterObj:jtagStatusChange(adapter.TAP_RESET)	-- TAP到RESET状态，默认连接IDCODE扫描链
-- AdapterObj:jtagStatusChange(adapter.TAP_DR_SHIFT)	-- TAP到DR-Shift状态，读取idcode
-- local idcodes = AdapterObj:jtagExchangeIO(string.pack("I4I4", 0, 0), 64)
-- local idcodesa, idcodesb = string.unpack("I4I4", idcodes)
-- print(string.format("%x,%x", idcodesa, idcodesb))

-- AdapterObj:reset()	-- 复位
-- -- SmartOCD扩展方式读取IDCODE
-- AdapterObj:jtagSetTAPInfo{4,5}
-- print(adapter.JTAG_IDCODE)
-- AdapterObj:jtagWriteTAPIR(0, adapter.JTAG_IDCODE)
-- local arm_idcode = AdapterObj:jtagExchangeTAPDR(0, "\000\000\000\000", 32)
-- print(string.format("%x", string.unpack("I4", arm_idcode)))





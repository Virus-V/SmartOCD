--[[
 * SmartOCD CMSIS-DAP操作
 * Date:2018年04月20日22:13:45
 * Author: Virus.V <virusv@live.com>
]]
adapter = require("Adapter");	-- 加载Adapter库
cmsis_dap = require("CMSIS-DAP"); -- 加载CMSIS-DAP库
cmsis_dapObj = cmsis_dap.New(); -- 创建新的CMSIS-DAP对象
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
cmsis_dap.Connect(cmsis_dapObj, vid_pids, nil)
-- 初始化Adapter对象
cmsis_dapObj:Init()
-- 配置CMSIS-DAP Transfer参数
cmsis_dap.TransferConfigure(cmsis_dapObj, 5, 5, 5)
-- SWD参数
cmsis_dap.swdConfigure(cmsis_dapObj, 0)
-- 点亮连接状态指示灯
cmsis_dapObj:SetStatus(adapter.STATUS_CONNECTED)
cmsis_dapObj:SetClock(100000)	-- 10MHz
-- 选择传输模式
if cmsis_dapObj:HaveTransType(adapter.SWD) then
	cmsis_dapObj:SelectTransType(adapter.SWD)
else
	cmsis_dapObj:SelectTransType(adapter.JTAG)
end
cmsis_dapObj:SetStatus(adapter.STATUS_RUNING)

-- 读取JTAG Pins
local pins_data = cmsis_dapObj:jtagPins(0, 0, 0)
print("JTAG Pins: TCK:" .. ((pins_data & adapter.PIN_SWCLK_TCK) >> adapter.PIN_SWCLK_TCK)
	.. " TMS:" .. ((pins_data & (1 << adapter.PIN_SWDIO_TMS)) >> adapter.PIN_SWDIO_TMS)
	.. " TDI:" .. ((pins_data & (1 << adapter.PIN_TDI)) >> adapter.PIN_TDI)
	.. " TDO:" .. ((pins_data & (1 << adapter.PIN_TDO)) >> adapter.PIN_TDO)
	.. " nTRST:" .. ((pins_data & (1 << adapter.PIN_nTRST)) >> adapter.PIN_nTRST)
	.. " nRESET:" .. ((pins_data & (1 << adapter.PIN_nRESET)) >> adapter.PIN_nRESET))

-- -- JTAG 原生方式读取IDCODE
-- cmsis_dapObj:jtagStatusChange(adapter.TAP_RESET)	-- TAP到RESET状态，默认连接IDCODE扫描链
-- cmsis_dapObj:jtagStatusChange(adapter.TAP_DR_SHIFT)	-- TAP到DR-Shift状态，读取idcode
-- local idcodes = cmsis_dapObj:jtagExchangeIO(string.pack("I4I4", 0, 0), 64)
-- local idcodesa, idcodesb = string.unpack("I4I4", idcodes)
-- print(string.format("%x,%x", idcodesa, idcodesb))

-- cmsis_dapObj:reset()	-- 复位
-- -- SmartOCD扩展方式读取IDCODE
-- cmsis_dapObj:jtagSetTAPInfo{4,5}
-- print(adapter.JTAG_IDCODE)
-- cmsis_dapObj:jtagWriteTAPIR(0, adapter.JTAG_IDCODE)
-- local arm_idcode = cmsis_dapObj:jtagExchangeTAPDR(0, "\000\000\000\000", 32)
-- print(string.format("%x", string.unpack("I4", arm_idcode)))





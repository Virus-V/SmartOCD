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
cmObj:Frequent(20000000)	-- 500KHz
print("CMSIS-DAP: Current frequent is " .. cmObj:Frequent() .. "Hz")
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

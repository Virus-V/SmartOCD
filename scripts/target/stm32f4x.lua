--[[
    STM32F407测试
]]
dap = require("DAP")
dofile("scripts/adapters/cmsis_dap.lua")    -- 获得CMSIS-DAP对象
dofile("scripts/libs/romtable.lua")
if cmsis_dapObj:CurrentTransType() == adapter.JTAG then
	-- JTAG 原生方式读取IDCODE
	cmsis_dapObj:jtagStatusChange(adapter.TAP_RESET)	-- TAP到RESET状态，默认连接IDCODE扫描链
	cmsis_dapObj:jtagStatusChange(adapter.TAP_DR_SHIFT)	-- TAP到DR-Shift状态，读取idcode
	local raw_idcodes = cmsis_dapObj:jtagExchangeIO(string.pack("I4I4I4I4", 0, 0, 0, 0), 128)
	-- string.unpack 额外返回一个参数是next position，下面这个返回三个参数
	local idcodes = {string.unpack("I4I4I4I4", raw_idcodes)}
	for key=1, #idcodes-1 do
		print(string.format("TAP #%d : 0x%08X", key-1, idcodes[key]))
	end
	-- DAP测试
	cmsis_dap.jtagConfigure(cmsis_dapObj, {4, 5})    -- 当前是JTAG模式，设置仿真器中JTAG扫描链TAP个数和IR长度信息
	dap.SetIndex(cmsis_dapObj, 0)    -- 设置DAP在JTAG扫描链中的索引，SWD模式下忽略
end

dap.SetRetry(cmsis_dapObj, 5)    -- 设置WAIT重试次数
dap.SetIdleCycle(cmsis_dapObj, 5)    -- 设置两次传输之间的空闲周期
dap.Init(cmsis_dapObj)   -- 初始化DAP
dap.FindAP(cmsis_dapObj, dap.AP_TYPE_AMBA_AHB)   -- 选择AMBA APB AP
--[[
    读取当前AP参数
    PT:Packed Transfer
    LD:Large Data 支持64位，128位，256位数据传输
    LA:Large Address
    BE:Big-Endian
    BT:Byte Transfer 支持字节、半字、字宽度传输
]]
do 
	local PackedTrans, LargeData, LargeAddr, BigEndian, ByteTrans = dap.GetAPCapacity(cmsis_dapObj, "PT","LD","LA","BE","BT")
	if PackedTrans then print("Support Packed Transfer.") end
	if LargeData then print("Support Large Data Extention.") end
	if LargeAddr then print("Support Large Address.") end
	if BigEndian then print("Big Endian Access.") end
	if ByteTrans then print("Support Less Word Transfer.") end
end 

local rom_table_base = dap.GetROMTable(cmsis_dapObj)
print(string.format("ROM Table Base: 0x%08X", rom_table_base))
dap.SetCSWReg(cmsis_dapObj, 0xa2000000)	-- 设置CSW，具体参考CoreSight Specs
DisplayROMTable(cmsis_dapObj, rom_table_base, 0)
print(string.format("CTRL/STAT:0x%08X", dap.GetCTRL_STATReg(cmsis_dapObj)))
print("Hello World")
cmsis_dapObj:SetStatus(adapter.STATUS_IDLE)

--[[
    STM32F407测试
]]
dap = require("DAP")
dofile("scripts/adapters/cmsis_dap.lua")    -- 获得CMSIS-DAP对象
dofile("scripts/libs/romtable.lua")
if AdapterObj:TransType() == adapter.JTAG then
	-- JTAG 原生方式读取IDCODE
	AdapterObj:jtagStatusChange(adapter.TAP_RESET)	-- TAP到RESET状态，默认连接IDCODE扫描链
	AdapterObj:jtagStatusChange(adapter.TAP_DR_SHIFT)	-- TAP到DR-Shift状态，读取idcode
	local raw_idcodes = AdapterObj:jtagExchangeIO(string.pack("I4I4I4I4", 0, 0, 0, 0), 128)
	-- string.unpack 额外返回一个参数是next position，下面这个返回三个参数
	local idcodes = {string.unpack("I4I4I4I4", raw_idcodes)}
	for key=1, #idcodes-1 do
		print(string.format("TAP #%d : 0x%08X", key-1, idcodes[key]))
	end
	-- DAP测试
	cmsis_dap.jtagConfigure(AdapterObj, {4, 5})    -- 当前是JTAG模式，设置仿真器中JTAG扫描链TAP个数和IR长度信息
	dap.SetIndex(AdapterObj, 0)    -- 设置DAP在JTAG扫描链中的索引，SWD模式下忽略
end

dap.SetRetry(AdapterObj, 5)    -- 设置WAIT重试次数
dap.SetIdleCycle(AdapterObj, 5)    -- 设置两次传输之间的空闲周期
dap.Init(AdapterObj)   -- 初始化DAP
dap.FindAP(AdapterObj, dap.AP_TYPE_AMBA_AHB)   -- 选择AMBA APB AP
--[[
    读取当前AP参数
    PT:Packed Transfer
    LD:Large Data 支持64位，128位，256位数据传输
    LA:Large Address
    BE:Big-Endian
    BT:Byte Transfer 支持字节、半字、字宽度传输
]]
do 
	local PackedTrans, LargeData, LargeAddr, BigEndian, ByteTrans = dap.GetAPCapacity(AdapterObj, "PT","LD","LA","BE","BT")
	if PackedTrans then print("Support Packed Transfer.") end
	if LargeData then print("Support Large Data Extention.") end
	if LargeAddr then print("Support Large Address.") end
	if BigEndian then print("Big Endian Access.") end
	if ByteTrans then print("Support Less Word Transfer.") end
end 

--local rom_table_base = dap.GetROMTable(AdapterObj)
--print(string.format("ROM Table Base: 0x%08X", rom_table_base))
-- 初始化系统时钟
-- dap.Memory32(AdapterObj, 0x40023804, 0x08012008)	-- RCC_PLLCFGR 16 Mhz /8 (M) * 128 (N) /4(P)
-- print("OK");
-- dap.Memory32(AdapterObj, 0x40023C00, 0x00000102)	-- FLASH_ACR = PRFTBE | 2(Latency)
-- print("OK");
-- local RCC_CR = dap.Memory32(AdapterObj, 0x40023800)	-- RCC_CR |= PLLON
-- print("OK");
-- dap.Memory32(AdapterObj, 0x40023800, RCC_CR | 0x01000000)
-- print("OK");
-- sleep(100)	-- Wait for PLL to lock
-- dap.Memory32(AdapterObj, 0x40023808, dap.Memory32(AdapterObj, 0x40023808) | 0x00001000)	-- RCC_CFGR |= RCC_CFGR_PPRE1_DIV2
-- print("OK");
-- dap.Memory32(AdapterObj, 0x40023808, dap.Memory32(AdapterObj, 0x40023808) | 0x00000002)	-- RCC_CFGR |= RCC_CFGR_SW_PLL
-- print("OK");
-- --AdapterObj:SetClock(4000000)	-- 将仿真器时钟频率设置为8MHz
-- -- 在低功耗模式下允许调试
-- dap.Memory32(AdapterObj, 0xE0042004, dap.Memory32(AdapterObj, 0xE0042004) | 0x00000007)	-- DBGMCU_CR |= DBG_STANDBY | DBG_STOP | DBG_SLEEP
-- print("Fault");
-- -- 在停机模式下停止看门狗
-- dap.Memory32(AdapterObj, 0xE0042008, dap.Memory32(AdapterObj, 0xE0042008) | 0x00001800)	-- DBGMCU_APB1_FZ |= DBG_IWDG_STOP | DBG_WWDG_STOP
-- print("OK");
-- -- dap.Reg(AdapterObj, dap.AP_REG_CSW, 0xa2000002)	-- 设置CSW，具体参考CoreSight Specs
-- print(string.format("CSW: 0x%08X.",dap.Reg(AdapterObj, dap.AP_REG_CSW)))

-- DisplayROMTable(AdapterObj, rom_table_base, 0)
local rom_table_base = dap.GetROMTable(AdapterObj)
print(string.format("ROM Table Base: 0x%08X", rom_table_base))
-- 打印ROM开头的
for ROMAddr=0x08000000,0x08000040,4 do
	-- Word Read
	print(string.format("[0x%08X]:0x%08X.", ROMAddr, dap.Memory32(AdapterObj, ROMAddr)))
	-- Half Word Read
	print(string.format("[0x%08X]:0x%04X.", ROMAddr, dap.Memory16(AdapterObj, ROMAddr)))
	print(string.format("[0x%08X]:0x%04X.", ROMAddr+2, dap.Memory16(AdapterObj, ROMAddr+2)))
	-- Byte Read
	print(string.format("[0x%08X]:0x%02X.", ROMAddr, dap.Memory8(AdapterObj, ROMAddr)))
	print(string.format("[0x%08X]:0x%02X.", ROMAddr+1, dap.Memory8(AdapterObj, ROMAddr+1)))
	print(string.format("[0x%08X]:0x%02X.", ROMAddr+2, dap.Memory8(AdapterObj, ROMAddr+2)))
	print(string.format("[0x%08X]:0x%02X.", ROMAddr+3, dap.Memory8(AdapterObj, ROMAddr+3)))
end
print("Hello World")
print(string.format("CTRL/STAT:0x%08X", dap.Reg(AdapterObj, dap.DP_REG_CTRL_STAT)))
print("Hello World")
-- for addr=0x08000000, 0x08003700, 4 do
-- 	print(string.format("Addr:0x%08X:0x%08X", addr, dap.Memory32(AdapterObj, addr)))
-- end
AdapterObj:SetStatus(adapter.STATUS_IDLE)

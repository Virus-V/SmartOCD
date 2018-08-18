# SmartOCD
Smart On Chip Debugger

-----

Intrusive on-chip debugging tools for the ARMv8 architecture. The purpose is to use it to easily debug Uboot, UEFI programs or operating system kernels.

The chip can be tested using the Lua scripting language, or GDB can be used to perform single-step operations, breakpoints, etc. on the chip.

-----

用于ARMv8架构的交互式片上调试工具。 目的是使用它来调试Uboot，UEFI程序或操作系统内核。

可以使用Lua脚本语言测试芯片，或者可以使用GDB在芯片上执行单步操作，断点等。

支持的仿真器有：
 - CMSIS-DAP
 - USB Blaster（即将支持）
 - J-Link（即将支持）

简单示例——CMSIS-DAP仿真器初始化：
```lua
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
cmsis_dapObj:SetClock(10000000)	-- 10MHz
-- 选择传输模式
if cmsis_dapObj:HaveTransType(adapter.SWD) then	-- 如果支持SWD模式，则优先选择
	cmsis_dapObj:SelectTransType(adapter.SWD)
else
	cmsis_dapObj:SelectTransType(adapter.JTAG)
end
cmsis_dapObj:SetStatus(adapter.STATUS_RUNING)	-- 点亮仿真器“运行”状态LED
```
简单示例——打印STM32F407 的 ROM Table：
```lua
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
```

输出：
```
   _____                      __  ____  __________ 
  / ___/____ ___  ____ ______/ /_/ __ \/ ____/ __ \
  \__ \/ __ `__ \/ __ `/ ___/ __/ / / / /   / / / /
 ___/ / / / / / / /_/ / /  / /_/ /_/ / /___/ /_/ / 
/____/_/ /_/ /_/\__,_/_/   \__/\____/\____/_____/  
 * SmartOCD v1.0.0 By: Virus.V <virusv@live.com>
 * Complile time: 2018-08-18T22:50:26+0800
 * Github: https://github.com/Virus-V/SmartOCD
22:57:38 DEBUG cmsis-dap.c:115: Try connecting vid: 0xc251, pid: 0xf001 usb device.
22:57:38 DEBUG usb.c:101: Found 8 usb devices.
22:57:38 INFO  cmsis-dap.c:117: Successful connection vid: 0xc251, pid: 0xf001 usb device.
22:57:38 INFO  usb.c:291: Currently it is configuration indexed by:0, bConfigurationValue is 1.
22:57:38 DEBUG usb.c:372: usb end point 'in' 0x81, max packet size 64 bytes.
22:57:38 DEBUG usb.c:376: usb end point 'out' 0x01, max packet size 64 bytes.
22:57:38 DEBUG usb.c:397: Claiming interface 0
22:57:38 INFO  cmsis-dap.c:189: CMSIS-DAP Init.
22:57:38 INFO  cmsis-dap.c:212: CMSIS-DAP the maximum Packet Size is 1024.
22:57:38 INFO  cmsis-dap.c:223: CMSIS-DAP FW Version is 1.100000.
22:57:38 INFO  cmsis-dap.c:229: CMSIS-DAP the maximum Packet Count is 4.
22:57:38 INFO  cmsis-dap.c:262: CMSIS-DAP Capabilities 0x13.
22:57:38 INFO  cmsis-dap.c:1417: Set Transfer Configure: Idle:5, Wait:5, Match:5.
22:57:38 INFO  cmsis-dap.c:1434: SWD: Turnaround clock period: 1 clock cycles. DataPhase: No.
22:57:38 INFO  cmsis-dap.c:518: CMSIS-DAP Clock Frequency: 100.00KHz.
22:57:38 INFO  cmsis-dap.c:380: Switch to SWD mode.
JTAG Pins: TCK:0 TMS:0 TDI:0 TDO:1 nTRST:0 nRESET:1
22:57:38 INFO  DAP.c:30: DAP DPIDR:0x2BA01477.
22:57:38 DEBUG DAP.c:59: DAP Power up. CTRL_STAT:0xF0000040.
22:57:38 DEBUG DAP.c:114: Default CSW: 0x23000042.
22:57:38 INFO  DAP.c:184: Probe AP 0, AP_IDR: 0x24770011, CTRL/STAT: 0xF0000040.
Support Packed Transfer.
Support Less Word Transfer.
ROM Table Base: 0xE00FF003

* Component CID:0xB105100D, PID:0x00000A0411.
- MEMTYPE system memory present on bus.
- Next Entry:0xFFF0F003 BaseAddr:0xE000E000.

* Component CID:0xB105E00D, PID:0x04000BB00C.
Cortex-M4 SCS (System Control Space)
- Next Entry:0xFFF02003 BaseAddr:0xE0001000.

* Component CID:0xB105E00D, PID:0x04003BB002.
Cortex-M3 DWT (Data Watchpoint and Trace)
- Next Entry:0xFFF03003 BaseAddr:0xE0002000.

* Component CID:0xB105E00D, PID:0x04002BB003.
Cortex-M3 FPB (Flash Patch and Breakpoint)
- Next Entry:0xFFF01003 BaseAddr:0xE0000000.

* Component CID:0xB105E00D, PID:0x04003BB001.
Cortex-M3 ITM (Instrumentation Trace Module)
- Next Entry:0xFFF41003 BaseAddr:0xE0040000.

* Component CID:0xB105900D, PID:0x04000BB9A1.
Cortex-M4 TPIU (Trace Port Interface Unit)
- Type is Trace Sink - Port
- Next Entry:0xFFF42003 BaseAddr:0xE0041000.

* Component CID:0xB105900D, PID:0x04000BB925.
Cortex-M4 ETM (Embedded Trace)
- Type is Trace Source - Processor
CTRL/STAT:0xF0000040
Hello World

```
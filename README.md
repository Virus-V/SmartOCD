# SmartOCD

## 简介

SmartOCD(Smart On Chip Debugger)是用于ARMv8架构的交互式片上调试工具。目的是使用它来更方便地调试Uboot，UEFI程序或操作系统内核。

它可以使用Lua脚本语言编程，通过DAP协议和MEM-AP与CoreSight组件进行交互，或者直接读写总线上挂载的设备（ROM、RAM或Peripherals）等。

*此项目为实验性项目，开发它只是为了更好的去理解和学习底层的一些东西。*

目前支持的仿真器有：
- CMSIS-DAP
+ USB Blaster *（即将支持）*
+ J-Link *（即将支持）*

## DAP

> The Debug Access Port(DAP) is an implementation of an ARM Debug Interface version 5.1 (ADIv5.1) comprising a number of components supplied in a single configuration.  
> All the supplied components fit into the various architectural components for Debug Ports(DPs), which are used to access the DAP from an external debugger and Access Ports (APs), to access on-chip system resources.  
> The DAP provides **real-time** access for the debugger without halting the processor to:
> * AMBA system memory and peripheral registers.
> * All debug configuration registers.

简单来说，DAP是仿真器和被调试芯片之间的桥梁。它由DP和AP两个部分组成：
- DP：仿真器通过DP来访问DAP，它一般有三类：
	- SW-DP：通过SW（Serial Wire）协议访问DAP  
	注：SW协议只存在于ADIv5以及以上版本中
	- JTAG-DP：通过JTAG协议访问DAP
	- SWJ-DP：通过SWD/JTAG协议访问DAP
- AP：DAP通过AP来访问片上资源，它一般有四种：
	- JTAG-AP：片上TAP控制器的DAP接口，用来访问SoC中的JTAG设备。它作为JTAG Master驱动JTAG Chain
	- AHB-AP：AHB System的DAP接口，可以访问AHB总线上的设备。
	- APB-AP：Debug APB的DAP接口，一般用它来访问调试组件。
	- AXI-AP：AXI总线的DAP接口。

## CoreSight

> Modern SoCs are continually increasing in complexity and packing in more and more features.  Large designs with mul-tiple CPU architectures, GPUs, numerous power domains, various DVFS regions, big.LITTLE™ and advanced security schemes are very common. This in turn adds a big overhead for silicon bring-up and software devel-opment/optimization while at the same time designers are under pressure to keep costs down.  
> CoreSight, ARM’s debug and trace product family, is the most complete on-chip debug and trace solution for the entire System-On-Chip (SoC), making ARM-based SoCs the easiest to debug and optimize.   
> 
> **Key Benefits**
> - Higher visibility of complete system operation through fewer pins
> - Standard solution across all silicon vendors for widest tools support
> - Re-usable for single ARM core, multi-core or core and DSP systems
> - Enables faster time-to-market for more reliable and higher performance products
> - Supports for high performance, low power SoCs

CoreSight是面对越来越复杂、功能越来越多的片上系统而提出的Debug & Trace的解决方案。CoreSight包含有许多调试组件，这些组件与CoreSight架构一起构成了CoreSight系统。

## JTAG、SWD
JTAG和SWD是仿真器与被调试芯片之间两种不同的通信方式。JTAG遵循IEEE 1149.7，主要用于芯片内部测试，属于一种比较“古老”的协议，多用在DSP、FPGA和较早系列的ARM中。SWD是串行调试协议，它是在ADIv5（ARM Debug Interface version 5）标准中提出来的一种串行协议。SWD比JTAG在高速模式下面更加可靠，比JTAG所占用的引脚资源更少。所以推荐使用SWD。

## Lua
1993 年在巴西里约热内卢天主教大学(Pontifical Catholic University of Rio de Janeiro in Brazil)诞生了一门编程语言，发明者是该校的三位研究人员，他们给这门语言取了个浪漫的名字——Lua，在葡萄牙语里代表美丽的月亮。事实证明她没有糟蹋这个优美的单词，Lua 语言正如它名字所预示的那样成长为一门简洁、优雅且富有乐趣的语言。

Lua 从一开始就是作为一门方便嵌入(到其它应用程序)并可扩展的轻量级脚本语言来设计的，因此她一直遵从着简单、小巧、可移植、快速的原则，官方实现完全采用 ANSI C 编写，能以 C 程序库的形式嵌入到宿主程序中。标准 Lua 5.1 解释器采用的是著名的 MIT 许可协议。正由于上述特点，所以 Lua 在游戏开发、机器人控制、分布式应用、图像处理、生物信息学等各种各样的领域中得到了越来越广泛的应用。其中尤以游戏开发为最，许多著名的游戏，比如 Escape from Monkey Island、World of Warcraft、大话西游，都采用了 Lua 来配合引擎完成数据描述、配置管理和逻辑控制等任务。即使像 Redis 这样中性的内存键值数据库也提供了内嵌用户 Lua 脚本的官方支持。

作为一门过程型动态语言，Lua 有着如下的特性：

1. 变量名没有类型，值才有类型，变量名在运行时可与任何类型的值绑定;
2. 语言只提供唯一一种数据结构，称为表(table)，它混合了数组、哈希，可以用任何类型的值作为 key 和 value。提供了一致且富有表达力的表构造语法，使得 Lua 很适合描述复杂的数据;
3. 函数是一等类型，支持匿名函数和正则尾递归(proper tail recursion);
4. 支持词法定界(lexical scoping)和闭包(closure);
5. 提供 thread 类型和结构化的协程(coroutine)机制，在此基础上可方便实现协作式多任务;
6. 运行期能编译字符串形式的程序文本并载入虚拟机执行;
7. 通过元表(metatable)和元方法(metamethod)提供动态元机制(dynamic meta-mechanism)，从而允许程序运行时根据需要改变或扩充语法设施的内定语义;
8. 能方便地利用表和动态元机制实现基于原型(prototype-based)的面向对象模型;
9. 从 5.1 版开始提供了完善的模块机制，从而更好地支持开发大型的应用程序;

Lua 的语法类似 PASCAL 和 Modula 但更加简洁，所有的语法产生式规则(EBNF)不过才 60 几个。熟悉 C 和 PASCAL 的程序员一般只需半个小时便可将其完全掌握。而在语义上 Lua 则与 Scheme 极为相似，她们完全共享上述的 1 、3 、4 、6 点特性，Scheme 的 continuation 与协程也基本相同只是自由度更高。最引人注目的是，两种语言都只提供唯一一种数据结构：Lua 的表和 Scheme 的列表(list)。正因为如此，有人甚至称 Lua 为“只用表的 Scheme”。

## Demo

### 初始化
```lua
adapter = require("Adapter")	-- 加载Adapter库
cmsis_dap = require("CMSIS-DAP") -- 加载CMSIS-DAP库
AdapterObj = cmsis_dap.New() -- 创建新的CMSIS-DAP Adapter对象
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
cmsis_dap.Connect(AdapterObj, vid_pids, nil)
-- 初始化Adapter对象
AdapterObj:Init()
-- 配置CMSIS-DAP Transfer参数
cmsis_dap.TransferConfigure(AdapterObj, 5, 5, 5)
-- SWD参数
cmsis_dap.swdConfigure(AdapterObj, 0)
-- 点亮连接状态指示灯
AdapterObj:SetStatus(adapter.STATUS_CONNECTED)
AdapterObj:SetClock(500000)	-- 500KHz
-- 选择传输模式
if AdapterObj:HaveTransType(adapter.SWD) then	-- 优先选择SWD协议
	AdapterObj:TransType(adapter.SWD)
else
	AdapterObj:TransType(adapter.JTAG)
end
AdapterObj:SetStatus(adapter.STATUS_RUNING)	-- 点亮仿真器的RUN状态灯

-- 读取JTAG Pins
local pins_data = AdapterObj:jtagPins(0, 0, 0)
print("JTAG Pins: TCK:" .. ((pins_data & adapter.PIN_SWCLK_TCK) >> adapter.PIN_SWCLK_TCK)
	.. " TMS:" .. ((pins_data & (1 << adapter.PIN_SWDIO_TMS)) >> adapter.PIN_SWDIO_TMS)
	.. " TDI:" .. ((pins_data & (1 << adapter.PIN_TDI)) >> adapter.PIN_TDI)
	.. " TDO:" .. ((pins_data & (1 << adapter.PIN_TDO)) >> adapter.PIN_TDO)
	.. " nTRST:" .. ((pins_data & (1 << adapter.PIN_nTRST)) >> adapter.PIN_nTRST)
	.. " nRESET:" .. ((pins_data & (1 << adapter.PIN_nRESET)) >> adapter.PIN_nRESET))
```

### 简单示例——打印STM32F407的ROM前64个字：
```lua
dap = require("DAP")
dofile("scripts/adapters/cmsis_dap.lua")    -- 获得CMSIS-DAP的AdapterObj对象
if AdapterObj:TransType() == adapter.JTAG then
	-- JTAG 原生方式读取IDCODE
	AdapterObj:jtagStatusChange(adapter.TAP_RESET)	-- TAP到RESET状态，默认连接IDCODE扫描链
	AdapterObj:jtagStatusChange(adapter.TAP_DR_SHIFT)	-- TAP到DR-Shift状态，读取idcode
	local raw_idcodes = AdapterObj:jtagExchangeIO(string.pack("I4I4I4I4", 0, 0, 0, 0), 128)
	-- string.unpack 额外返回一个参数是next position，下面这个返回五个参数
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

local rom_table_base = dap.GetROMTable(AdapterObj)
print(string.format("ROM Table Base: 0x%08X", rom_table_base))
-- 打印ROM开头的64个32位字
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
print(string.format("CTRL/STAT:0x%08X", dap.Reg(AdapterObj, dap.DP_REG_CTRL_STAT)))
print("Hello World")
AdapterObj:SetStatus(adapter.STATUS_IDLE)	-- 熄灭RUN状态灯
```
输出如下：
```
$ sudo ./smartocd -f ./scripts/target/stm32f4x.lua -e
   _____                      __  ____  __________ 
  / ___/____ ___  ____ ______/ /_/ __ \/ ____/ __ \
  \__ \/ __ `__ \/ __ `/ ___/ __/ / / / /   / / / /
 ___/ / / / / / / /_/ / /  / /_/ /_/ / /___/ /_/ / 
/____/_/ /_/ /_/\__,_/_/   \__/\____/\____/_____/  
 * SmartOCD v1.1.0 By: Virus.V <virusv@live.com>
 * Complile time: 2018-08-24T22:20:39+0800
 * Github: https://github.com/Virus-V/SmartOCD
12:11:52 INFO  cmsis-dap.c:117: Successful connection vid: 0xc251, pid: 0xf001 usb device.
12:11:52 INFO  usb.c:291: Currently it is configuration indexed by:0, bConfigurationValue is 1.
12:11:52 INFO  cmsis-dap.c:189: CMSIS-DAP Init.
12:11:52 INFO  cmsis-dap.c:212: CMSIS-DAP the maximum Packet Size is 1024.
12:11:52 INFO  cmsis-dap.c:223: CMSIS-DAP FW Version is 1.100000.
12:11:52 INFO  cmsis-dap.c:229: CMSIS-DAP the maximum Packet Count is 4.
12:11:52 INFO  cmsis-dap.c:262: CMSIS-DAP Capabilities 0x13.
12:11:52 INFO  cmsis-dap.c:1404: Set Transfer Configure: Idle:5, Wait:5, Match:5.
12:11:52 INFO  cmsis-dap.c:1421: SWD: Turnaround clock period: 1 clock cycles. DataPhase: No.
12:11:52 INFO  cmsis-dap.c:518: CMSIS-DAP Clock Frequency: 500.00KHz.
12:11:52 INFO  cmsis-dap.c:380: Switch to SWD mode.
JTAG Pins: TCK:0 TMS:0 TDI:0 TDO:1 nTRST:0 nRESET:1
12:11:52 INFO  DAP.c:30: DAP DPIDR:0x2BA01477.
12:11:52 INFO  DAP.c:184: Probe AP 0, AP_IDR: 0x24770011, CTRL/STAT: 0xF0000040.
Support Packed Transfer.
Support Less Word Transfer.
ROM Table Base: 0xE00FF003
[0x08000000]:0x2001FFFF.
[0x08000000]:0xFFFF.
[0x08000002]:0x2001.
[0x08000000]:0xFF.
[0x08000001]:0xFF.
[0x08000002]:0x01.
[0x08000003]:0x20.
[0x08000004]:0x08001965.
[0x08000004]:0x1965.
[0x08000006]:0x0800.
[0x08000004]:0x65.
[0x08000005]:0x19.
[0x08000006]:0x00.
[0x08000007]:0x08.
[0x08000008]:0x08001505.
[0x08000008]:0x1505.
[0x0800000A]:0x0800.
[0x08000008]:0x05.
[0x08000009]:0x15.
[0x0800000A]:0x00.
[0x0800000B]:0x08.
[0x0800000C]:0x08001511.
[0x0800000C]:0x1511.
[0x0800000E]:0x0800.
[0x0800000C]:0x11.
[0x0800000D]:0x15.
[0x0800000E]:0x00.
[0x0800000F]:0x08.
[0x08000010]:0x08001519.
[0x08000010]:0x1519.
[0x08000012]:0x0800.
[0x08000010]:0x19.
[0x08000011]:0x15.
[0x08000012]:0x00.
[0x08000013]:0x08.
[0x08000014]:0x08001521.
[0x08000014]:0x1521.
[0x08000016]:0x0800.
[0x08000014]:0x21.
[0x08000015]:0x15.
[0x08000016]:0x00.
[0x08000017]:0x08.
[0x08000018]:0x08001529.
[0x08000018]:0x1529.
[0x0800001A]:0x0800.
[0x08000018]:0x29.
[0x08000019]:0x15.
[0x0800001A]:0x00.
[0x0800001B]:0x08.
[0x0800001C]:0x00000000.
[0x0800001C]:0x0000.
[0x0800001E]:0x0000.
[0x0800001C]:0x00.
[0x0800001D]:0x00.
[0x0800001E]:0x00.
[0x0800001F]:0x00.
[0x08000020]:0x00000000.
[0x08000020]:0x0000.
[0x08000022]:0x0000.
[0x08000020]:0x00.
[0x08000021]:0x00.
[0x08000022]:0x00.
[0x08000023]:0x00.
[0x08000024]:0x00000000.
[0x08000024]:0x0000.
[0x08000026]:0x0000.
[0x08000024]:0x00.
[0x08000025]:0x00.
[0x08000026]:0x00.
[0x08000027]:0x00.
[0x08000028]:0x00000000.
[0x08000028]:0x0000.
[0x0800002A]:0x0000.
[0x08000028]:0x00.
[0x08000029]:0x00.
[0x0800002A]:0x00.
[0x0800002B]:0x00.
[0x0800002C]:0x080019A9.
[0x0800002C]:0x19A9.
[0x0800002E]:0x0800.
[0x0800002C]:0xA9.
[0x0800002D]:0x19.
[0x0800002E]:0x00.
[0x0800002F]:0x08.
[0x08000030]:0x08001531.
[0x08000030]:0x1531.
[0x08000032]:0x0800.
[0x08000030]:0x31.
[0x08000031]:0x15.
[0x08000032]:0x00.
[0x08000033]:0x08.
[0x08000034]:0x00000000.
[0x08000034]:0x0000.
[0x08000036]:0x0000.
[0x08000034]:0x00.
[0x08000035]:0x00.
[0x08000036]:0x00.
[0x08000037]:0x00.
[0x08000038]:0x080019A9.
[0x08000038]:0x19A9.
[0x0800003A]:0x0800.
[0x08000038]:0xA9.
[0x08000039]:0x19.
[0x0800003A]:0x00.
[0x0800003B]:0x08.
[0x0800003C]:0x080019A9.
[0x0800003C]:0x19A9.
[0x0800003E]:0x0800.
[0x0800003C]:0xA9.
[0x0800003D]:0x19.
[0x0800003E]:0x00.
[0x0800003F]:0x08.
[0x08000040]:0x080019A9.
[0x08000040]:0x19A9.
[0x08000042]:0x0800.
[0x08000040]:0xA9.
[0x08000041]:0x19.
[0x08000042]:0x00.
[0x08000043]:0x08.
Hello World
CTRL/STAT:0xF0000040
Hello World
```

## Package
SmartOCD的API以Package的方式封装到Lua中。
### Adapter
#### 介绍
Adapter是根据仿真器的基本功能和操作抽象出来的。通过Adapter对象，可以实现控制JTAG、DAP等基本操作。
每打开一个仿真器都会返回一个Adapter对象实例。例如 `AdapterObj = cmsis_dap.New()` 就是创建一个CMSIS-DAP的Adapter实例。

在介绍常量和方法之前，假设Lua代码中已经引入Adapter包并赋值给全局变量adapter：
```lua
adapter = require("Adapter")	-- 加载Adapter Package
```

#### 常量

传输模式：
- JTAG
- SWD

JTAG引脚的二进制位索引：
- PIN_SWCLK_TCK
- PIN_SWDIO_TMS
- PIN_TDI
- PIN_TDO
- PIN_nTRST
- PIN_nRESET

TAP状态：
- TAP_RESET
- TAP_IDLE
- TAP_DR_SELECT
- TAP_DR_CAPTURE
- TAP_DR_SHIFT
- TAP_DR_EXIT1
- TAP_DR_PAUSE
- TAP_DR_EXIT2
- TAP_DR_UPDATE
- TAP_IR_SELECT
- TAP_IR_CAPTURE
- TAP_IR_SHIFT
- TAP_IR_EXIT1
- TAP_IR_PAUSE
- TAP_IR_EXIT2
- TAP_IR_UPDATE

仿真器状态：
- STATUS_CONNECTED
- STATUS_DISCONNECT
- STATUS_RUNING
- STATUS_IDLE

#### 方法
- Init(*self*)  
  作用：初始化Adapter对象对应的仿真器  
  参数：  
  1. Adapter对象
  
  返回值：无  
  示例：
  ```lua
  -- 初始化Adapter对象
  AdapterObj:Init()
  ```
  ----
- Deinit(*self*)
  作用：执行Adapter对象对应的仿真器的反初始化动作  
  参数：  
  1. Adapter对象
  
  返回值：无  
  示例：
  ```lua
  -- 初始化Adapter对象
  AdapterObj:Deinit()
  ```
  ----
- SetStatus(*self, status*)  
  作用：设置仿真器的状态指示灯（如果有的话）  
  参数：  
  1. Adapter对象
  2. Number：Status状态  
   参考Adapter常量 **仿真器状态** 部分
  
  返回值：无  
  示例：
  ```lua
  -- 点亮连接状态指示灯
  AdapterObj:SetStatus(adapter.STATUS_CONNECTED)
  -- 点亮运行状态指示灯
  AdapterObj:SetStatus(adapter.STATUS_RUNING)
  -- 熄灭运行状态指示灯
  AdapterObj:SetStatus(adapter.STATUS_IDLE)
  -- 熄灭连接状态指示灯
  AdapterObj:SetStatus(adapter.STATUS_DISCONNECT)
  ```
  ----
- SetClock(*self, clock*)  
  作用：设置仿真器JTAG/SWD同步时钟频率  
  参数：  
  1. Adapter对象
  2. Number：Clock频率，单位Hz
  
  返回值：无  
  示例：
  ```lua
  -- 设置仿真器通信时钟速率
  AdapterObj:SetClock(500000)	-- 500KHz
  ```
  ----
- TransType(*self [, setType]*)  
  作用：设置或读取仿真器当前传输模式（必须仿真器支持此模式）  
  参数：  
  1. Adapter对象
  2. Number：传输模式（可选）  
   参考Adapter常量 **传输模式** 部分
  
  返回值：无 或者  
  1. Number：仿真器当前传输模式  
   
  示例：
  ```lua
  -- 设置当前仿真器为SWD传输模式
  AdapterObj:TransType(adapter.SWD)
  -- 设置当前仿真器为JTAG模式
  AdapterObj:TransType(adapter.JTAG)
  -- 读取当前仿真器传输模式
  local currTransType = AdapterObj:TransType()
  ```
  ----
- HaveTransType(*self, type*)  
  作用：检查仿真器是否支持某一传输模式  
  参数：  
  1. Adapter对象
  2. Number：传输模式  
   参考Adapter常量 **传输模式** 部分
  
  返回值：  
  1. Boolean：true=支持；false=不支持  

  示例：
  ```lua
  -- 优先选择SWD传输模式
  if AdapterObj:HaveTransType(adapter.SWD) then
      AdapterObj:TransType(adapter.SWD)
  else
      AdapterObj:TransType(adapter.JTAG)
  end
  ```
  ----
- Reset(*self [, hard [, srst [, pinWait]]]*)  
  作用：对目标芯片进行复位操作  
  参数：  
  1. Adapter对象
  2. Boolean：hard 是否进行硬复位  
   所谓硬复位，是指让nTRST引脚输出有效信号。该参数只在JTAG传输模式下有效。
  3. Boolean：srst 系统复位  
   系统复位表示是否同时让nRESET引脚输出有效信号。该参数只在JTAG传输模式下和 hard 参数为 true 时有效。
  4. Number：死区时间，单位µs
   当nTRST和nRESET引脚为开漏模式时，等待引脚电平有效的时间。SWD传输模式不使用此参数。
  
  返回值：无  
  示例：
  ```lua
  -- 软复位：JTAG的TMS引脚连续5个时钟的高电平；SWD进行line reset
  AdapterObj:Reset()
  AdapterObj:Reset(false)
  -- 硬复位：JTAG的nTRST输出有效；SWD进行line reset
  AdapterObj:Reset(true)
  -- 硬复位同时系统复位：JTAG的nTRST和nRESET输出有效；SWD进行line reset
  AdapterObj:Reset(true, true)
  -- 硬复位、系统复位同时带有死区时间控制
  AdapterObj:Reset(true, true, 300)
  ```
  ----
- jtagStatusChange(*self, newStatus*)  
  作用：在JTAG模式下，切换TAP状态机的状态  
  参数：  
  1. Adapter对象
  2. Number：newStatus 要切换到的TAP状态
   参考Adapter常量 **TAP状态** 部分
  
  返回值：无  
  示例：
  ```lua
  -- TAP到RESET状态
  AdapterObj:jtagStatusChange(adapter.TAP_RESET)
  -- TAP到DR-Shift状态
  AdapterObj:jtagStatusChange(adapter.TAP_DR_SHIFT)
  -- 执行动作
  AdapterObj:jtagExecuteCmd()
  ```
  ----
- jtagExchangeIO(*self, data, bitCnt*)  
  作用：交换TDI和TDO之间的数据。将data参数的数据发送到TDI，然后捕获TDO的数据作为函数返回值。注意，此方法会默认调用jtagExecuteCmd()方法！  
  参数：  
  1. Adapter对象
  2. String：data 要发送的数据
  3. Number：bitCnt 要发送多少个二进制位，不得大于data参数的数据实际长度
  
  返回值：  
  1. String：捕获的TDO数据
  示例：
  ```lua
  -- TAP到RESET状态，默认连接IDCODE扫描链
  AdapterObj:jtagStatusChange(adapter.TAP_RESET)
  -- TAP到DR-Shift状态，读取idcode
  AdapterObj:jtagStatusChange(adapter.TAP_DR_SHIFT)
  local raw_idcodes = AdapterObj:jtagExchangeIO(string.pack("I4I4", 0, 0), 64)
  -- string.unpack 额外返回一个参数是next position
  local idcodes = {string.unpack("I4I4", raw_idcodes)}
  for key=1, #idcodes-1 do
  	print(string.format("TAP #%d : 0x%08X", key-1, idcodes[key]))
  end
  ```
  ----
- jtagIdleWait(*self, cycles*)  
  作用：进入TAP的Run-Test/Idle状态等待cycle个周期。此函数常用来等待耗时操作的完成。  
  参数：  
  1. Adapter对象
  2. Number：cycle 等待的周期数
  
  返回值：无  
  示例：
  ```lua
  -- TAP进入Run-Test/Idle等待50个TCLK时钟周期
  AdapterObj:jtagIdleWait(50)
  ```
  ----
- jtagExecuteCmd(*self*)  
  作用：执行前面的JTAG操作。  
  参数：  
  1. Adapter对象
  
  返回值：无  
  示例：
  ```lua
  -- TAP到RESET状态
  AdapterObj:jtagStatusChange(adapter.TAP_RESET)
  -- TAP到DR-Shift状态
  AdapterObj:jtagStatusChange(adapter.TAP_DR_SHIFT)
  -- TAP进入Run-Test/Idle等待50个TCLK时钟周期
  AdapterObj:jtagIdleWait(50)
  -- 执行前面三个JTAG动作
  AdapterObj:jtagExecuteCmd()
  ```
  ----
- jtagCleanCmd(*self*)  
  作用：清除尚未执行JTAG操作  
  参数：  
  1. Adapter对象
  
  返回值：无  
  示例：
  ```lua
  -- TAP到RESET状态
  AdapterObj:jtagStatusChange(adapter.TAP_RESET)
  -- TAP到DR-Shift状态
  AdapterObj:jtagStatusChange(adapter.TAP_DR_SHIFT)
  -- TAP进入Run-Test/Idle等待50个TCLK时钟周期
  AdapterObj:jtagIdleWait(50)
  -- 取消前面三个JTAG动作
  AdapterObj:jtagCleanCmd()
  -- 这一步没有任何JTAG操作被执行，函数将直接返回
  AdapterObj:jtagExecuteCmd()
  ```
  ----
- jtagPins(*self, pinData, pinMask, pinWait*)
  作用：设置JTAG引脚电平并返回设置之后的引脚电平状态  
  参数：  
  1. Adapter对象
  2. Number：pinData 引脚数据，LSB有效
  3. Number：pinMask 引脚掩码，LSB有效
  4. Number：pinWait 死区时间，单位µs
  
  返回值：  
  1. Number：pinData JTAG全部引脚最新的状态，也就时执行设置JTAG引脚动作之后的JTAG引脚状态，LSB有效
  示例：
  ```lua
  -- 读取JTAG Pins
  local pins_data = AdapterObj:jtagPins(0, 0, 0)
  print("JTAG Pins: TCK:" .. ((pins_data & adapter.PIN_SWCLK_TCK) >> adapter.PIN_SWCLK_TCK)
	.. " TMS:" .. ((pins_data & (1 << adapter.PIN_SWDIO_TMS)) >> adapter.PIN_SWDIO_TMS)
	.. " TDI:" .. ((pins_data & (1 << adapter.PIN_TDI)) >> adapter.PIN_TDI)
	.. " TDO:" .. ((pins_data & (1 << adapter.PIN_TDO)) >> adapter.PIN_TDO)
	.. " nTRST:" .. ((pins_data & (1 << adapter.PIN_nTRST)) >> adapter.PIN_nTRST)
	.. " nRESET:" .. ((pins_data & (1 << adapter.PIN_nRESET)) >> adapter.PIN_nRESET))
  -- 设置JTAG的TMS TDI引脚电平，并返回所有引脚最新的状态
  local pins_data = AdapterObj:jtagPins((1 << adapter.PIN_SWDIO_TMS) | (0 << adapter.PIN_TDI), adapter.PIN_SWDIO_TMS | adapter.PIN_TDI, 0)
  print(string.format("JTAG Pins: 0x%X.", pins_data))
  ```
  ----
- jtagSetTAPInfo(*self, tapInfo*)  
  作用：JTAG较高级用法。使用该函数声明JTAG扫描链上有多少个TAP，而且指定每个TAP的IR长度，
  为 `jtagWriteTAPIR` 和 `jtagExchangeTAPDR` 两函数提供参考信息。
  参数：  
  1. Adapter对象
  2. Array：tapInfo TAP信息数组
  
  返回值：无  
  示例：
  ```lua
  -- 加载DAP Package
  dap = require("DAP")
  -- 设置JTAG扫描链中有两个TAP，IR长度分别是4,5
  AdapterObj:jtagSetTAPInfo{4,5}
  -- 写入JTAG扫描链中位于0号索引的TAP的IR寄存器，IR长度为4
  AdapterObj:jtagWriteTAPIR(0, dap.JTAG_IDCODE)
  -- 交换JTAG扫描链中位于0号索引的TAP的DR寄存器，DR长度为32，IDCODE寄存器
  local arm_idcode = AdapterObj:jtagExchangeTAPDR(0, "\000\000\000\000", 32)
  -- 打印IDCODE
  print(string.format("%x", string.unpack("I4", arm_idcode)))
  ```
  ----
- jtagWriteTAPIR(*self, tapIdx, irData*)  
  作用：将 _irData_ 写入JTAG扫描链中位于 _tapIdx_ 的TAP的IR寄存器  
  参数：  
  1. Adapter对象
  2. Number：tapIdx TAP在JTAG扫描链中的位置，从0开始
  3. Number：irData 要写入IR寄存器的数据，将要写入的二进制位个数由 `jtagSetTAPInfo` 的 _tapInfo_ 参数指定。
  
  返回值：无  
  示例：参考 `jtagSetTAPInfo` 方法的示例部分  
  ____
- jtagExchangeTAPDR(*self, tapIdx, data, bitCnt*)  
  作用：交换JTAG扫描链中位于 _tapIdx_ 的TAP的DR寄存器  
  参数：  
  1. Adapter对象
  2. Number：tapIdx TAP在JTAG扫描链中的位置，从0开始
  3. Number：data 要交换进去的DR的数据
  4. Number：bitCnt 指定DR寄存器的二进制位个数，不得大于 _data_ 数据的长度
  
  返回值：  
  1. String：交换出来的DR寄存器数据  

  示例：参考 `jtagSetTAPInfo` 方法的示例部分

----

### DAP
#### 介绍
#### 常量
#### 方法

----

### CMSIS-DAP


## 开发人员手册
### 开发计划
### 开发指引
### 核心设计

## 版本历史
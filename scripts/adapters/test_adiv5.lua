--[[
    CMSIS-DAP测试程序
]]
adiv5 = require("ADIv5")
dofile("scripts/adapters/cmsis_dap.lua")    -- 获得CMSIS-DAP对象
-- 切换到SWD模式
cmObj:TransferMode(adapter.MODE_SWD)
print("Change transfer mode to SWD")

dapSkill = cmObj:GetSkill(adapter.SKILL_DAP);

cmObj = nil

-- 读取DP IDR
local dpidr = dapSkill:DapSingleRead(adapter.REG_DP, 0)
print(string.format("DPIDR: 0x%X", dpidr)) -- 0x2BA01477

-- 创建ADIv5 DAP对象
dap = adiv5.Create(dapSkill)   -- 接收Adapter类型的参数
apAHB = dap:FindAccessPort(adiv5.AP_Memory, adiv5.Bus_AMBA_AHB)
-- 读ROM Table
print(string.format("ROM Table: 0x%X", apAHB:RomTable()))
print("-------Read TEST--------")
-- 读8位
print(string.format("Byte 0x08000000: 0x%X", apAHB:Memory8(0x08000000)))
print(string.format("Byte 0x08000001: 0x%X", apAHB:Memory8(0x08000001)))
print(string.format("Byte 0x08000002: 0x%X", apAHB:Memory8(0x08000002)))
print(string.format("Byte 0x08000003: 0x%X", apAHB:Memory8(0x08000003)))
-- 读16位
print(string.format("Halfword 0x08000000: 0x%X", apAHB:Memory16(0x08000000)))
print(string.format("Halfword 0x08000002: 0x%X", apAHB:Memory16(0x08000002)))
-- 读32位
print(string.format("Word 0x08000000: 0x%X", apAHB:Memory32(0x08000000)))
print("-------Write TEST--------")
-- 写8位
apAHB:Memory8(0x20000000, 0xa5)
print(string.format("Byte 0x20000000: 0x%X", apAHB:Memory8(0x20000000)))
-- 写16位
apAHB:Memory16(0x20000002, 0xbeef)
print(string.format("Halfword 0x20000002: 0x%X", apAHB:Memory16(0x20000002)))
-- 写32位
apAHB:Memory32(0x20000004, 0x5a5aa5a5)
print(string.format("Word 0x20000004: 0x%X", apAHB:Memory32(0x20000004)))
-- 控制GPIOE
RCC_BASE = 0x40000000+0x20000+0x3800
GPIOE_BASE = 0x40000000+0x20000+0x1000

-- 打开GPIOE的时钟
RCC_AHB1ENR = apAHB:Memory32(RCC_BASE+0x30)
print(string.format("RCC_AHB1ENR: 0x%X", RCC_AHB1ENR))
RCC_AHB1ENR = RCC_AHB1ENR | 0x10    -- 设置GPIOEN位
apAHB:Memory32(RCC_BASE+0x30, RCC_AHB1ENR)

print(string.format("RCC_BASE: 0x%X", RCC_BASE))
print(string.format("GPIOE_BASE: 0x%X", GPIOE_BASE))

-- 初始化GPIOE
apAHB:Memory32(GPIOE_BASE, 0x150)   -- MODE
apAHB:Memory32(GPIOE_BASE+0x4, 0x0) -- OTYPE
apAHB:Memory32(GPIOE_BASE+0x8, 0x0) -- OSPEED
apAHB:Memory32(GPIOE_BASE+0xC, 0x2A0)   -- PUPDR

-- PE2 = 1
apAHB:Memory32(GPIOE_BASE+0x18, 0x4 << 16)
apAHB:Memory32(GPIOE_BASE+0x18, 0x8 << 16)
apAHB:Memory32(GPIOE_BASE+0x18, 0x10 << 16)

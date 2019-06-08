--[[
    CMSIS-DAP测试程序
]]
adiv5 = require("ADIv5")
dofile("scripts/adapters/cmsis_dap.lua")    -- 获得CMSIS-DAP对象
-- 切换到SWD模式
cmObj:TransferMode(adapter.SWD)
print("Change transfer mode to SWD")
-- 创建ADIv5 DAP对象
dap = adiv5.Create(cmObj)   -- 接收Adapter类型的参数
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
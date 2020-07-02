--[[
    CMSIS-DAP测试程序
]]

dofile("scripts/adapters/cmsis_dap.lua")    -- 获得CMSIS-DAP对象
-- 系统复位
cmObj:Reset(adapter.RESET_SYSTEM)
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

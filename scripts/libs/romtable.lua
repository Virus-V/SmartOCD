--[[
    遍历ROM Table
]]
local dap_infos = {
    { 0x4BB, 0x000, "Cortex-M3 SCS",              "(System Control Space)", },
	{ 0x4BB, 0x001, "Cortex-M3 ITM",              "(Instrumentation Trace Module)", },
	{ 0x4BB, 0x002, "Cortex-M3 DWT",              "(Data Watchpoint and Trace)", },
	{ 0x4BB, 0x003, "Cortex-M3 FPB",              "(Flash Patch and Breakpoint)", },
	{ 0x4BB, 0x008, "Cortex-M0 SCS",              "(System Control Space)", },
	{ 0x4BB, 0x00a, "Cortex-M0 DWT",              "(Data Watchpoint and Trace)", },
	{ 0x4BB, 0x00b, "Cortex-M0 BPU",              "(Breakpoint Unit)", },
	{ 0x4BB, 0x00c, "Cortex-M4 SCS",              "(System Control Space)", },
	{ 0x4BB, 0x00d, "CoreSight ETM11",            "(Embedded Trace)", },
	{ 0x4BB, 0x00e, "Cortex-M7 FPB",              "(Flash Patch and Breakpoint)", },
	{ 0x4BB, 0x490, "Cortex-A15 GIC",             "(Generic Interrupt Controller)", },
	{ 0x4BB, 0x4a1, "Cortex-A53 ROM",             "(v8 Memory Map ROM Table)", },
	{ 0x4BB, 0x4a2, "Cortex-A57 ROM",             "(ROM Table)", },
	{ 0x4BB, 0x4a3, "Cortex-A53 ROM",             "(v7 Memory Map ROM Table)", },
	{ 0x4BB, 0x4a4, "Cortex-A72 ROM",             "(ROM Table)", },
	{ 0x4BB, 0x4a9, "Cortex-A9 ROM",              "(ROM Table)", },
	{ 0x4BB, 0x4af, "Cortex-A15 ROM",             "(ROM Table)", },
	{ 0x4BB, 0x4c0, "Cortex-M0+ ROM",             "(ROM Table)", },
	{ 0x4BB, 0x4c3, "Cortex-M3 ROM",              "(ROM Table)", },
	{ 0x4BB, 0x4c4, "Cortex-M4 ROM",              "(ROM Table)", },
	{ 0x4BB, 0x4c7, "Cortex-M7 PPB ROM",          "(Private Peripheral Bus ROM Table)", },
	{ 0x4BB, 0x4c8, "Cortex-M7 ROM",              "(ROM Table)", },
	{ 0x4BB, 0x4b5, "Cortex-R5 ROM",              "(ROM Table)", },
	{ 0x4BB, 0x470, "Cortex-M1 ROM",              "(ROM Table)", },
	{ 0x4BB, 0x471, "Cortex-M0 ROM",              "(ROM Table)", },
	{ 0x4BB, 0x906, "CoreSight CTI",              "(Cross Trigger)", },
	{ 0x4BB, 0x907, "CoreSight ETB",              "(Trace Buffer)", },
	{ 0x4BB, 0x908, "CoreSight CSTF",             "(Trace Funnel)", },
	{ 0x4BB, 0x909, "CoreSight ATBR",             "(Advanced Trace Bus Replicator)", },
	{ 0x4BB, 0x910, "CoreSight ETM9",             "(Embedded Trace)", },
	{ 0x4BB, 0x912, "CoreSight TPIU",             "(Trace Port Interface Unit)", },
	{ 0x4BB, 0x913, "CoreSight ITM",              "(Instrumentation Trace Macrocell)", },
	{ 0x4BB, 0x914, "CoreSight SWO",              "(Single Wire Output)", },
	{ 0x4BB, 0x917, "CoreSight HTM",              "(AHB Trace Macrocell)", },
	{ 0x4BB, 0x920, "CoreSight ETM11",            "(Embedded Trace)", },
	{ 0x4BB, 0x921, "Cortex-A8 ETM",              "(Embedded Trace)", },
	{ 0x4BB, 0x922, "Cortex-A8 CTI",              "(Cross Trigger)", },
	{ 0x4BB, 0x923, "Cortex-M3 TPIU",             "(Trace Port Interface Unit)", },
	{ 0x4BB, 0x924, "Cortex-M3 ETM",              "(Embedded Trace)", },
	{ 0x4BB, 0x925, "Cortex-M4 ETM",              "(Embedded Trace)", },
	{ 0x4BB, 0x930, "Cortex-R4 ETM",              "(Embedded Trace)", },
	{ 0x4BB, 0x931, "Cortex-R5 ETM",              "(Embedded Trace)", },
	{ 0x4BB, 0x932, "CoreSight MTB-M0+",          "(Micro Trace Buffer)", },
	{ 0x4BB, 0x941, "CoreSight TPIU-Lite",        "(Trace Port Interface Unit)", },
	{ 0x4BB, 0x950, "Cortex-A9 PTM",              "(Program Trace Macrocell)", },
	{ 0x4BB, 0x955, "Cortex-A5 ETM",              "(Embedded Trace)", },
	{ 0x4BB, 0x95a, "Cortex-A72 ETM",             "(Embedded Trace)", },
	{ 0x4BB, 0x95b, "Cortex-A17 PTM",             "(Program Trace Macrocell)", },
	{ 0x4BB, 0x95d, "Cortex-A53 ETM",             "(Embedded Trace)", },
	{ 0x4BB, 0x95e, "Cortex-A57 ETM",             "(Embedded Trace)", },
	{ 0x4BB, 0x95f, "Cortex-A15 PTM",             "(Program Trace Macrocell)", },
	{ 0x4BB, 0x961, "CoreSight TMC",              "(Trace Memory Controller)", },
	{ 0x4BB, 0x962, "CoreSight STM",              "(System Trace Macrocell)", },
	{ 0x4BB, 0x975, "Cortex-M7 ETM",              "(Embedded Trace)", },
	{ 0x4BB, 0x9a0, "CoreSight PMU",              "(Performance Monitoring Unit)", },
	{ 0x4BB, 0x9a1, "Cortex-M4 TPIU",             "(Trace Port Interface Unit)", },
	{ 0x4BB, 0x9a4, "CoreSight GPR",              "(Granular Power Requester)", },
	{ 0x4BB, 0x9a5, "Cortex-A5 PMU",              "(Performance Monitor Unit)", },
	{ 0x4BB, 0x9a7, "Cortex-A7 PMU",              "(Performance Monitor Unit)", },
	{ 0x4BB, 0x9a8, "Cortex-A53 CTI",             "(Cross Trigger)", },
	{ 0x4BB, 0x9a9, "Cortex-M7 TPIU",             "(Trace Port Interface Unit)", },
	{ 0x4BB, 0x9ae, "Cortex-A17 PMU",             "(Performance Monitor Unit)", },
	{ 0x4BB, 0x9af, "Cortex-A15 PMU",             "(Performance Monitor Unit)", },
	{ 0x4BB, 0x9b7, "Cortex-R7 PMU",              "(Performance Monitor Unit)", },
	{ 0x4BB, 0x9d3, "Cortex-A53 PMU",             "(Performance Monitor Unit)", },
	{ 0x4BB, 0x9d7, "Cortex-A57 PMU",             "(Performance Monitor Unit)", },
	{ 0x4BB, 0x9d8, "Cortex-A72 PMU",             "(Performance Monitor Unit)", },
	{ 0x4BB, 0xc05, "Cortex-A5 Debug",            "(Debug Unit)", },
	{ 0x4BB, 0xc07, "Cortex-A7 Debug",            "(Debug Unit)", },
	{ 0x4BB, 0xc08, "Cortex-A8 Debug",            "(Debug Unit)", },
	{ 0x4BB, 0xc09, "Cortex-A9 Debug",            "(Debug Unit)", },
	{ 0x4BB, 0xc0e, "Cortex-A17 Debug",           "(Debug Unit)", },
	{ 0x4BB, 0xc0f, "Cortex-A15 Debug",           "(Debug Unit)", },
	{ 0x4BB, 0xc14, "Cortex-R4 Debug",            "(Debug Unit)", },
	{ 0x4BB, 0xc15, "Cortex-R5 Debug",            "(Debug Unit)", },
	{ 0x4BB, 0xc17, "Cortex-R7 Debug",            "(Debug Unit)", },
	{ 0x4BB, 0xd03, "Cortex-A53 Debug",           "(Debug Unit)", },
	{ 0x4BB, 0xd07, "Cortex-A57 Debug",           "(Debug Unit)", },
	{ 0x4BB, 0xd08, "Cortex-A72 Debug",           "(Debug Unit)", },
	{ 0x097, 0x9af, "MSP432 ROM",                 "(ROM Table)" },
	{ 0x09f, 0xcd0, "Atmel CPU with DSU",         "(CPU)" },
	{ 0x0c1, 0x1db, "XMC4500 ROM",                "(ROM Table)" },
	{ 0x0c1, 0x1df, "XMC4700/4800 ROM",           "(ROM Table)" },
	{ 0x0c1, 0x1ed, "XMC1000 ROM",                "(ROM Table)" },
	{ 0x0E5, 0x000, "SHARC+/Blackfin+",           "", },
	{ 0x0F0, 0x440, "Qualcomm QDSS Component v1", "(Qualcomm Designed CoreSight Component v1)", },
	{ 0x3eb, 0x181, "Tegra 186 ROM",              "(ROM Table)", },
	{ 0x3eb, 0x211, "Tegra 210 ROM",              "(ROM Table)", },
	{ 0x3eb, 0x202, "Denver ETM",                 "(Denver Embedded Trace)", },
	{ 0x3eb, 0x302, "Denver Debug",               "(Debug Unit)", },
	{ 0x3eb, 0x402, "Denver PMU",                 "(Performance Monitor Unit)", },
	{ 0x1000, 0x120, "TI SDTI",                    "(System Debug Trace Interface)", },
	{ 0x1000, 0x343, "TI DAPCTL",                  "", },
}

function ComponentInfo(memAccessPort, addr)
	-- 获得对齐后的地址
    local base_addr = addr & 0xFFFFF000
    -- 读取pid和cid
    local cid, pid = memAccessPort:GetCidPid(base_addr)
    if (cid & 0xFFFF0FFF) ~= 0xB105000D then
        -- 无效的CID
        print("Invaild CID:" .. string.format("0x%08X", cid))
        return
    end
    print(string.format( "* Component CID:0x%08X, PID:0x%010X.", cid, pid))

    -- 获得当前Component的类型
    local compon_type = (cid >> 12) & 0xF
    local part_num = pid & 0xFFF
    local designer_id = ((pid >> 32) & 0xF) << 8 | ((pid >> 12) & 0xFF)
    -- TODO 判断designer_id是否是JEP106，designer_id & 0x80 == 0x80
    -- 打印组件详细信息
    for idx,info in ipairs(dap_infos) do
        if info[1] == designer_id and info[2] == part_num then
            print("- " .. info[3] .. " " .. info[4])
            break
        end
    end
    if compon_type == 0x1 then    -- ROM Table
        local mem_type = memAccessPort:Memory32(base_addr + 0xFCC)
        if mem_type & 0x1 == 0x1 then
            print("ROMTable - MEMTYPE system memory present on bus.")
        else
            print("ROMTable - MEMTYPE system memory not present: dedicated debug bus.")
        end
    elseif compon_type == 0x9 then -- CoreSight component
        -- 读DEVARCH,DEVID,DEVTYPE
        local dev_arch = memAccessPort:Memory32(base_addr + 0xFBC)
        local dev_id = memAccessPort:Memory32(base_addr + 0xFC8)
        local dev_type = memAccessPort:Memory32(base_addr + 0xFCC)
        print(string.format( "* DEVARCH:0x%08X, DEVID:0x%08X, DEVTYPE: 0x%08X.", dev_arch, dev_id, dev_type))
        local minor = (dev_type >> 4) & 0x0F
        local major_type, sub_type = "other"
        if dev_type & 0x0F == 0 then
            major_type = "Miscellaneous"
            if minor == 4 then
                sub_type = "Validation component"
            end
        elseif dev_type & 0x0F == 1 then
            major_type = "Trace Sink"
            if minor == 1 then
                sub_type = "Port"
            elseif minor == 2 then
                sub_type = "Buffer"
            elseif minor == 3 then
                sub_type = "Router"
            end
        elseif dev_type & 0x0F == 2 then
            major_type = "Trace Link"
            if minor == 1 then
                sub_type = "Funnel, router"
            elseif minor == 2 then
                sub_type = "Filter"
            elseif minor == 3 then
                sub_type = "FIFO, buffer"
            end
        elseif dev_type & 0x0F == 3 then
            major_type = "Trace Source"
            if minor == 1 then
                sub_type = "Processor"
            elseif minor == 2 then
                sub_type = "DSP"
            elseif minor == 3 then
                sub_type = "Engine/Coprocessor"
            elseif minor == 4 then
                sub_type = "Bus"
            elseif minor == 6 then
                sub_type = "Software"
            end
        elseif dev_type & 0x0F == 4 then
            major_type = "Debug Control"
            if minor == 1 then
                sub_type = "Trigger matrix"
            elseif minor == 2 then
                sub_type = "Debug auth"
            elseif minor == 3 then
                sub_type = "Power requestor"
            end
        elseif dev_type & 0x0F == 5 then
            major_type = "Debug Logic"
            if minor == 1 then
                sub_type = "Processor"
            elseif minor == 2 then
                sub_type = "DSP"
            elseif minor == 3 then
                sub_type = "Engine/Coprocessor"
            elseif minor == 4 then
                sub_type = "Bus"
            elseif minor == 5 then
                sub_type = "Memory"
            end
        elseif dev_type & 0x0F == 6 then
            major_type = "Perfomance Monitor"
            if minor == 1 then
                sub_type = "Processor"
            elseif minor == 2 then
                sub_type = "DSP"
            elseif minor == 3 then
                sub_type = "Engine/Coprocessor"
            elseif minor == 4 then
                sub_type = "Bus"
            elseif minor == 5 then
                sub_type = "Memory"
            end
        end
        print("- Type is " .. major_type .. " - " .. sub_type)
    end
end

--[[--
scripts/adapters/ft2232.lua
Copyright (c) 2020 Virus.V <virusv@live.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
--]]--

adapter = require("Adapter")
ftdi = require("FTDI"); -- 加载FTDI库
print("Init FTDI.")

ftdiObj = ftdi.Create(); -- 创建新的FTDI Adapter对象
-- FTDI的VID和PID
local vid_pids = {
	{0x0403, 0x6010},	-- FTDI 2232H BL602板载
}

-- 连接FTDI仿真器，channel 1
ftdiObj:Connect(vid_pids, nil, 1)
-- 设置频率为500KHz
ftdiObj:Frequency(500000)
print("FTDI: Current frequent is " .. ftdiObj:Frequency() .. "Hz")
return ftdiObj

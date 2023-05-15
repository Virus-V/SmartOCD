-- Copyright (c) 2023, Virus.V <virusv@live.com>
-- All rights reserved.
--
-- Redistribution and use in source and binary forms, with or without
-- modification, are permitted provided that the following conditions are met:
--
-- * Redistributions of source code must retain the above copyright notice, this
--   list of conditions and the following disclaimer.
--
-- * Redistributions in binary form must reproduce the above copyright notice,
--   this list of conditions and the following disclaimer in the documentation
--   and/or other materials provided with the distribution.
--
-- * Neither the name of SmartOCD nor the names of its
--   contributors may be used to endorse or promote products derived from
--   this software without specific prior written permission.
--
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
-- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
-- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
-- DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
-- FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
-- DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
-- SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
-- CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
-- OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
-- OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
-- Copyright 2023 Virus.V <virusv@live.com>
--

local loop = require('Loop')
local tcp = require('Loop.tcp')

--[[
local server = tcp.Create()
server:Bind("", 1337)
server:Listen(128, function (err)
  assert(not err, err)
  local client = tcp.Create()
  server:Accept(client)
  client:ReadStart(function (err, chunk)
    assert(not err, err)
    if chunk then
      print (chunk)
      client:Write(chunk)
    else
      print("client shutdown")
      client:Shutdown(function ()
        client:Close()
        server:Close()
      end)
    end
  end)
end)
]]

-- loop.Run('default')
-- loop.Close()


local function on_connect(server, err)
  assert(not err, err)
  local c = tcp.Create()
  server:Accept(c)

  c:ReadStart(function (err, chunk)

    assert(not err, err)
    if chunk then
      print (chunk)
      c:Write(chunk)
    else
      print("client shutdown")
      c:Shutdown(function ()
        c:Close()
        server:Close()
      end)
    end

  end)

end

function GdbCreate(addr, port, handler)
  local s = tcp.Create()

  s:Bind(addr, port)
  s:Listen(128, on_connect)
  return {
    server = server
  }
end

GdbCreate("0.0.0.0", 1337)

loop.Run('default')
loop.Close()

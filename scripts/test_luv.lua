local loop = require('Loop')
local tcp = require('Loop.tcp')

local server = tcp.Create()
server:Bind("127.0.0.1", 1337)
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

loop.Run('default')
loop.Close()

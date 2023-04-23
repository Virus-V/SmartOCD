local uv = require('luv')

local domains = {
  "www.baidu.com",
  "www.163.com"
}

local i = 1
local function next()
  uv.GetAddrInfo(domains[i], nil, {
    v4mapped = true,
    all = true,
    addrconfig = true,
    canonname = true,
    numericserv = true,
    socktype = "STREAM"
  }, function (err, data)
    assert(not err, err)
    print(data)
    i = i + 1
    if i <= #domains then
      next()
    end
  end)
end
next();

repeat
  -- print("\nTick..")
until uv.Run('once') == 0

print("done")

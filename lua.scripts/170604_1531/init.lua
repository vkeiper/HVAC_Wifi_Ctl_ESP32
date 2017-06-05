--init.lua

dofile("ssr_pins.lua")

print("Init Lua Version v0.0.3 20170604_1357_VK")
wifi.setmode(wifi.STATION)
wifi.sta.config("NETGEAR26","fluffyvalley904")
--wifi.sta.sethostname("MBRHVAC")

print("Connecting to:NETGEAR26 PWD:fluffyvalley904")
wifi.sta.connect()
tmr.alarm(1, 1000, 1, function()
    if wifi.sta.getip() == nil then
        print("IP unavaiable, Waiting...")
    else
        tmr.stop(1)
        print("WiFi mode is: " .. wifi.getmode())
        print("The module MAC address is: " .. wifi.ap.getmac())
        print("Config done, IP is "..wifi.sta.getip())
        
		srv=net.createServer(net.TCP)
		srv:listen(80,function(conn)
			conn:on("receive", function(client,request)
				local buf = "";
				local _, _, method, path, vars = string.find(request, "([A-Z]+) (.+)?(.+) HTTP");
				if(method == nil)then
					_, _, method, path = string.find(request, "([A-Z]+) (.+) HTTP");
				end
				local _GET = {}
				if (vars ~= nil)then
					for k, v in string.gmatch(vars, "(%w+)=(%w+)&*") do
						_GET[k] = v
					end
				end
				buf = buf.."<h1> HVAC Remote Control Web Server</h1>";
				buf = buf.."<p>SSR_AUXFAN <a href=\"?pin=ON1\"><button>ON</button></a>&nbsp;<a href=\"?pin=OFF1\"><button>OFF</button></a></p>";
				buf = buf.."<p>SSR_ACMAIN <a href=\"?pin=ON2\"><button>ON</button></a>&nbsp;<a href=\"?pin=OFF2\"><button>OFF</button></a></p>";
				local _on,_off = "",""
				if(_GET.pin == "ON1")then
					  gpio.write(aSSR[1][1], gpio.HIGH);
				elseif(_GET.pin == "OFF1")then
					  gpio.write(aSSR[1][1], gpio.LOW);
				elseif(_GET.pin == "ON2")then
					  gpio.write(aSSR[2][1], gpio.HIGH);
				elseif(_GET.pin == "OFF2")then
					  gpio.write(aSSR[2][1], gpio.LOW);
				end
             
                TempValRaw = adc.read(0)  
                TempDeg = (TempValRaw *100) / 29       
                buf = buf .."Temp sensor:  RAW-" ..TempValRaw .."Scaled-" ..tostring(TempDeg/10) .."degC\n\f";
                buf = buf .."Node Heap:" ..node.heap();
                print(node.heap())
                
                client:send(buf);
				client:close();
				collectgarbage();
			end)
		end)
    end
end)



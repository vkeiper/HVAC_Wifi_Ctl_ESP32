--init.lua

dofile("ssr_pins.lua")

print("Init Lua Version v0.0.3 20170604_1704_VK")
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
        
		--start web site
		
		srv=net.createServer(net.TCP)
		srv:listen(80,function(conn)
			conn:on("receive", function(client,request)
				local buf = ""
				local _, _, method, path, vars = string.find(request, "([A-Z]+) (.+)?(.+) HTTP")
				--debug code to print rxd frame from web
				print(request)
				if(method == nil)then
					_, _, method, path = string.find(request, "([A-Z]+) (.+) HTTP")
				end
				local _GET = {}
				if (vars ~= nil)then
					for k, v in string.gmatch(vars, "(%w+)=(%w+)&*") do
						_GET[k] = v
					end
				end
                
				buf = buf.."<head> <script language=\"javascript\">";
				buf = buf.."function colorchange(id) { var xmlhttp; xmlhttp=new XMLHttpRequest();";
				buf = buf.."if(document.getElementById(id).value == \'OFF\' )";
				buf = buf.."{ document.getElementById(id).style.background = \'#ff0000\'; ";
				buf = buf.."document.getElementById(id).value = \'ON\';";
				buf = buf.."xmlhttp.open(\"GET\",\"a.htm?pin=ON2\",true);xmlhttp.send();}";
				buf = buf.."else { document.getElementById(id).style.background = \'#00ff00\'; ";
				buf = buf.."document.getElementById(id).value = \'OFF\'; xmlhttp.open(\"GET\",\"a.htm?pin=OFF2\",true);xmlhttp.send();}}";
				buf = buf.." </script></head> <body>";
				buf = buf.."<input type=\"button\" id=\"btn\" onclick=\"colorchange(\'btn\');\" value =\"OFF\" style=\"background:#00ff00;\"></body></html>";

				local _on,_off = "",""
				
				if(_GET.pin == "ON1")then
                      gpio.write(aSSR[1][1], gpio.HIGH);
                elseif(_GET.pin == "OFF1")then
                      gpio.write(aSSR[1][1], gpio.LOW);
                elseif(_GET.pin == "ON2")then
                      gpio.write(aSSR[2][1], gpio.HIGH);
                elseif(_GET.pin == "OFF2")then
                      gpio.write(aSSR[2][1], gpio.LOW);
                elseif(_GET.pin == "ON3")then
                      gpio.write(aSSR[3][1], gpio.HIGH);
                elseif(_GET.pin == "OFF3")then
                      gpio.write(aSSR[3][1], gpio.LOW);
                end
             
                TempValRaw = adc.read(0)  
                TempDeg = (TempValRaw *100) / 29       
                buf = buf .."<p>Temp sensor:  RAW- " ..TempValRaw .."&nbsp Scaled- " ..tostring(TempDeg/10) .." degC</p>";
                buf = buf .."<p>Node Heap: " ..node.heap() .."</p>";
                print(node.heap())
             	
				client:send(buf)
			end)
			conn:on("sent", function (c) c:close() end)
		end)
		--end web site
    end
end)



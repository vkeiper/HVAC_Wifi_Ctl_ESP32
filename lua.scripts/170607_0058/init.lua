--init.lua

dofile("ssr_pins.lua")

--Create Timer to restore AC after warmup period, will be 15 minutes after debug
local tmrFrostCheck = tmr.create()
-- register timer if not already reg'd
tmrFrostCheck:register(1000, tmr.ALARM_AUTO, function() dofile("ssrs_rules.lua") end)

mn_auxfan = false
mn_acmain = false
print("Init Lua Version v0.0.3 20170604_1704_VK")
wifi.setmode(wifi.STATION)
--home network
wifi.sta.config("NETGEAR26","fluffyvalley904")
--ATDI Wifi
--wifi.sta.config("Astrodyne","Astro$01")
--wifi.sta.sethostname("MBRHVAC")

print("Connecting to:NETGEAR26 PWD:fluffyvalley904")
wifi.sta.connect()
tmr.alarm(1, 1000, 1, function()
    if wifi.sta.getip() == nil then
        print("IP unavaiable, Waiting...")
    else
        tmr.stop(1)
        --start testing for frost
        tmr.start(tmrFrostCheck)
        
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
				--print(request)
				if(method == nil)then
					_, _, method, path = string.find(request, "([A-Z]+) (.+) HTTP")
				end
				local _GET = {}
				if (vars ~= nil)then
					for k, v in string.gmatch(vars, "(%w+)=(%w+)&*") do
						_GET[k] = v
					end
				end
                buf = buf.."<title>WiFi Linked HVAC Controller</title>"
                buf = buf.."\"<meta http-equiv=\"refresh\" content=\"5\" >"
                buf = buf.."<h1>MBR HVAC Control Web Server</h1>"
                if gpio.read(aSSR[2][1]) == 1 then
                    buf = buf.."<p>SSR_AUXFAN&nbsp <a href=\"?pin=ON1\"><button style=\"background:green; cursor:pointer\">ON</button></a>&nbsp;<a href=\"?pin=OFF1\"><button>OFF</button></a></p>"
                else
                    buf = buf.."<p>SSR_AUXFAN&nbsp <a href=\"?pin=ON1\"><button cursor:pointer\">ON</button></a>&nbsp;<a href=\"?pin=OFF1\"><button>OFF</button></a></p>"
                end
                if gpio.read(aSSR[1][1]) == 1 then
                    buf = buf.."<p>SSR_ACMAIN&nbsp <a href=\"?pin=ON2\"><button style=\"background:green; cursor:pointer\">ON</button></a>&nbsp;<a href=\"?pin=OFF2\"><button>OFF</button></a></p>"
                else
                    buf = buf.."<p>SSR_ACMAIN&nbsp <a href=\"?pin=ON2\"><button>ON</button></a>&nbsp;<a href=\"?pin=OFF2\"><button>OFF</button></a></p>"
                end 

                if gpio.read(aSSR[3][1]) == 1 then
                    buf = buf.."<p>TSTAT&nbsp <a href=\"?pin=ON3\"><button style=\"background:green; cursor:pointer\>ON</button></a>&nbsp;<a href=\"?pin=OFF3\"><button>OFF</button></a></p>"
                else
                    buf = buf.."<p>TSTAT&nbsp <a href=\"?pin=ON3\"><button>ON</button></a>&nbsp;<a href=\"?pin=OFF3\"><button>OFF</button></a></p>"
                end
                local _on,_off = "",""
                if(_GET.pin == "ON1")then
                      mn_auxfan = true
                elseif(_GET.pin == "OFF1")then
                      mn_auxfan = false
                elseif(_GET.pin == "ON2")then
                      mn_acmain = true
                elseif(_GET.pin == "OFF2")then
                      mn_acmain = false
                elseif(_GET.pin == "ON3")then
                      gpio.write(aSSR[3][1], gpio.LOW);
                elseif(_GET.pin == "OFF3")then
                      gpio.write(aSSR[3][1], gpio.HIGH);
                end
             
                buf = buf .."<p>Temp sensor:  RAW " ..an_ch0raw .."bits &nbsp Scaled " ..string.format("%2.2f",an_condtemp) .." degC</p>";
                if tstat_dmd == true then ssta = "COOL" else ssta = "OFF"  end
                if frost_flt == true then sfrst = "ERR" else sfrst = "---"  end
                
                buf = buf .."<p> TSTAT " ..ssta .."&nbspFROST &nbsp" ..sfrst .."</p>";
                buf = buf .."<p>Node Heap: " ..node.heap() .."</p>";
                
				client:send(buf)
			end)
			conn:on("sent", function (c) c:close() end)
		end)
		--end web site
    end
end)



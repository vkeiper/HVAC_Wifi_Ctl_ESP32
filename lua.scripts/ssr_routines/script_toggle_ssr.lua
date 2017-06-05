pin = 1
gpio.mode(pin,gpio.OUTPUT)
ssron_fanaux = false
cycle_cnt =0

--start the wifi cfg 
--enduser_setup.start()

function togglessr()
  if not ssron_fanaux then
    gpio.write(pin,gpio.HIGH)
    print("pin set HIGH Cnt=",cycle_cnt)
    ssron_fanaux = true
  else
    gpio.write(pin,gpio.LOW)
    print("pin set LOW  Cnt=", cycle_cnt)
    ssron_fanaux = false
  end
   cycle_cnt = cycle_cnt + 1
end
  
tmr.alarm(1,300,1,function() togglessr() end)
-- after sometime
tmr.stop(0)


  

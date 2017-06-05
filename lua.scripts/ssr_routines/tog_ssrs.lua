--assign pins
-- Array 1st equal GPIO pin 'Dx' 
-- 2nd Last pin state
-- 3rd Update count
-- 4th element Name

aSSR = {{1,0,0,"AUXFAN"},{2,0,0,"ACMAIN"}, {3,0,0,"ACBLOW"}, {4,0,0,"ACPUMP"}}

--set DDR
gpio.mode(aSSR[1][1],gpio.OUTPUT)
gpio.mode(aSSR[2][1],gpio.OUTPUT)
gpio.mode(aSSR[3][1],gpio.OUTPUT)
gpio.mode(aSSR[4][1],gpio.OUTPUT)

--method to toggle pins for SSR control
function TogSSR(pin)
  --last status for th pin passed in
  if not aSSR[pin][2] then
    gpio.write(pin,gpio.HIGH)
    print(aSSR[pin][4],pin," set HIGH Cnt=",aSSR[pin][3])
    aSSR[pin][2] = true
  else
    gpio.write(pin,gpio.LOW)
    print(aSSR[pin][4],pin," set LOW Cnt=",aSSR[pin][3])
   aSSR[pin][2] = false
  end
   aSSR[pin][3] = aSSR[pin][3] + 1
end


tmr.alarm(1,1000,1,function() TogSSR(aSSR[1][1]) end)

tmr.alarm(2,2000,1,function() TogSSR(aSSR[2][1]) end)

-- after sometime
tmr.stop(0)

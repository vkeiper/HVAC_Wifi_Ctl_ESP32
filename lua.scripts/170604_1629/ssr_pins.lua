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


gpio.write(aSSR[1][1],gpio.LOW)
gpio.write(aSSR[2][1],gpio.HIGH)

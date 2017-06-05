
-- Ananlog temperature register for condensor temp
an_condtemp =0

--Create Timer to restore AC after warmup period, will be 15 minutes after debug
local tmrFrost = tmr.create()
-- register timer if not already reg'd
tmrFrost:register(5000, tmr.ALARM_SINGLE, function() RestoreACMain() end)


--method to toggle pins for SSR control
function FrostCheck()
  --last status for th pin passed in
  an_condtemp = (adc.read(0) * 100) /29
  print("ADC RAW ",adc.read(0)," bits", " Condensor Temp ",an_condtemp/10,"Deg C")
  if an_condtemp <32 * 10 then
    running, mode = tmrFrost:state()
    print("running: " .. tostring(running) .. ", mode: " .. mode) -- running: false, mode: 0
    --only start the timer once per frost session
    if not running then
        tmr.start(tmrFrost)
        print("FROST EMINENT, SHUTDOWN ACPUMP & WAIT FOR WARMUP")
        --shutdown AC main
        gpio.write(aSSR[2][1],gpio.LOW)
        print(aSSR[2][4],2," set LOW Cnt=",aSSR[2][3])
        aSSR[2][2] = false
        aSSR[2][3] = aSSR[2][3] + 1
        --Turn on AC AUXFAN to accellerate melting ICE off condenser
        gpio.write(aSSR[1][1],gpio.HIGH)
        print(aSSR[1][4],1," set HIGH Cnt=",aSSR[1][3])
        aSSR[1][2] = true
        aSSR[1][3] = aSSR[1][3] + 1
    end
  else
     print("NO FROST COND. NOMAL OP MODE")
     tmr.stop(tmrFrost)
  end
end

--method to restart the AC after warmup period
function RestoreACMain()
    if an_condtemp >40 * 10 then
  
        print("Frost Timer Fired- RESTORE ACMAIN")
        gpio.write(aSSR[2][1],gpio.HIGH)
        print(aSSR[2][4],pin," set HIGH Cnt=",aSSR[2][3])
        aSSR[2][2] = true
        
        --Ensure AC AUXFAN is ON to reduce chance of ICE on condenser
        gpio.write(aSSR[1][1],gpio.HIGH)
        print(aSSR[1][4],1," Keep HIGH Cnt=",aSSR[1][3])
        aSSR[1][2] = true
        aSSR[1][3] = aSSR[1][3] + 1
        --stop firing
        tmr.stop(tmrFrost)
     else
        print("CONDENSER <32 degC ReARM Timer")
        --restart timer to reseed wait state 
        tmr.start(tmrFrost)

     end
end

--Execute Frost Check 
FrostCheck()


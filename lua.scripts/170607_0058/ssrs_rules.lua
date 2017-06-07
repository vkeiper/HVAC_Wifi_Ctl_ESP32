
-- Ananlog temperature register for condensor temp
an_condtemp =0
an_ch0raw =0
dofile("ptc10k.lc")
--Create Timer to restore AC after warmup period, will be 15 minutes after debug
local tmrFrost = tmr.create()
-- register timer if not already reg'd
tmrFrost:register(5000, tmr.ALARM_SINGLE, function() RestoreACMain() end)

local acmain_on = 0
tstat_dmd = false
frost_flt = false
--method to toggle pins for SSR control
function FrostCheck()
  
  --last status for th pin passed in
  an_ch0raw = adc.read(0)
  --an_condtemp = an_ch0raw *(300/1023)
  an_condtemp =  ProcessPtc(an_ch0raw)
  print("ADC",adc.read(0),"bits", "Acoil Temp",string.format("%2.2f",an_condtemp),"degC",an_condtemp* 9/5 + 32, "degF")
  --print(string.format("%2.2f",an_condtemp))
  if an_condtemp <1  then
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

        frost_flt = true
    end
  else
     print("NO FROST COND. NOMAL OP MODE")
     tmr.stop(tmrFrost)
  end
end

--method to restart the AC after warmup period
function RestoreACMain()
    if an_condtemp >5  then
        frost_flt = false
        if tstat_dmd then
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
            tmr.stop(tmrFrost)
        end
    else
        print("CONDENSER <32 degC ReARM Timer")
        --restart timer to reseed wait state 
        tmr.start(tmrFrost)
    end
end

function TestTstat()
    -- read stat demand for cool state
    if gpio.read(0) == 0 then tstat_dmd = true else tstat_dmd = false end
    --status dbg print
    print("MAN",mn_auxfan,mn_acmain,"GPIO0 ",gpio.read(0),"TSTAT",tstat_dmd,"FRST ", frost_flt)
    
    --Test if ACMAIN should be on
    if mn_auxfan == true then 
        gpio.write(aSSR[2][1],gpio.HIGH) 
    else
        if tstat_dmd == true and frost_flt == false then
            gpio.write(aSSR[2][1],gpio.HIGH)
        elseif tstat_dmd == false  or frost_flt == true then
            gpio.write(aSSR[2][1],gpio.LOW)
        end
    end

    --Test If AuxFan should be ON
    if mn_acmain == true then 
        gpio.write(aSSR[1][1],gpio.HIGH) 
    else
        if tstat_dmd == true and frost_flt == false then
            gpio.write(aSSR[1][1],gpio.HIGH)
        elseif tstat_dmd == false  or frost_flt == true then
            gpio.write(aSSR[1][1],gpio.LOW)
        end
    end

    
    

end

--Test Tstat for demand for cooling
TestTstat()

--Execute Frost Check 
FrostCheck()


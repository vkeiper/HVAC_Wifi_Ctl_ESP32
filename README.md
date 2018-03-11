 DVT_TEST BBU w/o Multioutput Controller
=======================================
These tests were all run using PCAN Explorer to send CANbus commands
--------------------------------------------------------------------
Wednesday, February 14, 2018
11:45 AM

	1. [ ] Battery FET operation
      1. ON when "1" sent
	  2. OFF when "0" sent
	  4. OFF when CANbus timeout (5 seconds)
		
	2 Load Resistor operation
		a. Load resistor is switch into circuit 5A for duration selected and then turns off.
		b. ON when "1" sent
		c. OFF when "0" sent
		d. Default time 200mS
			i. ON pulse duration +/-10mS of Duration requested in mS
				1) Actually was +1.4mS max
		e. Future features to be added after essentials
			i. Custom Test duration (Bytes 2/3 in 0x201 1mS/bit min 100-max 65000)
				1) Send 100,200,1000,2000,5000 measure pulses within +/-10mS
			ii. Lockout load resistor operation if Battery FET is on
				1) Set lockout bit in 0x200 CANbus test report 
			iii. Cool down period
				1) Lock load resistor operation for period of time
		
	3. Scale ADCs
		a. Battery Voltage
			i. Data 0V to full scale volts
			ii. NA because analog battery controller disconnects battery <40V
Meter
ADC
Vbat
0.0
NA
NA
1
NA
NA
10
NA
NA
20
NA
NA
30
NA
NA
40
2787
40.1
50.02
3489
50.1
58.9
4095
58.9
60
4095
58.9
			iii. 
		b. Battery Discharge Current
			i. Data 0A to 45A
				1) 130-135bits on ADC pin with no load
				2) Added code to apply scaled offset based on amount of current
					a) Logic, ADC value on Idchg pin is stored when BAT FET is off
						i) <1000 bits use 100% offset applied
						ii) >1000 && <1547 50% offset applied
						iii) >1547 && <2547 0% offset applied
						iv) >2547 && <3547 50% offset applied
						v) >3547 100% offset applied
Set
Load
ADC
Idchg
0
0.0
133
.1
1
0.9
221
1.1
10
9.9
998
9.6
20
19.99
1911
21.1
30
29.99
2822
30.5
40
39.99
3727
39.8
43.5
43.4
4042
43.3
45
45
4095
43.9
		c. Temperature (NTC sensor)  **Self heating from microcontroller in play, must reloacate
			i. Same sensor and code used in LQB
			ii. At ambient 22-25C measured
			
	4. Discharge behavior
		a. When Rectifier output shuts down and the BAT FET is turned on
			i. BBU deliver current to multioutput
			ii. Controller stays powered and delivers CANbus messages down to UV point
				1) Stays on above >40V
			iii. If battery voltage recovers after the analog controller cuts off at the UV point the BATFET remains OFF
				1) Turn on RECT to 55V
				2) Turn on Battery to 55V
				3) Apply load 43
				4) Turn PSU off
					a) Battery Voltage remains at Load
				5) Slowly reduce battery voltage down to 38V
					a) Record voltage when battery output is disconnected from the load.
						i) 40.9V__
				6) Slowly increase battery voltage to 55V (leave PSU off)
					a) Battery output remains disconnected from load
	5. Discharge on battery test
		a. Switch to battery (PSU off) while load at 43A
		b. _140Sec  Record time supports load (120 second specification)
		
	6. Battery Health Parameters
		a. < 5 years in service
			i. There is No real time clock or EEPROM on the BBU, so it cannot perform these tests  
			ii. If we could burn the serial number with date code encoded while being programed it may be possible for the Multioutput controller to test this value against with the current date and time passed in over USB
				1) Can the serial number be burned with the programmer itself?
					a) Research microchips documentation.
				2) Would KnS be ok adding the date and time being passed in through USB
		b. < 200 discharges
			i. Again no EEPROM on the BBU, so can the multioutput track these in its EEPROM?
		c. Periodic load test failures
			i. Again no EEPROM on the BBU, so can the multioutput track these in its EEPROM?
			
		
	7. Critical Code timing
		a. Per analysis the following times have been deemed critical and must be tested and adhered to limits
			i. Main loop
				1) Max 5mS mean 100uS
			ii. CANbus command latency for 0x201 (5-10mS mean 20mS max)
				1) The Discharge FET ON command must be executed within 20mS of CANbus frame reception
				2) 10mS from CANbus frame arriving at transceiver to executing new set points
					a) There are 3 degrees of separation from the CANbus to set point execution
						i) CANbus DMA ISR stuffs ring buffer (1mS per 8 byte frame at 250Kbs)
						ii) SrvIntCAN() executes in main loop
							One. Polled timing, max 5mS mean 200uS
							Two. Latency of execution itself. 
								First. 1mS per CANbus command received ( 
								Second. 1mS per CANbus frames queued in the TX ring buffer for transmission
								
				3) DoControls() state machine called from main loop (100uS mean 5mS max)
					a) This is called directly after CANbus is received so this time ill be offset only by 
			iii. Systick timer must not jitter +/-10uS
				1) This is our system timer and the ISR should have highest priority
# HVAC_Wifi_Ctl_ESP32
Wireless control of a window AC using the ESP32 devkit, SSR's and PTC temp sensors.

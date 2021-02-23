# XDOF-data-logger

The data logger uses the xdof sensor pcb (jps2000 / XDOF-TOF-feather-nugget ) mounted on an ADAFRUIT feather nrf52832 and transfers data via Bluetooth to another feather nrf52832 (dongle). The latter works as central BT station and is connected via USB to a PC where data are stored in excel using PLX DAQv2 by Net^devil freeware.
There are the colums date , time, yaw, pitch, roll, dphi, alt and stat logged every second. 
Yaw, pitch and roll are the Euler angles; dphi is the delta angle between previous and current data point (1 sec apart); alt is the altitude relative to start. It is determined via the air pressure. Resolution is about 15cm. The value is drifting due to natural air pressure changes. Also wind and opening / closing doors can have an effect. Stat ist the calibration status of the IMU only values 2 and 3 should be trusted. 

The status is also indicated with the npx led on the xdof board. red = 0,1 ; yellow = 2, green = 3. 

For calibration, place the sensor in all orientations and move it gently around ( figure of eight) then the LED should become green. This should be done at the place of measurements and after some minutes of warmup. See BNO080 specification and application notes for calibration matters and readme of jps2000/ BNO080 for additional information.

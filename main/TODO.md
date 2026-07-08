# 
# 2026-06-24

* add the INA219 current sensor
	* find or create the INA219 ESP-IDF driver
	* hook up the current sensor to the 12V line
	* add browser support

* hook up the TIP120 switch for the fan
	* setup PWM for the fan

* Look into embedded HW & SW support for monitoring electrical & temperature
  safety on the Incubator
	* what are the appropriate alarms
	* when to shutdown the system
	* need for UL testing

# 2026-07-07
* Incorporated an I2C breakout board, which allows for multiple I2C connections of various sensors via pin headers, dupont male on the
  board. By then re-doing the fairly ugly mess, using longer 4-wire cable lengths and female pin headers, it became slightly more 
  orderly. Testing indicates that the sensors are functioning well.
  * Need to connect the heating pad LED indicator; 
  * Order cell culture flasks from Amazon, since we're getting closer to testing with the shaker table setup in the incubator.
  * Check hook up to fan - might just need to turn on GPIO connection
  * When that is done, Change the GPIO to PWM for controlling the fan speed.
   
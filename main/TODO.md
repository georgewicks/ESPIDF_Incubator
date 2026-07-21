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
  
# 2026-07-15
* The FAN PWM is working as intended 
* The Heating Pad PWM Ramp up, Plateau, Ramp down work as intended.
* Need to clear up & make standard device component structures: everyone seems to have a different way of creating their interface: some writers make code that assumes that a configuration object/item is allocated by the caller, while others will take care of the allocation of the object, resulting in sometimes pointers to pointers, which can lead to a lot of confusion in navigating. Might need to do that with some of the I2C sensor code.
* Need to do the calls to get the current, voltage, etc. from the INA219
* from a demo program for the INA219, do
    Serial.print(INA.getBusVoltage(), 2);
    Serial.print(INA.getShuntVoltage_mV(), 2);
    Serial.print(INA.getCurrent_mA(), 2);
    Serial.print(INA.getPower_mW(), 2);

# 2026-07-16
* Will re-use some of the esp-idf-lib INA219 code for the INA219 configuration and calibration. Including the License file.

# 2026-07-20
* I am pushing the use of the INA219 sensor to the backburner for the time being. I have preliminary data for the current draw on the 12V Heating Pad circuit, which gives a range of 22 mA to 30 mA by use of the multimeter. This certainly does not look like a pressing issue - but may become important in the future, so I am also putting in application notes for current sensing in the docs folder.
* must re-wire & remove multimeter.

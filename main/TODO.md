# 
# 2026-06-02 --- Add PWM control for the 12V Heating element

History: we tried using a simpler 5V heating element, but that proved inadequate. It only warmed up 1 degree centigrade warmer over the ambient temperature. It is entirely possible that the LM2985 DC-DC converter may be severely limiting current, and that might be the reason for the dismal warming properties, however, I did find a more powerful 12V heating element available on Amazon, and that does seem to be extremely good. However, it does heat up very quickly: the temperature readings were greater than 170 degrees F in about a minute - dangerously quick overheating potential.

Therefore, it is worthwhile to explore the application of PWM to the heating element TIP120 NPN Darlington pair power transistor. (NOTE: may want to also revisit using a better switching transistor).

Please see the "PWM Control of 12V Heating Element in the Incubator" in the docs folder for more details

* Basic outline
    * Step 1: Source must the header:    #include "driver/ledc.h"
    * Step 2: Configure the Timer. (see doc)
    * Step 3: Configure the Channel.  ("")
    * Step 4: Set (and change) the Duty Cycle. (see doc)

# 2026-06-15 --- Need to add another temperature sensor to the system in order to better monitor the heating pad.

Add a DS18b20 Onewire temperature sensor, and attach the probe directly on the surface of the heating pad. By getting a more precise temperature reading, we can better address what the PWM maximum values should be for the heating pad - we want to avoid any potential problems. 
    * use GPIO 5 for the onewire. 
    * add 4.7K ohm resistor between GPIO 5 and VCC (3.3v) - the pull-up resistor needed for onewire
    * add the ds18b20 component from the esp-bsp folder to the Incubator project
    * add the monitoring handling to the TempControl.c module.
    * Create heating_pad_monitor branch git branch.

# The DS18b20 temp sensor for ESP-IDF was too involved for me - several revision changes - so I abandoned ans instead used the BME280
# Will need to add the 12v fan, and maybe another heating pad for better temperature distribution. 2026-06-21

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

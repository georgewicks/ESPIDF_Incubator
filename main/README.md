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

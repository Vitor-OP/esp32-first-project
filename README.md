# ESP-32 EDUCATIONAL PROJECT
  The idea is to create a simple clock that also displays the temperature using multiple ESP32 features, such as dual-core, internet connection, internal RTC, I2C, and interrupts.

## Features:
- Uses the internal RTC to count time from 0.
- Takes temperature from the thermometer module DTH22.
- Displays it in the 128x32 OLED.
- Divides the task of displaying and the task of fetching values to each core (just to use parallel programming).
- Interrupt dictating what is being displayed in the OLED.

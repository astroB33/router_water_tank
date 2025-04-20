Overview
========

Uses a dimmer to route the overproduction of solar panels to the water tank.

Used hardware:
* Controller : [esp32](https://www.upesy.fr/)
* Dimmer : SSR relay with zero crossing and thermal dissipator, take a much bigger value than needed (can be found on AliExpress)
* Temperature sensors : non waterproof DS18B20 (as many as needed) + one plugable board (can be found on AliExpress)
* Relays : plugable SRD-03VDC (can be found on AliExpress)
* Power : 5V USB power + USB cable (can be found on AliExpress)

![Alt text](html_display.png?raw=true "HTML display")

Details
=======

The ESP32 is powered by though the USB port, delivering 5V.
The ESP32 3.3V power output is used to power the temperature sensors, the SRD relays, the triac (SSR relay).
The ESP32 inputs/outputs (3.3V) are used to read values (triac, sensor) and enable the relays.

Following wiring is used in this project.

![Wiring](wiring.png?raw=true "Wiring")

water_tank_power_handler
------------------------

The water_tank_power_handler deals with the dimmer and 4 relays with the following logic:
* The water tank has indeed 3 "small" resistors of 800W for a total of 3*800W (configurable).
* One of the resistor is connected to a "dimmer + relay" and a relay for full power.
* The other 2 resistors are each connected to a relay.
* If the requested power is smaller than 800W, then the dimmer is used.
* If the requested power is in 800W .. 1600W, then one relay is used + the dimmer for the "power - 800W".
* If the requested power is in 1600W .. 2400W, then 2 relays are used + the dimmer for the "power - 1600W".
* If the requested power is greater than 2400W, then 3 relays are used + the dimmer is turned off (the relay associated the the dimmer is turned off).

A minimal dimmer power can be set, so the dimmer is only used if the requested power for the dimmer exceeds this power. E.g. if the minimum is set to 100W, then the dimmer not used when the total power is 50W, 850W or 1650W.

power_manager
-------------

The power_manager managers the water_tank_power_handler with the following logic:
* No maximum temperature is handled by the ESP32, the hardware sensor temperature of the water tank handles it.
* During day time (configurable), the power provided though the `onPowerInfo` triggers the dimmable power.
* During night time (configurable), the maximal power is applied, in order to reach the stored minimal temperature (configurable).
* Periodically (configurable), the anti-legionelosis temperature (configurable) is used during night time. The period is reset if the temperature is reached during day time.
* A configurable inhibition time can also be set (e.g. during holidays) : only the night heating is inhibited, the day time routing is still performed.
* If time can't be retrieved, maximal heating is applied.
* Callbacks can be provided to manage the rurning on and off the overproduction power retrieval.

router_water_tank.ino
---------------------

Implements the following features:
* Watchdog and ntp time.
* Periodic temperature retrieval : As many as needed DallasTemperature in daisy chain can be used, the average values is computed, excluding invalid values. If no valid valud is retieved, just wait for the next period.
* Implementation of the callbacks to request the overproduction power retrieval and handles the reception of the value. My other project is used to retrieve the power value over the network but it can be changed to read it locally.
* Periodic trigger of `onRefresh` of the power_manager.
* Diplays on a HTML page various info and enables the change some settings.
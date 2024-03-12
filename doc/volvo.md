DOUBLE E HOBBY Volvo EC160E Excavator
=====================================

![](/img/volvo1.jpg)


Firmware features
-----------------

+ Hydraulic pump and valve control
+ Dual motor control (tracks)
+ Turret motor control
+ Acceleration ramping
+ Drive mode switching (independent/differential tracks)
+ LED lighting (headlights, tail light)
+ iBUS servo link with FlySky transmitter
+ iBUS telemetry (voltage, temperature)
+ MCU: STM32F103Cx (Blue Pill board)


Rationale
---------

The firmware controls response of the pump depending on input on channels 1, 2 and 3. The main goal is to make the operation smooth and fluid comparing to a simple channel mix in the transmitter. The conventional approach does not take into account the fact that there is some travel distance before a valve begins to open at `VALVE_MIN`. In this model, a valve is not immediately fully open upon reaching that point, but rather upon reaching `VALVE_MAX` which takes some more travel. A better strategy is to keep the pump off until a valve is about to start opening and then give the input reduced weight until the valve is fully open. As a bonus, there is no need to create a channel mix on the transmitter for the pump.

![](/img/pump1.jpg)

Another feature is that the digital servos of the valves are also controlled by the firmware. The main reason for that is servo trimming. Due to their design, the valve servos are not centered mechanically in this model hence they must be trimmed. Consequently, the trimming values need to be further taken into account in the firmware for proper pump control. Since there is no point in keeping the same values in two places, they are stored in the firmware only. It simplifies the model's setup on the transmitter to a great extent. As another bonus, the pump/servo refresh rate is fixed at 250Hz.

The remaining three motors (turret/tracks) are plugged into the board leaving the receiver without any direct connections to servos/ESCs. This makes it possible to implement differential track control (drive mode 1 and 2) via a separate 3-way switch on channel 8.


Pinout
------

| Pin | I/O | Description    |
|-----|-----|----------------|
| A3  | IN  | iBUS servo     |
| A2  | I/O | iBUS sens      |
| A6  | OUT | Bucket valve   |
| A7  | OUT | Boom valve     |
| B0  | OUT | Stick valve    |
| B1  | OUT | Pump           |
| A1  | OUT | Turret         |
| B10 | OUT | Left track     |
| B11 | OUT | Right track    |
| A0  | OUT | Light          |
| A4  | AIN | Temperature    |
| A5  | AIN | Voltage        |
| C13 | OUT | Status LED (*) |
| A9  | OUT | DEBUG          |

(*) active low


Channel mapping
---------------

| # | Drive mode off     | Drive mode 1    | Drive mode 2    |
|---|--------------------|-----------------|-----------------|
| 1 | Bucket             | Turret          | Left/right      |
| 2 | Boom               | Boom            | Forward/reverse |
| 3 | Stick              | Forward/reverse | Stick           |
| 4 | Turret             | Left/right      | Turret          |
| 5 | Left track         | --              | --              |
| 6 | Right track        | --              | --              |
| 7 | Light on/off       |
| 8 | Drive mode 1/2/off |


Installation
------------

```
cmake -B build
cd build
make
make flash-volvo   # Flash using ST-LINK probe
```

It is also possible to flash the firmware without a probe:

* Set the boot jumpers to boot in DFU mode (BOOT0=1, BOOT1=0).
* Connect a USB-TTL adapter (RX to pin A9, TX to pin A10).
* Press the reset button.
* Run the following command with the correct device name:

```
stm32flash -w volvo.bin /dev/ttyUSB0
```

* Disconnect the USB-TTL adapter.
* Return the jumpers to their initial positions (BOOT0=0, BOOT1=0).
* Press the reset button.


Upgrading the model
-------------------

Make sure valve channels 1, 2 and 3 are properly centered via the _Function/Subtrims_ menu on the transmitter. Transfer the values to the corresponding `CHx_TRIM` constants at the beginning of `src/volvo.c`. Don't worry about the values being shown as percentages in the menu. They are actually in microseconds! For example, `Ch1 -50%` yields `CH1_TRIM -50` and so on. You can skip this step and center the channels later by trial and error and/or consulting with debug output.

Grind off the side of the board along pins A0-B11 (12 pins) using a precision file so that one side of a 12x3 header can be soldered to these pins.

![](/img/volvo2.jpg)

The outer row of the header pins must be connected to ground. The middle row must be connected to the 6V rail except for pins A4 and A5 (sensors) that must be connected to the 3.3V rail instead. Make appropriate connections on the back of the board and solder other header connectors if deemed necessary.

![](/img/volvo3.jpg)

I have also upgraded the stock 3-in-1 combo ESC that drives the turret/tracks by installing 3 separate HGLRC 35A ESCs and flashing them with [ESCape32 firmware](https://github.com/neoxic/ESCape32). The drive is now very smooth, elegant and precise comparing to the stock ESC that had a very high minimum throttle point. This optional step is highly recommended!

![](/img/volvo4.jpg)

Fix the board on the pad alongside the receiver with a double-sided tape and make appropriate wire connections. Note that the stock 10-channel FS-iA10B receiver has been replaced with a 6-channel FS-iA6B in my case. Connect the voltage sensor to the main power supply and place the temperature sensor on the transparent low-pressure pipe.

![](/img/volvo5.jpg)

![](/img/volvo6.jpg)

The temperature sensor is a TMP36 sensor. The voltage sensor is a simple voltage divider with R1=100K and R2=10K (you can pick your own values). Measure voltage on the VDD pin along with the exact values of R1 and R2 with a multimeter and update the integer constants `VOLT1` and `VOLT2` at the beginning of `src/volvo.c`. `VOLT1` must be in millivolts whereas `VOLT2` must be constructed by calculating the formula `VOLT1*(R1+R2)/R2` and then converting the value to tens of millivolts.

The LEDs on the schematics denote LED connection points rather than the actual number of diodes. For example, there are three LEDs in this model (two headlights and one tail light).

![](/img/circuit1.png)

Flash the board and give it a run. You should see two telemetry sensors on your transmitter.

![](/img/telemetry1.jpg)

The voltage sensor can be used to set up the battery charge monitor on the transmitter by tapping on the battery indicators, ticking the _"Ext"_ checkbox and updating the _"High/Ala./Low"_ values.

![](/img/telemetry2.jpg)

STM32 Frequency Meter with USB
==============================

This STM32F103-based device uses its **TIM2** timer to calculate the frequency of digital signal feed to its **TIM2_ETR** pin (aka. **PA0**).
Measured frequency are sent through USB CDC and you can view it with terminal utilities such as `screen`, `minicom` or `picocom`.
The code can work unmodified on Maple Mini or its clones (although the bootloader will be erased),
and minimal modifications (LED and USB pull-up pins) are required for other boards.

Features
--------

* Works with the minimal STM32F103C8T6, bootloader not required.
* USB CDC interface allows easy interfacing with PC.
* Resolution down to 1Hz. (Accuracy limited by the crystal oscillator used.)
* 1Hz update rate.
* Configurable clock generator for diagnosis (output on **MCO** pin, aka. **PA8**).
* Configurable digital filter.
* Holding support.

Build and Flash
---------------

Make sure you have `git` and the [`gcc-arm-embedded`](https://launchpad.net/gcc-arm-embedded) toolchain.
You will also need [`stm32flash`](https://code.google.com/p/stm32flash/).
You may also want to modify **Makefile** to specify the toolchain prefix and ISP serial port on your system.

For the hardware, pull **BOOT1** or **PB2** down.
Connect the UART serial cable to **USART1**'s **TX** and **RX** pin.
Pull down **BOOT0** (press and hold **BUT** on Maple Mini, or the **BOOT0** button on some boards)
and then power or reset the board.

Once ready, type the following commands:


```
git clone https://github.com/dword1511/stm32-freqmeter.git
cd stm32-freqmeter
make
make flash
```

After flash is finished, you should be able to see the LED blinking, on for one second and off for one second.
Connect the board to PC with USB, and you should be able to see a USB CDC serial port (`/dev/ttyACM0` for example).
Then, type `screen /dev/ttyACM0` or `minicom -D /dev/ttyACM0` or `picocom /dev/ttyACM0` to start using.

Output and Usage
----------------

Typical terminal output will look like the following after being powered up:


```
   0.000060 MHz . [Hold: OFF]

Clock output:       OFF
Digital Filter:        OFF
Prescaler: OFF
```

There are 3 parts in the first line:

* Frequency in MHz unit, with resolution down to 1Hz. If long wire is attached to input pin, 50/60Hz power-line interference might be shown.
* An activity-indicating dot that blinks in-sync with the LED on board.
* Holding indicator.

The following lines show information about:
* Diagnostic clock output configuration.
* Digital filter configuration.
* Prescaler configuration.

To switch between different diagnostic clock outputs, press `o` in the terminal.
The output will then change to something like:


```
   8.015324 MHz . [Hold: OFF]

Clock output:  8 MHz RC
Digital Filter:        OFF
Prescaler: OFF
```

Following clock configurations are available:

* Off.
* 8MHz internal RC oscillator (2.5% maximum error and not stable).
* 8MHz external crystal oscillator.
* 36MHz clock from system clock divided by 2.

To hold current measurement, press `h`. Press again to cancel holding:


```
   8.014395 MHz . [Hold: ON]

Clock output:  8 MHz RC
Digital Filter:        OFF
Prescaler: OFF
```

To cycle through digital filter configurations, press `f`.
The digital filter will suppress frequencies that is over one-half of the displayed value:


```
   8.014395 MHz . [Hold: OFF]

Clock output:  8 MHz RC
Digital Filter:  9.000 MHz
Prescaler: OFF
```

Following filter configurations are available:

* Off.
* 36MHz
* 18MHz
* 9MHz
* 6MHz
* 4.5MHz
* 3MHz
* 2.25MHz
* 1.5MHz
* 1.125MHz
* 900kHz
* 750kHz
* 562.5kHz
* 450kHz
* 375kHz
* 281.25kHz

To cycle through prescaler configurations, press `p`.
The prescaler will scale down the input signal so higher frequencies
can be measured as well (while sacrificing some precision):

```
   8.045592 MHz   [Hold: OFF]

Clock output:  8 MHz RC
Digital Filter:        OFF
Pre-scaler:   2
```

The following prescaler configurations are available:

* Off.
* 2
* 4
* 8

Add-ons
-------

Please see the "addons" folder for details.

Known Issues
------------

* Currently the code drops 20 ticks out of 36,000,000 ticks (<0.6ppm error).
  However, before we use TCXO to supply clock to the MCU, fixing it (by multiplying 1.00000056)
  will not improve precision notably.

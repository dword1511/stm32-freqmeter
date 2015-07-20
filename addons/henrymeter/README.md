Henrymeter Frontend for STM32 Frequency Meter
=============================================

This simple circuitry and program allows you to measure inductors with the frequency meter.

Building the Hardware
---------------------

Generally speaking, you can use a Pierce or Colpitts to measure the inductance.
However, this only works for inductors with high Q value,
while unfortunately most small-sized inductors in reality have low Q values.
As a result, a circuit based on LM393 comparator is used.

The first stage forms an oscillator that is not very sensitive to the Q value of the inductor,
and the second stage is a Schmitt trigger, whose hysteresis can be adjusted by resistor R9.
C1 is the reference capacitor and must be a 5% (preferably 1%) film capacitor.
C2 is a decoupling capacitor, whose capacitance must be much larger than C1.
It allows the LC network to be biased at mid-rail level.
C3 and R3 forms a low-pass filter that tracks the output DC level,
and will cancel it by feeding it back to inverting input of the comparator.
R4 provides positive feedback.
R5 and R6 provides pull-ups (since LM393s have open-collector output stages).
R1, R2, R7 and R8 provide mid-rail levels.
C4 is a bypass capacitor for the power supply.

The oscillator may not operate with inductors with very small inductance.
To solve the problem, you can put a inductor of a few hundreds of uH in series with the DUT.
The PCB has a 4-pin socket. 2 pins are reserved for putting the inductor.
The other 2 pins are for connecting test leads.

Once built, you may want to calibrate the reference capacitor.
First short out test leads to find the offset.
Then use a few inductor with known inductance to calculate the error in the reference capacitance.

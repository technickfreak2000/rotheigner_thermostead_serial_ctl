This is a custom firmware for the "Rotheigner Thermostatkopf Calor". It enables you to control multiple over serial - bus like. 

Hornbach: [Rotheigner Thermostatkopf Calor](https://www.hornbach.de/p/rotheigner-thermostatkopf-calor-m30-x-1-5-weiss-70002001/5645331/)

Display, buttons and the temperature sensor are not supported! 
I use this in combination with an esp32 and esphome to control my underfloor heating. The valves are all located in my basement, so i don't need those things! Cheper then the wireless controlled ones, no batteries to worrie about and even more reliable if you connect everything with a wire! 

This project is not complete and has some bugs - dev version!

# How to compile and flash? 
Clone the repo, cd to SDCC and execute _UX_compile_run.sh
I never used the windows one.
And the latest sdcc release currently has a bug, it works with this: sdcc-4.4.0-1-x86_64.pkg

# Which IO to use for flashing and 
Just look at the datasheet [ST Site](https://www.st.com/en/microcontrollers-microprocessors/stm8l052c6.html)

I used a ST-Link V2 and a standard ch340 at 3.3V.

Quickly trace the pins with a multimeter. Pads can be used for the st link, but they don't have the currently used serial pins exposed. I soldered rx and tx to resistors (i think one was the original tmp sensor at the top) on the back side of the pcb. I'll try to use more accessible pins in the future.

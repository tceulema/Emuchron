## Project Emuchron
### What is Emuchron
Emuchron is a lightweight [Adafruit Monochron](http://www.adafruit.com/product/204)
(product discontinued) emulator for Debian Linux 12 and 13.\
It features a test and debugging platform for Monochron clocks and high-level
glcd graphics functions, and a software framework for clock plugins.

### Why does Emuchron exist
Coding clocks for the Monochron open source clock is (debatable) fun, but has
its drawbacks. Clocks sometimes seem to hang, the graphics turn out not to be
fluid or are simply incorrect.\
Up to now the only way to debug a clock and graphics functionality is to upload
firmware to Monochron that generates debug output strings, and have these
strings sent back via the Monochron FTDI bus to a terminal application on the
connected computer. Although this debug method is still very useful, it is
considered cumbersome and inflexible.

Enter Emuchron.

The main feature of Emuchron is to emulate the Monochron hardware and keep the
emulator stubs as far away as possible from functional clock code and
high-level graphics functions. This allows a programmer to code, debug and test
clocks and graphics functions in a controlled Debian Linux environment ahead of
uploading firmware to Monochron. Emuchron is controlled via a command line tool
*mchron* dedicated to supporting these development and test features.

### What can Emuchron emulator tool *mchron* do for you
*mchron* allows accessing clock plugins at will, feed clocks with a continuous
stream of time and keyboard events, change the time/date/alarm, access the
graphics library to draw on a stubbed LCD display (OpenGL2/GLUT and/or
ncurses), execute command script files, and run a stubbed Monochron application
ahead of building and uploading the actual firmware. In combination with the
standard gdb debugger and a gdb front-end gui this is a powerful means to test
specific functionality and find and solve bugs.

The *mchron* interpreter supports named variables representing numeric values,
repeat, if-then-else and return logic constructs, and basic mathematical
expression evaluation for numeric command arguments.

### What comes extra with Emuchron
Compared to the original Monochron [firmware](https://github.com/adafruit/monochron),
Emuchron includes functional enhancements and draw speed optimizations in the
high-level glcd graphics library, modified clock configuration pages, many
example clocks, a graphics performance test module, a two-tone and Mario melody
alarm, and demo and test scripts.

### Where can I find more information on Emuchron
For detailed information refer to the 100+ page installation, configuration and
end-user manual "Emuchron_Manual.pdf" in project folder
[support](https://github.com/tceulema/Emuchron/tree/master/support).

### Acknowledgements
- CaitSith2 and ladyada\
The Emuchron project started with the original Monochron pong clock
[firmware](https://github.com/adafruit/monochron).
- Balza3\
The Mario alarm in Emuchron is based on notes, beats and play logic provided in
an [Arduino project](http://www.youtube.com/watch?v=VqeYvJpibLY).
- Tz / HarleyHacking\
The core functionality to encode a QR in the Emuchron QR clocks uses code from
[qrduino](https://github.com/tz1/qrduino).
- Techninja (James T) and Super-Awesome Sylvia\
The Emuchron MarioWorld clock is based on code from
[MarioChron](https://github.com/techninja/MarioChron).
- Jamie Zawinski and CaitSith2\
The Emuchron Dali clock is based on [xdaliclock](https://www.jwz.org/xdaliclock)
code that is later integrated in Monochron
[Multichron](https://github.com/CaitSith2/monochron/tree/MultiChron/firmware) code.

### Using and distributing Emuchron
The Emuchron project and its contents is provided as-is and is distributed
under [GNU Public License v3](http://www.gnu.org/licenses/gpl.txt).

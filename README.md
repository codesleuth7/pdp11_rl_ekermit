PDP-11 RL01/RL02 bare metal ekermit server. This project was created as a way to backup PDP-11 RL01/RL02 disks using minimal hardware without an operating system. If you have a bootable PDP-11 with other storage peripherals then faster options to backup RL disks may work better then this. Using the Kermit protocol to backup megabytes of data is slow, especially with some console serial port speed limitations.

The E-Kermit (Embedded Kermit) package from Columbia University and authored by Frank da Cruz is the foundation of this code. All the PDP-11 hardware support software was added to run as a bare metal program. The result of this build is the "ek" binary and the matching ek.ptap (paper tape) version. The SIMH PDP-11 simulator v4.0 was used for testing and a "pdp11.ini" example file will load the ek.ptap file. The rawRL02.dsk file was used for testing and is included. 

Ek will require a console serial port, RL disk controller, enough memory for the ek program to fit, and also expects a working 50/60hz clock. Each device is expected at the default address and vector.

Develment environment:

This code was compiled using GCC version 9.3.0 configured for the PDP-11.

The included makefile depends on the pdp11-aout-* utilities found in your PATH.

More information on the GCC v9.3.0 compiler for PDP-11 can be found here:

https://www.teckelworks.com/2020/03/c-programming-on-a-bare-metal-pdp-11/

SIMH (v4.0 or newer is required) can be found here:

https://github.com/simh/simh


